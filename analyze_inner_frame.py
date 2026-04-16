from __future__ import annotations

import argparse
import json
import math
import statistics
import struct
from collections import Counter
from dataclasses import asdict, dataclass
from pathlib import Path

from PIL import Image, ImageDraw, ImageOps

from decode_stylus_capture import ReassembledFrame, reassemble_frames


INNER_HEADER_BYTES = 80
BLOCK_BYTES = 16


@dataclass(frozen=True)
class LayoutCandidate:
    mode: str
    score: float
    width: int
    height: int
    block_width: int
    block_height: int
    grid_width: int
    grid_height: int
    transpose_block: bool


def payload_to_u16(payload: bytes) -> list[int]:
    return list(struct.unpack("<" + "H" * (len(payload) // 2), payload))


def continuity_score(values: list[int], width: int, height: int) -> float:
    total = 0.0
    count = 0

    for row_index in range(height):
        row_offset = row_index * width
        for column_index in range(width - 1):
            total += abs(values[row_offset + column_index] - values[row_offset + column_index + 1])
            count += 1

    for row_index in range(height - 1):
        row_offset = row_index * width
        next_row_offset = (row_index + 1) * width
        for column_index in range(width):
            total += abs(values[row_offset + column_index] - values[next_row_offset + column_index])
            count += 1

    return total / max(1, count)


def factor_pairs(number: int) -> list[tuple[int, int]]:
    pairs = []
    for candidate in range(1, int(math.isqrt(number)) + 1):
        if number % candidate == 0:
            pairs.append((candidate, number // candidate))
    return pairs


def render_block_layout(
    values: list[int],
    values_per_block: int,
    block_width: int,
    block_height: int,
    grid_width: int,
    transpose_block: bool,
) -> tuple[list[int], int, int]:
    if len(values) % values_per_block != 0:
        raise ValueError("values length is not aligned to the block size")

    blocks = [values[index:index + values_per_block] for index in range(0, len(values), values_per_block)]
    grid_height = len(blocks) // grid_width
    output_block_width = block_height if transpose_block else block_width
    output_block_height = block_width if transpose_block else block_height
    image_width = grid_width * output_block_width
    image_height = grid_height * output_block_height
    rendered = [0] * (image_width * image_height)

    for block_index, block in enumerate(blocks):
        tile_x = block_index % grid_width
        tile_y = block_index // grid_width
        for value_index, value in enumerate(block):
            pixel_x = value_index % block_width
            pixel_y = value_index // block_width
            if transpose_block:
                pixel_x, pixel_y = pixel_y, pixel_x
            output_x = tile_x * output_block_width + pixel_x
            output_y = tile_y * output_block_height + pixel_y
            rendered[output_y * image_width + output_x] = value

    return rendered, image_width, image_height


def search_layouts(
    values: list[int],
    values_per_block: int,
    block_shapes: list[tuple[int, int]],
    mode: str,
    top_n: int = 6,
) -> list[LayoutCandidate]:
    block_count = len(values) // values_per_block
    candidates: list[LayoutCandidate] = []

    for block_width, block_height in block_shapes:
        for first_factor, second_factor in factor_pairs(block_count):
            for grid_width, grid_height in ((first_factor, second_factor), (second_factor, first_factor)):
                transpose_options = [False, True] if block_width != block_height else [False]
                for transpose_block in transpose_options:
                    rendered, image_width, image_height = render_block_layout(
                        values,
                        values_per_block,
                        block_width,
                        block_height,
                        grid_width,
                        transpose_block,
                    )
                    aspect_ratio = image_width / image_height
                    if aspect_ratio < 0.4 or aspect_ratio > 2.5:
                        continue
                    candidates.append(
                        LayoutCandidate(
                            mode=mode,
                            score=round(continuity_score(rendered, image_width, image_height), 2),
                            width=image_width,
                            height=image_height,
                            block_width=block_width,
                            block_height=block_height,
                            grid_width=grid_width,
                            grid_height=grid_height,
                            transpose_block=transpose_block,
                        )
                    )

    unique = {}
    for candidate in sorted(candidates, key=lambda item: item.score):
        key = (
            candidate.mode,
            candidate.width,
            candidate.height,
            candidate.block_width,
            candidate.block_height,
            candidate.grid_width,
            candidate.grid_height,
            candidate.transpose_block,
        )
        unique.setdefault(key, candidate)
    return list(unique.values())[:top_n]


def normalize_to_u8(values: list[int]) -> bytes:
    filtered = [value for value in values if value not in (0, 0xFFFF)]
    if not filtered:
        return bytes([0] * len(values))

    low = min(filtered)
    high = max(filtered)
    scale = max(1, high - low)
    output = bytearray()
    for value in values:
        clamped = min(max(value, low), high)
        output.append(int((clamped - low) * 255 / scale))
    return bytes(output)


def save_layout_preview(values: list[int], candidate: LayoutCandidate, values_per_block: int, output_path: Path) -> None:
    rendered, image_width, image_height = render_block_layout(
        values,
        values_per_block,
        candidate.block_width,
        candidate.block_height,
        candidate.grid_width,
        candidate.transpose_block,
    )
    image = Image.frombytes("L", (image_width, image_height), normalize_to_u8(rendered))
    image = ImageOps.autocontrast(image)
    image.save(output_path)


def create_contact_sheet(preview_files: list[tuple[str, Path]], output_path: Path) -> None:
    if not preview_files:
        return

    opened = [(label, Image.open(path).convert("L")) for label, path in preview_files]
    tile_width = max(image.width for _, image in opened)
    tile_height = max(image.height for _, image in opened)
    label_height = 28
    columns = 2
    rows = math.ceil(len(opened) / columns)
    canvas = Image.new("L", (columns * tile_width, rows * (tile_height + label_height)), color=0)
    draw = ImageDraw.Draw(canvas)

    for index, (label, image) in enumerate(opened):
        column = index % columns
        row = index // columns
        x_offset = column * tile_width
        y_offset = row * (tile_height + label_height)
        canvas.paste(image, (x_offset, y_offset))
        draw.text((x_offset + 4, y_offset + tile_height + 4), label, fill=255)

    canvas.save(output_path)


def parse_inner_header(payload: bytes) -> dict[str, object]:
    head = payload[:INNER_HEADER_BYTES]
    return {
        "header_bytes": INNER_HEADER_BYTES,
        "u32_words": list(struct.unpack("<20I", head)),
        "u16_words": list(struct.unpack("<40H", head)),
        "hex": head.hex(),
    }


def block_variance_summary(frames: list[ReassembledFrame], stream_tag: int, frame_size: int, sample_limit: int = 120) -> dict[str, object]:
    matched = [frame for frame in frames if frame.stream_tag == stream_tag and frame.complete and len(frame.payload) == frame_size][:sample_limit]
    if not matched:
        return {}

    block_count = frame_size // BLOCK_BYTES
    frame_blocks = [[frame.payload[offset:offset + BLOCK_BYTES] for offset in range(0, frame_size, BLOCK_BYTES)] for frame in matched]
    summary = []
    for block_index in range(block_count):
        columns = list(zip(*(blocks[block_index] for blocks in frame_blocks)))
        mean_variance = sum(statistics.pvariance(values) for values in columns) / BLOCK_BYTES
        unique_count = len({blocks[block_index] for blocks in frame_blocks[:20]})
        summary.append(
            {
                "block_index": block_index,
                "mean_byte_variance": round(mean_variance, 2),
                "unique_values_in_first_20_frames": unique_count,
                "sample_hex": frame_blocks[0][block_index].hex(),
            }
        )

    return {
        "sample_frame_count": len(matched),
        "block_size": BLOCK_BYTES,
        "block_count": block_count,
        "lowest_variance_blocks": sorted(summary, key=lambda item: item["mean_byte_variance"])[:24],
        "highest_variance_blocks": sorted(summary, key=lambda item: item["mean_byte_variance"], reverse=True)[:24],
        "leading_blocks": summary[:32],
    }


def export_stream_analysis(frames: list[ReassembledFrame], stream_tag: int, output_dir: Path) -> dict[str, object]:
    stream_frames = [frame for frame in frames if frame.stream_tag == stream_tag and frame.complete]
    frame_sizes = Counter(len(frame.payload) for frame in stream_frames)
    modal_frame_size, modal_frame_count = frame_sizes.most_common(1)[0]
    sample_frame = next(frame for frame in stream_frames if len(frame.payload) == modal_frame_size)

    output_dir.mkdir(parents=True, exist_ok=True)

    byte_values = list(sample_frame.payload)
    word_values = payload_to_u16(sample_frame.payload)
    byte_candidates = search_layouts(byte_values, 16, [(1, 16), (2, 8), (4, 4), (8, 2), (16, 1)], "byte")
    word_candidates = search_layouts(word_values, 8, [(1, 8), (2, 4), (4, 2), (8, 1)], "u16")
    selected_candidates = byte_candidates[:4] + word_candidates[:4]

    preview_files = []
    for candidate in selected_candidates:
        file_name = (
            f"stream_{stream_tag:04x}_{candidate.mode}_{candidate.width}x{candidate.height}"
            f"_bw{candidate.block_width}x{candidate.block_height}_gw{candidate.grid_width}"
            f"_{'t' if candidate.transpose_block else 'n'}.png"
        )
        output_path = output_dir / file_name
        source_values = byte_values if candidate.mode == "byte" else word_values
        values_per_block = 16 if candidate.mode == "byte" else 8
        save_layout_preview(source_values, candidate, values_per_block, output_path)
        preview_files.append((file_name, output_path))

    contact_sheet_path = output_dir / f"stream_{stream_tag:04x}_contact_sheet.png"
    create_contact_sheet(preview_files, contact_sheet_path)

    result = {
        "stream_tag": stream_tag,
        "complete_frame_count": len(stream_frames),
        "frame_size_distribution": [{"frame_size": size, "count": count} for size, count in frame_sizes.most_common(12)],
        "modal_frame_size": modal_frame_size,
        "modal_frame_count": modal_frame_count,
        "sample_frame_token": sample_frame.frame_token,
        "inner_header": parse_inner_header(sample_frame.payload),
        "block_variance": block_variance_summary(frames, stream_tag, modal_frame_size),
        "top_byte_layouts": [asdict(candidate) for candidate in byte_candidates],
        "top_u16_layouts": [asdict(candidate) for candidate in word_candidates],
        "contact_sheet": contact_sheet_path.name,
    }
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description="Analyze the inner frame structure after UDP reassembly.")
    parser.add_argument("pcap", nargs="?", default="stylus.pcapng", help="Path to the pcapng file")
    parser.add_argument("--output-dir", default="inner_frame_analysis", help="Directory for analysis output")
    args = parser.parse_args()

    frames = reassemble_frames(Path(args.pcap))
    stream_tags = sorted({frame.stream_tag for frame in frames if frame.complete})
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    result = {
        "file": args.pcap,
        "streams": [],
    }

    for stream_tag in stream_tags:
        stream_output_dir = output_dir / f"stream_{stream_tag:04x}"
        result["streams"].append(export_stream_analysis(frames, stream_tag, stream_output_dir))

    summary_path = output_dir / "summary.json"
    summary_path.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()