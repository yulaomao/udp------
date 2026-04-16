from __future__ import annotations

import argparse
import json
import math
import statistics
import struct
from collections import Counter, defaultdict
from dataclasses import asdict, dataclass
from pathlib import Path

from PIL import Image, ImageOps

from decode_stylus_capture import ReassembledFrame, reassemble_frames


INNER_HEADER_BYTES = 80
RECORD_BYTES = 16
MASK_THRESHOLD = 32
VERTICAL_GAP = 2
HORIZONTAL_GAP = 2
MIN_COMPONENT_AREA = 20
MAX_TRACK_GAP = 3


@dataclass(frozen=True)
class RoiSegment:
    x: int
    pixels: list[int]
    source: str


@dataclass(frozen=True)
class RoiRow:
    index: int
    segments: list[RoiSegment]


@dataclass(frozen=True)
class FrameArtifacts:
    frame: ReassembledFrame
    rows: list[RoiRow]
    image: Image.Image
    processed_mask: list[list[int]]
    components: list[dict[str, object]]
    inner_header_u32: list[int]


def trim_record(record: bytes) -> bytes:
    start = 0
    end = len(record)
    while start < end and record[start] in (0x80, 0x00):
        start += 1
    while end > start and record[end - 1] in (0x80, 0x00):
        end -= 1
    return record[start:end]


def parse_roi_rows(payload: bytes) -> list[RoiRow]:
    body = payload[INNER_HEADER_BYTES:]
    rows: list[RoiRow] = []
    current_segments: list[RoiSegment] = []

    for index in range(0, len(body), RECORD_BYTES):
        record = body[index:index + RECORD_BYTES]
        trimmed = trim_record(record)
        if not trimmed:
            continue

        if trimmed[0] < 0x80:
            if current_segments:
                rows.append(RoiRow(index=len(rows), segments=current_segments))
            current_segments = [
                RoiSegment(x=trimmed[0], pixels=list(trimmed[1:]), source="start")
            ]
            continue

        if trimmed[-1] < 0x80:
            if not current_segments:
                current_segments = []
            current_segments.append(RoiSegment(x=trimmed[-1], pixels=list(trimmed[:-1]), source="tail"))
            continue

        if current_segments:
            last_segment = current_segments[-1]
            current_segments[-1] = RoiSegment(
                x=last_segment.x,
                pixels=last_segment.pixels + list(trimmed),
                source="cont",
            )
        else:
            current_segments = [RoiSegment(x=0, pixels=list(trimmed), source="cont")]

    if current_segments:
        rows.append(RoiRow(index=len(rows), segments=current_segments))

    return rows


def render_rows(rows: list[RoiRow], width: int, height: int) -> Image.Image:
    canvas = bytearray(width * height)
    for y, row in enumerate(rows[:height]):
        for segment in row.segments:
            for offset, value in enumerate(segment.pixels):
                x = segment.x + offset
                if 0 <= x < width:
                    canvas[y * width + x] = max(canvas[y * width + x], value)
    image = Image.frombytes("L", (width, height), bytes(canvas))
    return ImageOps.autocontrast(image)


def parse_inner_header_u32(payload: bytes) -> list[int]:
    return list(struct.unpack("<20I", payload[:INNER_HEADER_BYTES]))


def image_to_mask(image: Image.Image, threshold: int = MASK_THRESHOLD) -> list[list[int]]:
    width, height = image.size
    pixels = image.load()
    return [[1 if pixels[x, y] > threshold else 0 for x in range(width)] for y in range(height)]


def fill_vertical_gaps(mask: list[list[int]], max_gap: int = VERTICAL_GAP) -> list[list[int]]:
    height = len(mask)
    width = len(mask[0]) if height else 0
    output = [row[:] for row in mask]

    for x in range(width):
        active_rows = [y for y in range(height) if mask[y][x]]
        for first_row, second_row in zip(active_rows, active_rows[1:]):
            if 1 < second_row - first_row <= max_gap + 1:
                for y in range(first_row + 1, second_row):
                    output[y][x] = 1

    return output


def fill_horizontal_gaps(mask: list[list[int]], max_gap: int = HORIZONTAL_GAP) -> list[list[int]]:
    height = len(mask)
    width = len(mask[0]) if height else 0
    output = [row[:] for row in mask]

    for y in range(height):
        active_columns = [x for x in range(width) if mask[y][x]]
        for first_column, second_column in zip(active_columns, active_columns[1:]):
            if 1 < second_column - first_column <= max_gap + 1:
                for x in range(first_column + 1, second_column):
                    output[y][x] = 1

    return output


def process_mask(image: Image.Image) -> list[list[int]]:
    mask = image_to_mask(image)
    mask = fill_vertical_gaps(mask)
    mask = fill_horizontal_gaps(mask)
    return mask


def mask_to_image(mask: list[list[int]]) -> Image.Image:
    height = len(mask)
    width = len(mask[0]) if height else 0
    data = bytearray(255 if mask[y][x] else 0 for y in range(height) for x in range(width))
    return Image.frombytes("L", (width, height), bytes(data))


def is_component_candidate(area: int, width: int, height: int) -> bool:
    if area < MIN_COMPONENT_AREA:
        return False

    shortest_side = max(1, min(width, height))
    longest_side = max(width, height)
    aspect_ratio = longest_side / shortest_side

    if shortest_side <= 2 and area < 200:
        return False
    if aspect_ratio > 2.8 and area < 90:
        return False
    return True


def detect_components_from_mask(mask: list[list[int]], min_area: int = MIN_COMPONENT_AREA) -> list[dict[str, object]]:
    height = len(mask)
    width = len(mask[0]) if height else 0
    visited = set()
    components = []

    for y in range(height):
        for x in range(width):
            if not mask[y][x] or (x, y) in visited:
                continue

            stack = [(x, y)]
            visited.add((x, y))
            points = []
            min_x = max_x = x
            min_y = max_y = y

            while stack:
                px, py = stack.pop()
                points.append((px, py))
                min_x = min(min_x, px)
                max_x = max(max_x, px)
                min_y = min(min_y, py)
                max_y = max(max_y, py)

                for dy in (-1, 0, 1):
                    for dx in (-1, 0, 1):
                        if dx == 0 and dy == 0:
                            continue
                        nx = px + dx
                        ny = py + dy
                        if not (0 <= nx < width and 0 <= ny < height):
                            continue
                        if not mask[ny][nx] or (nx, ny) in visited:
                            continue
                        visited.add((nx, ny))
                        stack.append((nx, ny))

            area = len(points)
            if area < min_area:
                continue

            component_width = max_x - min_x + 1
            component_height = max_y - min_y + 1
            if not is_component_candidate(area, component_width, component_height):
                continue

            centroid_x = round(sum(point_x for point_x, _ in points) / area, 2)
            centroid_y = round(sum(point_y for _, point_y in points) / area, 2)
            components.append(
                {
                    "bbox": {"x": min_x, "y": min_y, "width": component_width, "height": component_height},
                    "area": area,
                    "centroid": {"x": centroid_x, "y": centroid_y},
                }
            )

    return sorted(components, key=lambda item: item["area"], reverse=True)


def infer_stream_canvas(stream_frames: list[ReassembledFrame], sample_limit: int = 150) -> tuple[int, int]:
    rows_per_frame = [parse_roi_rows(frame.payload) for frame in stream_frames[:sample_limit]]
    widths = [max(segment.x + len(segment.pixels) for row in rows for segment in row.segments) for rows in rows_per_frame if rows]
    heights = [len(rows) for rows in rows_per_frame if rows]
    width = statistics.multimode(widths)[0] if widths else 0
    height = statistics.multimode(heights)[0] if heights else 0
    return width, height


def build_frame_artifacts(frame: ReassembledFrame, width: int, height: int) -> FrameArtifacts:
    rows = parse_roi_rows(frame.payload)
    image = render_rows(rows, width, height)
    processed_mask = process_mask(image)
    components = detect_components_from_mask(processed_mask)
    return FrameArtifacts(
        frame=frame,
        rows=rows,
        image=image,
        processed_mask=processed_mask,
        components=components,
        inner_header_u32=parse_inner_header_u32(frame.payload),
    )


def summarize_header_fields(stream_frames: list[ReassembledFrame], sample_limit: int = 200) -> dict[str, object]:
    headers = [parse_inner_header_u32(frame.payload) for frame in stream_frames[:sample_limit]]
    fields = []
    for field_index in range(20):
        values = [header[field_index] for header in headers]
        unique_values = sorted(set(values))
        field_info = {
            "field_index": field_index,
            "unique_count": len(unique_values),
            "min": min(values),
            "max": max(values),
        }
        if len(unique_values) <= 6:
            field_info["values"] = unique_values
        fields.append(field_info)
    return {"sample_count": len(headers), "fields": fields}


def track_components(frame_artifacts: list[FrameArtifacts], max_gap: int = MAX_TRACK_GAP) -> list[dict[str, object]]:
    tracks: dict[int, dict[str, object]] = {}
    active_track_ids: list[int] = []
    next_track_id = 1

    for artifacts in frame_artifacts:
        frame_token = artifacts.frame.frame_token
        detections = artifacts.components
        candidates = []

        for track_id in list(active_track_ids):
            track = tracks[track_id]
            if frame_token - int(track["last_frame_token"]) > max_gap:
                active_track_ids.remove(track_id)
                continue

            last_detection = track["detections"][-1]
            last_x = float(last_detection["centroid"]["x"])
            last_y = float(last_detection["centroid"]["y"])
            last_area = float(last_detection["area"])
            for detection_index, detection in enumerate(detections):
                dx = float(detection["centroid"]["x"]) - last_x
                dy = float(detection["centroid"]["y"]) - last_y
                distance = math.hypot(dx, dy)
                area_ratio = max(float(detection["area"]), last_area) / max(1.0, min(float(detection["area"]), last_area))
                max_distance = 18.0 + 0.35 * max(
                    float(detection["bbox"]["width"]),
                    float(detection["bbox"]["height"]),
                    float(last_detection["bbox"]["width"]),
                    float(last_detection["bbox"]["height"]),
                )
                if distance <= max_distance and area_ratio <= 4.0:
                    candidates.append((distance, area_ratio, track_id, detection_index))

        assigned_tracks = set()
        assigned_detections = set()
        for _, _, track_id, detection_index in sorted(candidates):
            if track_id in assigned_tracks or detection_index in assigned_detections:
                continue
            detection = detections[detection_index]
            track = tracks[track_id]
            track["detections"].append({
                "frame_token": frame_token,
                "centroid": detection["centroid"],
                "area": detection["area"],
                "bbox": detection["bbox"],
            })
            track["last_frame_token"] = frame_token
            assigned_tracks.add(track_id)
            assigned_detections.add(detection_index)

        for detection_index, detection in enumerate(detections):
            if detection_index in assigned_detections:
                continue
            tracks[next_track_id] = {
                "track_id": next_track_id,
                "start_frame_token": frame_token,
                "last_frame_token": frame_token,
                "detections": [{
                    "frame_token": frame_token,
                    "centroid": detection["centroid"],
                    "area": detection["area"],
                    "bbox": detection["bbox"],
                }],
            }
            active_track_ids.append(next_track_id)
            next_track_id += 1

    output_tracks = []
    for track in tracks.values():
        detections = track["detections"]
        if len(detections) < 3:
            continue
        areas = [int(detection["area"]) for detection in detections]
        output_tracks.append(
            {
                "track_id": track["track_id"],
                "start_frame_token": track["start_frame_token"],
                "end_frame_token": track["last_frame_token"],
                "length": len(detections),
                "mean_area": round(statistics.fmean(areas), 2),
                "max_area": max(areas),
                "trajectory": detections,
            }
        )

    return sorted(output_tracks, key=lambda item: item["length"], reverse=True)


def summarize_frame(artifacts: FrameArtifacts, width: int, height: int) -> dict[str, object]:
    frame = artifacts.frame
    rows = artifacts.rows
    segment_lengths = [len(segment.pixels) for row in rows for segment in row.segments]
    segment_sources = Counter(segment.source for row in rows for segment in row.segments)
    max_x = max((segment.x + len(segment.pixels) for row in rows for segment in row.segments), default=0)
    return {
        "frame_token": frame.frame_token,
        "frame_size": frame.frame_size,
        "inner_header_u32": artifacts.inner_header_u32,
        "row_count": len(rows),
        "segment_count": sum(len(row.segments) for row in rows),
        "max_x": max_x,
        "canvas_width": width,
        "canvas_height": height,
        "segment_length_summary": {
            "min": min(segment_lengths) if segment_lengths else 0,
            "max": max(segment_lengths) if segment_lengths else 0,
            "mode": statistics.multimode(segment_lengths)[0] if segment_lengths else 0,
        },
        "segment_sources": dict(segment_sources),
        "first_rows": [
            {
                "row_index": row.index,
                "segments": [asdict(segment) for segment in row.segments],
            }
            for row in rows[:8]
        ],
    }


def decode_stream(stream_tag: int, stream_frames: list[ReassembledFrame], output_dir: Path, export_limit: int) -> dict[str, object]:
    width, height = infer_stream_canvas(stream_frames)
    output_dir.mkdir(parents=True, exist_ok=True)

    artifacts_by_frame = [build_frame_artifacts(frame, width, height) for frame in stream_frames]
    tracks = track_components(artifacts_by_frame)
    tracks_path = output_dir / "tracks.json"
    tracks_path.write_text(json.dumps(tracks, ensure_ascii=False, indent=2), encoding="utf-8")

    exported_frames = []
    for artifacts in artifacts_by_frame[:export_limit]:
        image_path = output_dir / f"frame_{artifacts.frame.frame_token}.png"
        artifacts.image.save(image_path)
        mask_path = output_dir / f"frame_{artifacts.frame.frame_token}_mask.png"
        mask_to_image(artifacts.processed_mask).save(mask_path)
        frame_json_path = output_dir / f"frame_{artifacts.frame.frame_token}.json"
        frame_summary = summarize_frame(artifacts, width, height)
        frame_summary["components"] = artifacts.components[:24]
        frame_json_path.write_text(json.dumps(frame_summary, ensure_ascii=False, indent=2), encoding="utf-8")
        exported_frames.append({
            "frame_token": artifacts.frame.frame_token,
            "image": image_path.name,
            "mask": mask_path.name,
            "summary": frame_json_path.name,
            "row_count": frame_summary["row_count"],
            "segment_count": frame_summary["segment_count"],
            "component_count": len(artifacts.components),
        })

    frame_sizes = Counter(frame.frame_size for frame in stream_frames)
    row_counts = [len(artifacts.rows) for artifacts in artifacts_by_frame[:150]]
    stable_tracks = [track for track in tracks if track["length"] >= len(stream_frames) * 0.9]
    result = {
        "stream_tag": stream_tag,
        "frame_count": len(stream_frames),
        "canvas_width": width,
        "canvas_height": height,
        "inner_header_summary": summarize_header_fields(stream_frames),
        "frame_size_distribution": [{"frame_size": size, "count": count} for size, count in frame_sizes.most_common(12)],
        "row_count_modes": statistics.multimode(row_counts),
        "tracks_file": tracks_path.name,
        "track_count": len(tracks),
        "stable_track_count": len(stable_tracks),
        "stable_tracks": stable_tracks[:12],
        "longest_tracks": tracks[:12],
        "exported_frames": exported_frames,
    }
    return result


def summarize_stream_relationship(stream_map: dict[int, list[ReassembledFrame]]) -> dict[str, object] | None:
    if 0x1003 not in stream_map or 0x1004 not in stream_map:
        return None

    by_token: dict[int, dict[int, ReassembledFrame]] = defaultdict(dict)
    for stream_tag, frames in stream_map.items():
        for frame in frames:
            by_token[frame.frame_token][stream_tag] = frame

    common_tokens = sorted(token for token, streams in by_token.items() if 0x1003 in streams and 0x1004 in streams)
    if not common_tokens:
        return None

    differing_fields = []
    for field_index in range(20):
        diffs = {
            parse_inner_header_u32(by_token[token][0x1004].payload)[field_index]
            - parse_inner_header_u32(by_token[token][0x1003].payload)[field_index]
            for token in common_tokens[:200]
        }
        if diffs != {0}:
            differing_fields.append({"field_index": field_index, "diff_values": sorted(diffs)})

    return {
        "common_frame_count": len(common_tokens),
        "shared_frame_token_range": [common_tokens[0], common_tokens[-1]],
        "interpretation": "两路流在同一 frame_token 下共享完全相同的时间/帧计数字段，只在少数字段上存在固定差值，更像同阶段的双路同步 ROI 输出，而不是前后处理阶段串联输出。结合画面内容相似但几何分布不同，当前更偏向左右目或双相机通道的 ROI 结果图。",
        "differing_header_fields": differing_fields,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description="Decode sparse ROI records from reassembled vendor frames.")
    parser.add_argument("pcap", nargs="?", default="stylus.pcapng", help="Path to the pcapng file")
    parser.add_argument("--output-dir", default="roi_decoded_output", help="Directory for decoded ROI frames")
    parser.add_argument("--export-limit", type=int, default=12, help="How many frames to export per stream")
    args = parser.parse_args()

    frames = [frame for frame in reassemble_frames(Path(args.pcap)) if frame.complete]
    stream_map: dict[int, list[ReassembledFrame]] = defaultdict(list)
    for frame in frames:
        stream_map[frame.stream_tag].append(frame)

    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    summary = {"file": args.pcap, "streams": []}

    for stream_tag, stream_frames in sorted(stream_map.items()):
        stream_dir = output_dir / f"stream_{stream_tag:04x}"
        stream_summary = decode_stream(stream_tag, stream_frames, stream_dir, args.export_limit)
        summary["streams"].append(stream_summary)

    relationship = summarize_stream_relationship(stream_map)
    if relationship is not None:
        summary["stream_relationship"] = relationship

    summary_path = output_dir / "summary.json"
    summary_path.write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(summary, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()