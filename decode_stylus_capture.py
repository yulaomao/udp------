from __future__ import annotations

import argparse
import json
from collections import defaultdict
from dataclasses import asdict, dataclass
from pathlib import Path

from PIL import Image, ImageOps
from scapy.all import IP, IPv6, Raw, UDP, PcapNgReader  # type: ignore

from analyze_capture import parse_vendor_header


@dataclass
class ReassembledFrame:
    stream_tag: int
    frame_token: int
    frame_size: int
    packet_count: int
    complete: bool
    payload: bytes


def get_ip_layer(packet):
    if IP in packet:
        return packet[IP]
    if IPv6 in packet:
        return packet[IPv6]
    return None


def reassemble_frames(path: Path) -> list[ReassembledFrame]:
    grouped_headers = defaultdict(list)
    grouped_payloads = defaultdict(list)

    with PcapNgReader(str(path)) as reader:
        for packet in reader:
            if UDP not in packet or Raw not in packet:
                continue
            ip_layer = get_ip_layer(packet)
            if ip_layer is None:
                continue

            payload = bytes(packet[Raw].load)
            header = parse_vendor_header(payload)
            if header is None:
                continue

            frame_key = (header.stream_tag, header.frame_token)
            grouped_headers[frame_key].append(header)
            grouped_payloads[frame_key].append(payload[24:])

    frames: list[ReassembledFrame] = []
    for frame_key, headers in grouped_headers.items():
        ordered = sorted(zip(headers, grouped_payloads[frame_key]), key=lambda item: item[0].payload_offset)
        frame_size = ordered[0][0].frame_size
        payload = bytearray(frame_size)
        complete = True
        expected_offset = 0

        for header, chunk in ordered:
            if header.payload_offset != expected_offset:
                complete = False
            payload_end = header.payload_offset + len(chunk)
            if payload_end > frame_size:
                complete = False
                break
            payload[header.payload_offset:payload_end] = chunk
            expected_offset = payload_end

        if expected_offset != frame_size:
            complete = False

        frames.append(
            ReassembledFrame(
                stream_tag=frame_key[0],
                frame_token=frame_key[1],
                frame_size=frame_size,
                packet_count=len(ordered),
                complete=complete,
                payload=bytes(payload),
            )
        )

    return sorted(frames, key=lambda item: (item.stream_tag, item.frame_token))


def candidate_dimensions(payload_size: int) -> list[tuple[int, int]]:
    candidates = []
    for width in range(16, min(payload_size, 1024) + 1):
        if payload_size % width != 0:
            continue
        height = payload_size // width
        if height < 16:
            continue
        candidates.append((width, height))
    return candidates


def row_continuity_score(payload: bytes, width: int, height: int) -> float:
    score = 0
    rows = [payload[index * width:(index + 1) * width] for index in range(height)]
    for first, second in zip(rows, rows[1:]):
        score += sum(abs(left - right) for left, right in zip(first, second))
    return score / max(1, height - 1)


def infer_top_dimensions(payload: bytes, top_n: int = 5) -> list[dict[str, float | int]]:
    scored = []
    for width, height in candidate_dimensions(len(payload)):
        aspect = width / height
        if aspect < 0.5 or aspect > 2.5:
            continue
        score = row_continuity_score(payload, width, height)
        scored.append({"width": width, "height": height, "score": round(score, 2)})
    return sorted(scored, key=lambda item: item["score"])[:top_n]


def save_preview_png(payload: bytes, width: int, height: int, output_path: Path) -> None:
    image = Image.frombytes("L", (width, height), payload)
    image = ImageOps.autocontrast(image)
    image.save(output_path)


def export_frames(frames: list[ReassembledFrame], output_dir: Path, extract_limit: int) -> dict[str, object]:
    output_dir.mkdir(parents=True, exist_ok=True)
    metadata = {"streams": []}

    grouped = defaultdict(list)
    for frame in frames:
        grouped[frame.stream_tag].append(frame)

    for stream_tag, stream_frames in sorted(grouped.items()):
        stream_dir = output_dir / f"stream_{stream_tag:04x}"
        stream_dir.mkdir(parents=True, exist_ok=True)
        complete_frames = [frame for frame in stream_frames if frame.complete]

        stream_meta = {
            "stream_tag": stream_tag,
            "frame_count": len(stream_frames),
            "complete_frame_count": len(complete_frames),
            "exported_frames": [],
        }

        for frame in complete_frames[:extract_limit]:
            frame_base = stream_dir / f"frame_{frame.frame_token}"
            raw_path = frame_base.with_suffix(".raw")
            raw_path.write_bytes(frame.payload)

            guesses = infer_top_dimensions(frame.payload)
            preview_paths = []
            for guess in guesses[:2]:
                png_path = stream_dir / (
                    f"frame_{frame.frame_token}_{guess['width']}x{guess['height']}.png"
                )
                save_preview_png(frame.payload, int(guess["width"]), int(guess["height"]), png_path)
                preview_paths.append(str(png_path.name))

            frame_meta = asdict(frame)
            frame_meta.pop("payload")
            frame_meta["dimension_guesses"] = guesses
            frame_meta["raw_file"] = raw_path.name
            frame_meta["preview_files"] = preview_paths
            stream_meta["exported_frames"].append(frame_meta)

        metadata["streams"].append(stream_meta)

    return metadata


def main() -> None:
    parser = argparse.ArgumentParser(description="Reassemble and preview vendor UDP frames from stylus.pcapng")
    parser.add_argument("pcap", nargs="?", default="stylus.pcapng", help="Path to the pcapng file")
    parser.add_argument("--output-dir", default="decoded_output", help="Directory for extracted frames")
    parser.add_argument("--extract-limit", type=int, default=3, help="Number of complete frames to export per stream")
    args = parser.parse_args()

    frames = reassemble_frames(Path(args.pcap))
    metadata = export_frames(frames, Path(args.output_dir), args.extract_limit)
    metadata["total_frames"] = len(frames)
    metadata_path = Path(args.output_dir) / "metadata.json"
    metadata_path.write_text(json.dumps(metadata, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(metadata, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()