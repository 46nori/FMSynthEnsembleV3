#!/usr/bin/env python3
#
# Generate a standard MIDI file for measuring FM program loudness.
#

from __future__ import annotations

import argparse
import csv
from pathlib import Path


def parse_int_list(value: str) -> list[int]:
    result: list[int] = []
    for part in value.split(","):
        part = part.strip()
        if not part:
            continue
        if "-" in part:
            start_text, end_text = part.split("-", 1)
            start = int(start_text)
            end = int(end_text)
            step = 1 if start <= end else -1
            result.extend(range(start, end + step, step))
        else:
            result.append(int(part))
    return result


def checked_midi_value(value: int, name: str) -> int:
    if not 0 <= value <= 127:
        raise argparse.ArgumentTypeError(f"{name} must be in 0..127: {value}")
    return value


def encode_vlq(value: int) -> bytes:
    if value < 0:
        raise ValueError("delta time must be non-negative")

    encoded = [value & 0x7F]
    value >>= 7
    while value:
        encoded.insert(0, 0x80 | (value & 0x7F))
        value >>= 7
    return bytes(encoded)


def meta_event(delta_ticks: int, meta_type: int, data: bytes) -> bytes:
    return encode_vlq(delta_ticks) + bytes([0xFF, meta_type]) + encode_vlq(len(data)) + data


def midi_event(delta_ticks: int, data: list[int]) -> bytes:
    return encode_vlq(delta_ticks) + bytes(data)


class MidiTrack:
    def __init__(self, ticks_per_second: float) -> None:
        self._ticks_per_second = ticks_per_second
        self._events: list[tuple[int, bytes]] = []

    def seconds_to_ticks(self, seconds: float) -> int:
        return round(seconds * self._ticks_per_second)

    def add(self, seconds: float, data: list[int]) -> None:
        self._events.append((self.seconds_to_ticks(seconds), bytes(data)))

    def add_meta(self, seconds: float, meta_type: int, data: bytes) -> None:
        self._events.append((self.seconds_to_ticks(seconds), bytes([0xFF, meta_type]) + encode_vlq(len(data)) + data))

    def render(self) -> bytes:
        body = bytearray()
        last_tick = 0
        for tick, data in sorted(enumerate(self._events), key=lambda item: (item[1][0], item[0])):
            del tick
            abs_tick, payload = data
            body.extend(encode_vlq(abs_tick - last_tick))
            body.extend(payload)
            last_tick = abs_tick
        body.extend(meta_event(0, 0x2F, b""))
        return bytes(body)


def write_midi(path: Path, track_data: bytes, ticks_per_quarter: int) -> None:
    header = b"MThd" + (6).to_bytes(4, "big") + (0).to_bytes(2, "big") + (1).to_bytes(2, "big") + ticks_per_quarter.to_bytes(2, "big")
    track = b"MTrk" + len(track_data).to_bytes(4, "big") + track_data
    path.write_bytes(header + track)


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate a MIDI file for FM program level measurement.")
    parser.add_argument("--out", type=Path, default=Path("out/fm_level_measurement.mid"), help="output MIDI file")
    parser.add_argument("--manifest", type=Path, default=Path("out/fm_level_measurement_manifest.csv"), help="output timing manifest CSV")
    parser.add_argument("--programs", default="0-127", help="program list, for example: 0-127 or 0,4,8-15")
    parser.add_argument("--notes", default="60,67,72", help="note list, for example: 60,67,72")
    parser.add_argument("--channel", type=int, default=0, help="MIDI channel number, 0-15")
    parser.add_argument("--volume", type=int, default=100, help="CC#7 volume, 0-127")
    parser.add_argument("--expression", type=int, default=127, help="CC#11 expression, 0-127")
    parser.add_argument("--velocity", type=int, default=100, help="note-on velocity, 0-127")
    parser.add_argument("--tempo-bpm", type=float, default=120.0, help="MIDI tempo")
    parser.add_argument("--ticks-per-quarter", type=int, default=480, help="MIDI resolution")
    parser.add_argument("--lead-in-seconds", type=float, default=0.5, help="silence before sync clicks")
    parser.add_argument("--sync-clicks", type=int, default=3, help="number of sync clicks before measurement")
    parser.add_argument("--sync-note", type=int, default=96, help="sync click note number")
    parser.add_argument("--sync-duration-seconds", type=float, default=0.05, help="sync click duration")
    parser.add_argument("--sync-gap-seconds", type=float, default=0.35, help="gap after each sync click")
    parser.add_argument("--pre-measure-seconds", type=float, default=0.8, help="gap after sync clicks before program 0")
    parser.add_argument("--note-seconds", type=float, default=1.0, help="duration of each measurement note")
    parser.add_argument("--note-gap-seconds", type=float, default=0.3, help="gap after each note")
    parser.add_argument("--program-gap-seconds", type=float, default=0.5, help="gap after each program")
    args = parser.parse_args()

    if not 0 <= args.channel <= 15:
        raise SystemExit("--channel must be in 0..15")
    for value, name in ((args.volume, "volume"), (args.expression, "expression"), (args.velocity, "velocity"), (args.sync_note, "sync_note")):
        checked_midi_value(value, name)

    programs = parse_int_list(args.programs)
    notes = parse_int_list(args.notes)
    for program in programs:
        checked_midi_value(program, "program")
    for note in notes:
        checked_midi_value(note, "note")

    ticks_per_second = args.ticks_per_quarter * args.tempo_bpm / 60.0
    usec_per_quarter = round(60_000_000 / args.tempo_bpm)
    track = MidiTrack(ticks_per_second)

    status_cc = 0xB0 | args.channel
    status_pc = 0xC0 | args.channel
    status_note_on = 0x90 | args.channel
    status_note_off = 0x80 | args.channel

    track.add_meta(0.0, 0x03, b"FM level measurement")
    track.add_meta(0.0, 0x51, usec_per_quarter.to_bytes(3, "big"))
    track.add(0.0, [status_cc, 7, args.volume])
    track.add(0.0, [status_cc, 11, args.expression])
    track.add(0.0, [status_cc, 121, 0])

    now = args.lead_in_seconds
    first_click_start = now if args.sync_clicks > 0 else 0.0
    if args.sync_clicks > 0:
        track.add(now, [status_pc, 0])
        for _ in range(args.sync_clicks):
            track.add(now, [status_note_on, args.sync_note, 127])
            track.add(now + args.sync_duration_seconds, [status_note_off, args.sync_note, 0])
            now += args.sync_duration_seconds + args.sync_gap_seconds

    measurement_start = now + args.pre_measure_seconds
    now = measurement_start
    rows: list[dict[str, str]] = []

    for program in programs:
        program_start = now
        track.add(now, [status_pc, program])
        track.add(now, [status_cc, 7, args.volume])
        track.add(now, [status_cc, 11, args.expression])
        for note in notes:
            start = now
            track.add(start, [status_note_on, note, args.velocity])
            track.add(start + args.note_seconds, [status_note_off, note, 0])
            rows.append({
                "program": str(program),
                "note": str(note),
                "start_seconds": f"{start:.6f}",
                "duration_seconds": f"{args.note_seconds:.6f}",
                "program_start_seconds": f"{program_start:.6f}",
            })
            now += args.note_seconds + args.note_gap_seconds
        track.add(now, [status_cc, 123, 0])
        now += args.program_gap_seconds

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.manifest.parent.mkdir(parents=True, exist_ok=True)
    write_midi(args.out, track.render(), args.ticks_per_quarter)

    with args.manifest.open("w", newline="") as f:
        f.write("# generated_by,generate_measurement_midi.py\n")
        f.write(f"# tempo_bpm,{args.tempo_bpm:.6f}\n")
        f.write(f"# channel,{args.channel}\n")
        f.write(f"# volume,{args.volume}\n")
        f.write(f"# expression,{args.expression}\n")
        f.write(f"# velocity,{args.velocity}\n")
        f.write(f"# first_click_start_seconds,{first_click_start:.6f}\n")
        f.write(f"# measurement_start_seconds,{measurement_start:.6f}\n")
        writer = csv.DictWriter(f, fieldnames=["program", "note", "start_seconds", "duration_seconds", "program_start_seconds"])
        writer.writeheader()
        writer.writerows(rows)

    print(f"Wrote {args.out}")
    print(f"Wrote {args.manifest}")
    print(f"Measurement starts at {measurement_start:.3f} seconds in the MIDI timeline.")


if __name__ == "__main__":
    main()
