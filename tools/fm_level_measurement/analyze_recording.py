#!/usr/bin/env python3
#
# Analyze a recorded WAV file from generate_measurement_midi.py.
#

from __future__ import annotations

import argparse
import csv
import math
import wave
from collections import defaultdict
from pathlib import Path


NEG_INF_DB = -120.0


def read_manifest(path: Path) -> tuple[dict[str, str], list[dict[str, str]]]:
    metadata: dict[str, str] = {}
    data_lines: list[str] = []
    for line in path.read_text().splitlines():
        if line.startswith("#"):
            key_value = line[1:].strip().split(",", 1)
            if len(key_value) == 2:
                metadata[key_value[0].strip()] = key_value[1].strip()
        elif line.strip():
            data_lines.append(line)

    rows = list(csv.DictReader(data_lines))
    return metadata, rows


def decode_sample(data: bytes, offset: int, sample_width: int) -> float:
    if sample_width == 1:
        return (data[offset] - 128) / 128.0
    if sample_width == 2:
        return int.from_bytes(data[offset:offset + 2], "little", signed=True) / 32768.0
    if sample_width == 3:
        raw = int.from_bytes(data[offset:offset + 3], "little", signed=False)
        if raw & 0x800000:
            raw -= 0x1000000
        return raw / 8388608.0
    if sample_width == 4:
        return int.from_bytes(data[offset:offset + 4], "little", signed=True) / 2147483648.0
    raise ValueError(f"unsupported sample width: {sample_width} bytes")


def rms_for_range(data: bytes, channels: int, sample_width: int, start_frame: int, end_frame: int) -> float:
    frame_size = channels * sample_width
    start = max(0, start_frame) * frame_size
    end = max(start_frame, end_frame) * frame_size
    end = min(end, len(data))

    total = 0.0
    count = 0
    for frame_offset in range(start, end, frame_size):
        for ch in range(channels):
            sample = decode_sample(data, frame_offset + ch * sample_width, sample_width)
            total += sample * sample
            count += 1

    if count == 0:
        return 0.0
    return math.sqrt(total / count)


def dbfs(rms: float) -> float:
    if rms <= 0.0:
        return NEG_INF_DB
    return 20.0 * math.log10(rms)


def find_first_audio_time(data: bytes, channels: int, sample_width: int, sample_rate: int, threshold_dbfs: float, search_seconds: float) -> float | None:
    threshold = 10.0 ** (threshold_dbfs / 20.0)
    frame_size = channels * sample_width
    total_frames = min(int(search_seconds * sample_rate), len(data) // frame_size)
    window_frames = max(1, int(0.010 * sample_rate))

    for start_frame in range(0, total_frames, window_frames):
        end_frame = min(start_frame + window_frames, total_frames)
        if rms_for_range(data, channels, sample_width, start_frame, end_frame) >= threshold:
            return start_frame / sample_rate
    return None


def median(values: list[float]) -> float:
    if not values:
        return 0.0
    sorted_values = sorted(values)
    mid = len(sorted_values) // 2
    if len(sorted_values) % 2:
        return sorted_values[mid]
    return (sorted_values[mid - 1] + sorted_values[mid]) / 2.0


def clamp(value: int, minimum: int, maximum: int) -> int:
    return max(minimum, min(maximum, value))


def format_cpp_table(tl_trim: dict[int, int], table_name: str) -> str:
    values = [tl_trim.get(program, 0) for program in range(128)]
    lines = [
        f"static constexpr int8_t {table_name}[128] = {{",
    ]
    for start in range(0, 128, 16):
        chunk = values[start:start + 16]
        lines.append("    " + ", ".join(f"{value:4d}" for value in chunk) + f",  // {start:3d}-{start + len(chunk) - 1:3d}")
    lines.append("};")
    return "\n".join(lines) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser(description="Analyze a WAV recording and create FM TL trim data.")
    parser.add_argument("wav", type=Path, help="recorded WAV file")
    parser.add_argument("--manifest", type=Path, default=Path("out/fm_level_measurement_manifest.csv"), help="manifest CSV from generate_measurement_midi.py")
    parser.add_argument("--out-csv", type=Path, default=Path("out/fm_level_trim.csv"), help="analysis CSV output")
    parser.add_argument("--out-cpp", type=Path, default=Path("out/fm_tl_trim.inc"), help="C++ table output")
    parser.add_argument("--table-name", default="fm_tl_trim", help="C++ table name")
    parser.add_argument("--offset-seconds", type=float, default=0.0, help="recording time minus MIDI timeline time")
    parser.add_argument("--auto-offset", action="store_true", help="estimate offset from the first sync click")
    parser.add_argument("--auto-offset-threshold-dbfs", type=float, default=-45.0, help="RMS threshold for sync click detection")
    parser.add_argument("--auto-offset-search-seconds", type=float, default=10.0, help="search range for sync click detection")
    parser.add_argument("--attack-skip-seconds", type=float, default=0.10, help="skip this much after each note-on")
    parser.add_argument("--release-skip-seconds", type=float, default=0.05, help="skip this much before each note-off")
    parser.add_argument("--target-dbfs", type=float, default=None, help="target level; defaults to median measured program level")
    parser.add_argument("--tl-step-db", type=float, default=0.75, help="YM2608 FM TL dB per step")
    parser.add_argument("--min-trim", type=int, default=-24, help="minimum trim step")
    parser.add_argument("--max-trim", type=int, default=24, help="maximum trim step")
    args = parser.parse_args()

    metadata, rows = read_manifest(args.manifest)

    with wave.open(str(args.wav), "rb") as wav:
        channels = wav.getnchannels()
        sample_width = wav.getsampwidth()
        sample_rate = wav.getframerate()
        frame_count = wav.getnframes()
        data = wav.readframes(frame_count)

    if sample_width not in (1, 2, 3, 4):
        raise SystemExit(f"Unsupported WAV sample width: {sample_width} bytes")

    offset_seconds = args.offset_seconds
    if args.auto_offset:
        first_click = float(metadata.get("first_click_start_seconds", "0"))
        detected = find_first_audio_time(
            data,
            channels,
            sample_width,
            sample_rate,
            args.auto_offset_threshold_dbfs,
            args.auto_offset_search_seconds,
        )
        if detected is None:
            raise SystemExit("Could not detect sync click. Try --offset-seconds or lower --auto-offset-threshold-dbfs.")
        offset_seconds = detected - first_click
        print(f"Auto offset: detected first audio at {detected:.3f}s, MIDI first click at {first_click:.3f}s, offset {offset_seconds:.3f}s")

    note_results: list[dict[str, str]] = []
    power_by_program: dict[int, list[float]] = defaultdict(list)
    db_by_program_note: dict[int, list[str]] = defaultdict(list)

    for row in rows:
        program = int(row["program"])
        note = int(row["note"])
        start = float(row["start_seconds"]) + offset_seconds + args.attack_skip_seconds
        end = float(row["start_seconds"]) + offset_seconds + float(row["duration_seconds"]) - args.release_skip_seconds
        start_frame = round(start * sample_rate)
        end_frame = round(end * sample_rate)
        rms = rms_for_range(data, channels, sample_width, start_frame, end_frame)
        level_dbfs = dbfs(rms)
        power_by_program[program].append(rms * rms)
        db_by_program_note[program].append(f"{note}:{level_dbfs:.2f}")
        note_results.append({
            "program": str(program),
            "note": str(note),
            "dbfs": f"{level_dbfs:.3f}",
            "window_start_seconds": f"{start:.6f}",
            "window_end_seconds": f"{end:.6f}",
        })

    program_levels: dict[int, float] = {}
    for program, powers in power_by_program.items():
        mean_power = sum(powers) / len(powers)
        program_levels[program] = dbfs(math.sqrt(mean_power))

    target_dbfs = args.target_dbfs if args.target_dbfs is not None else median(list(program_levels.values()))
    tl_trim: dict[int, int] = {}

    args.out_csv.parent.mkdir(parents=True, exist_ok=True)
    with args.out_csv.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=["program", "dbfs", "delta_db", "trim_step", "note_dbfs"])
        writer.writeheader()
        for program in sorted(program_levels):
            delta = program_levels[program] - target_dbfs
            trim = clamp(round(delta / args.tl_step_db), args.min_trim, args.max_trim)
            tl_trim[program] = trim
            writer.writerow({
                "program": program,
                "dbfs": f"{program_levels[program]:.3f}",
                "delta_db": f"{delta:.3f}",
                "trim_step": trim,
                "note_dbfs": " ".join(db_by_program_note[program]),
            })

    args.out_cpp.parent.mkdir(parents=True, exist_ok=True)
    args.out_cpp.write_text(format_cpp_table(tl_trim, args.table_name))

    note_csv = args.out_csv.with_name(args.out_csv.stem + "_notes.csv")
    with note_csv.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=["program", "note", "dbfs", "window_start_seconds", "window_end_seconds"])
        writer.writeheader()
        writer.writerows(note_results)

    print(f"WAV: {args.wav}")
    print(f"Format: {channels}ch, {sample_rate} Hz, {sample_width * 8}-bit PCM")
    print(f"Target: {target_dbfs:.2f} dBFS")
    print(f"Wrote {args.out_csv}")
    print(f"Wrote {note_csv}")
    print(f"Wrote {args.out_cpp}")


if __name__ == "__main__":
    main()
