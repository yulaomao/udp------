from __future__ import annotations

import argparse
from pathlib import Path

from scapy.utils import RawPcapNgReader, RawPcapNgWriter


def count_packets(path: Path) -> int:
    count = 0
    with RawPcapNgReader(str(path)) as reader:
        for _packet, _meta in reader:
            count += 1
    return count


def packet_timestamp_seconds(meta) -> float:
    raw_timestamp = (meta.tshigh << 32) | meta.tslow
    if not meta.tsresol:
        return 0.0
    microseconds = raw_timestamp * 1_000_000 // meta.tsresol
    return microseconds / 1_000_000


def output_paths(input_path: Path, parts: int) -> list[Path]:
    stem = input_path.stem
    suffix = input_path.suffix or ".pcapng"
    return [input_path.with_name(f"{stem}_{index:02d}{suffix}") for index in range(1, parts + 1)]


def split_pcapng(input_path: Path, parts: int) -> list[Path]:
    if parts < 2:
        raise ValueError("parts must be at least 2")

    total_packets = count_packets(input_path)
    if total_packets == 0:
        raise ValueError("input capture has no packets")

    targets = []
    base = total_packets // parts
    remainder = total_packets % parts
    for index in range(parts):
        targets.append(base + (1 if index < remainder else 0))

    paths = output_paths(input_path, parts)
    for path in paths:
        if path.exists():
            path.unlink()

    writers = [RawPcapNgWriter(str(path)) for path in paths]
    initialized = [False for _ in paths]
    counts = [0 for _ in paths]

    try:
        current_index = 0
        with RawPcapNgReader(str(input_path)) as reader:
            for packet, meta in reader:
                while current_index < len(targets) - 1 and counts[current_index] >= targets[current_index]:
                    current_index += 1

                writer = writers[current_index]
                if not initialized[current_index]:
                    writer._write_block_shb()
                    writer._write_block_idb(linktype=meta.linktype)
                    writer.header_present = True
                    initialized[current_index] = True

                timestamp = packet_timestamp_seconds(meta)
                writer._write_packet(
                    packet,
                    linktype=meta.linktype,
                    sec=timestamp,
                    caplen=len(packet),
                    wirelen=meta.wirelen,
                    ifname=meta.ifname,
                    direction=meta.direction,
                    comments=meta.comments,
                )
                counts[current_index] += 1
    finally:
        for writer in writers:
            writer.close()

    return paths


def main() -> None:
    parser = argparse.ArgumentParser(description="Split one pcapng into multiple valid pcapng files.")
    parser.add_argument("input", type=Path, help="Path to the source .pcapng file")
    parser.add_argument("--parts", type=int, default=3, help="Number of output files")
    args = parser.parse_args()

    created = split_pcapng(args.input, args.parts)
    for path in created:
        print(path)


if __name__ == "__main__":
    main()