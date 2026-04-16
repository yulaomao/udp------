from __future__ import annotations

import argparse
import binascii
import collections
import json
import statistics
import struct
from dataclasses import dataclass
from pathlib import Path

from scapy.all import IP, IPv6, Raw, UDP, PcapNgReader  # type: ignore


@dataclass(frozen=True)
class DirectionKey:
    src_ip: str
    src_port: int
    dst_ip: str
    dst_port: int

    def as_tuple(self) -> tuple[str, int, str, int]:
        return (self.src_ip, self.src_port, self.dst_ip, self.dst_port)


@dataclass(frozen=True)
class VendorHeader:
    stream_tag: int
    packet_words: int
    packet_sequence: int
    reserved: int
    frame_token: int
    frame_size: int
    payload_offset: int

    @property
    def stream_name(self) -> str:
        return f"0x{self.stream_tag:04x}"


def parse_vendor_header(payload: bytes) -> VendorHeader | None:
    if len(payload) < 24:
        return None

    values = struct.unpack("<HHIIIII", payload[:24])
    header = VendorHeader(*values)
    if header.packet_words * 2 != len(payload):
        return None
    if header.payload_offset > header.frame_size:
        return None
    if (len(payload) - 24) + header.payload_offset > header.frame_size:
        return None
    return header


def get_ip_layer(packet):
    if IP in packet:
        return packet[IP]
    if IPv6 in packet:
        return packet[IPv6]
    return None


def short_hex(payload: bytes, length: int = 24) -> str:
    return binascii.hexlify(payload[:length]).decode("ascii")


def ascii_preview(payload: bytes, length: int = 48) -> str:
    trimmed = payload[:length]
    return "".join(chr(byte) if 32 <= byte <= 126 else "." for byte in trimmed)


def common_prefix(payload: bytes, length: int = 4) -> str:
    return short_hex(payload, min(length, len(payload)))


def summarize_lengths(lengths: list[int]) -> dict[str, object]:
    if not lengths:
        return {}

    top_lengths = collections.Counter(lengths).most_common(10)
    summary: dict[str, object] = {
        "min": min(lengths),
        "max": max(lengths),
        "mean": round(statistics.fmean(lengths), 2),
        "median": statistics.median(lengths),
        "top_lengths": [{"length": length, "count": count} for length, count in top_lengths],
    }

    if len(lengths) > 1:
        summary["stdev"] = round(statistics.pstdev(lengths), 2)
    return summary


def analyze_pcap(path: Path, sample_limit: int = 6) -> dict[str, object]:
    flow_payloads: dict[DirectionKey, list[bytes]] = collections.defaultdict(list)
    flow_timestamps: dict[DirectionKey, list[float]] = collections.defaultdict(list)
    frame_groups: dict[tuple[int, int], list[VendorHeader]] = collections.defaultdict(list)
    stream_stats: dict[int, dict[str, object]] = {}
    non_udp = 0
    non_ip = 0
    total_packets = 0

    with PcapNgReader(str(path)) as reader:
        for packet in reader:
            total_packets += 1
            ip_layer = get_ip_layer(packet)
            if ip_layer is None:
                non_ip += 1
                continue
            if UDP not in packet:
                non_udp += 1
                continue

            udp = packet[UDP]
            payload = bytes(packet[Raw].load) if Raw in packet else b""
            key = DirectionKey(
                src_ip=str(ip_layer.src),
                src_port=int(udp.sport),
                dst_ip=str(ip_layer.dst),
                dst_port=int(udp.dport),
            )
            flow_payloads[key].append(payload)
            flow_timestamps[key].append(float(packet.time))

            header = parse_vendor_header(payload)
            if header is not None:
                frame_groups[(header.stream_tag, header.frame_token)].append(header)
                stats = stream_stats.setdefault(
                    header.stream_tag,
                    {
                        "stream_tag": header.stream_tag,
                        "packet_count": 0,
                        "frame_tokens": set(),
                        "frame_sizes": [],
                        "sequences": [],
                        "sample_headers": [],
                    },
                )
                stats["packet_count"] = int(stats["packet_count"]) + 1
                stats["frame_tokens"].add(header.frame_token)
                stats["frame_sizes"].append(header.frame_size)
                stats["sequences"].append(header.packet_sequence)
                sample_headers = stats["sample_headers"]
                if len(sample_headers) < sample_limit:
                    sample_headers.append(
                        {
                            "packet_sequence": header.packet_sequence,
                            "frame_token": header.frame_token,
                            "frame_size": header.frame_size,
                            "payload_offset": header.payload_offset,
                            "packet_words": header.packet_words,
                        }
                    )

    endpoints = collections.Counter()
    flows = []
    prefix_counter = collections.Counter()
    ordered_flows = sorted(flow_payloads.items(), key=lambda item: len(item[1]), reverse=True)

    for key, payloads in ordered_flows:
        lengths = [len(payload) for payload in payloads]
        payload_bytes = sum(lengths)
        prefixes = collections.Counter(common_prefix(payload) for payload in payloads if payload)
        prefix_counter.update(prefixes)
        timestamps = flow_timestamps[key]
        duration = round(max(timestamps) - min(timestamps), 6) if len(timestamps) > 1 else 0.0

        endpoints[(key.src_ip, key.src_port)] += len(payloads)
        endpoints[(key.dst_ip, key.dst_port)] += len(payloads)

        flow_info = {
            "direction": {
                "src_ip": key.src_ip,
                "src_port": key.src_port,
                "dst_ip": key.dst_ip,
                "dst_port": key.dst_port,
            },
            "packet_count": len(payloads),
            "payload_bytes": payload_bytes,
            "duration_seconds": duration,
            "length_summary": summarize_lengths(lengths),
            "common_prefixes": [{"prefix": prefix, "count": count} for prefix, count in prefixes.most_common(10)],
            "samples": [
                {
                    "length": len(payload),
                    "hex": short_hex(payload, 48),
                    "ascii": ascii_preview(payload),
                }
                for payload in payloads[:sample_limit]
            ],
        }
        flows.append(flow_info)

    parsed_frames = []
    for (stream_tag, frame_token), headers in sorted(frame_groups.items(), key=lambda item: (item[0][0], item[0][1])):
        headers = sorted(headers, key=lambda item: item.payload_offset)
        frame_size = headers[0].frame_size
        covered_bytes = sum(max(0, header.packet_words * 2 - 24) for header in headers)
        complete = False
        if headers:
            expected_offset = 0
            complete = True
            for header in headers:
                packet_payload_size = header.packet_words * 2 - 24
                if header.payload_offset != expected_offset:
                    complete = False
                    break
                expected_offset += packet_payload_size
            if expected_offset != frame_size:
                complete = False

        parsed_frames.append(
            {
                "stream_tag": stream_tag,
                "frame_token": frame_token,
                "frame_size": frame_size,
                "packet_count": len(headers),
                "covered_bytes": covered_bytes,
                "complete": complete,
                "offset_samples": [header.payload_offset for header in headers[:sample_limit]],
            }
        )

    stream_summaries = []
    for stream_tag, stats in sorted(stream_stats.items()):
        frame_sizes = list(stats["frame_sizes"])
        sequences = list(stats["sequences"])
        stream_frames = [frame for frame in parsed_frames if frame["stream_tag"] == stream_tag]
        complete_frames = sum(1 for frame in stream_frames if frame["complete"])

        stream_summaries.append(
            {
                "stream_tag": stream_tag,
                "stream_name": f"0x{stream_tag:04x}",
                "packet_count": stats["packet_count"],
                "frame_count": len(stats["frame_tokens"]),
                "complete_frames": complete_frames,
                "frame_size_summary": summarize_lengths(frame_sizes),
                "packet_sequence_range": {
                    "min": min(sequences),
                    "max": max(sequences),
                },
                "sample_headers": stats["sample_headers"],
                "sample_frames": stream_frames[:sample_limit],
            }
        )

    result = {
        "file": str(path),
        "total_packets": total_packets,
        "udp_packets": sum(len(payloads) for payloads in flow_payloads.values()),
        "non_udp_packets": non_udp,
        "non_ip_packets": non_ip,
        "top_endpoints": [
            {
                "ip": ip,
                "port": port,
                "packet_mentions": count,
            }
            for (ip, port), count in endpoints.most_common(12)
        ],
        "top_prefixes": [{"prefix": prefix, "count": count} for prefix, count in prefix_counter.most_common(12)],
        "vendor_header_inference": {
            "header_length": 24,
            "packet_words_times_two_matches_payload_length": True,
            "streams": stream_summaries,
        },
        "flows": flows,
    }
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description="Analyze UDP conversations inside a pcapng capture.")
    parser.add_argument("pcap", nargs="?", default="stylus.pcapng", help="Path to the pcapng file")
    parser.add_argument("--sample-limit", type=int, default=6, help="Number of payload samples per flow")
    parser.add_argument("--output", help="Optional path to save the JSON result")
    args = parser.parse_args()

    result = analyze_pcap(Path(args.pcap), sample_limit=args.sample_limit)
    text = json.dumps(result, ensure_ascii=False, indent=2)

    if args.output:
        Path(args.output).write_text(text, encoding="utf-8")
    else:
        print(text)


if __name__ == "__main__":
    main()