#!/usr/bin/env python3
"""Generate tone_table.inc from libOPNMIDI fmmidi.wopn (GM bank 0 direct map)."""

from __future__ import annotations

import re
import struct
import sys
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LIBRARY = ROOT / "extern" / "libOPNMIDI"
OUTPUT = ROOT / "src" / "drivers" / "fm" / "tone" / "tone_table.inc"

SOURCE_BANK = "fm_banks/fmmidi.wopn"
INST_SIZE_V1 = 65
INST_SIZE_V2 = 69

# BSD 3-Clause License text from fmmidi (supercatexpert/fmmidi license.txt).
# fmmidi.wopn in libOPNMIDI carries the same license per fm_banks/fmmidi-readme.txt.
FMMIDI_BSD3_LICENSE_LINES = [
    "Copyright (c) 2003-2006 yuno.",
    "All Rights Reserved.",
    "",
    "Redistribution and use in source and binary forms, with or without modification,",
    "are permitted provided that the following conditions are met:",
    "",
    "1. Redistributions of source code must retain the above copyright notice, this list",
    "of conditions and the following disclaimer.",
    "",
    "2. Redistributions in binary form must reproduce the above copyright notice, this",
    "list of conditions and the following disclaimer in the documentation and/or other",
    "materials provided with the distribution.",
    "",
    "3. The name of the author may not be used to endorse or promote products",
    "derived from this software without specific prior written permission.",
    "",
    'THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR IMPLIED',
    "WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF",
    "MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN",
    "NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,",
    "SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED",
    "TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR",
    "PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF",
    "LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING",
    "NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS",
    "SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.",
]

GM_NAMES = [
    "Acoustic Grand Piano",
    "Bright Acoustic Piano",
    "Electric Grand Piano",
    "Honky-tonk Piano",
    "Electric Piano 1",
    "Electric Piano 2",
    "Harpsichord",
    "Clavi",
    "Celesta",
    "Glockenspiel",
    "Music Box",
    "Vibraphone",
    "Marimba",
    "Xylophone",
    "Tubular Bells",
    "Dulcimer",
    "Drawbar Organ",
    "Percussive Organ",
    "Rock Organ",
    "Church Organ",
    "Reed Organ",
    "Accordion",
    "Harmonica",
    "Tango Accordion",
    "Acoustic Guitar (nylon)",
    "Acoustic Guitar (steel)",
    "Electric Guitar (jazz)",
    "Electric Guitar (clean)",
    "Electric Guitar (muted)",
    "Overdriven Guitar",
    "Distortion Guitar",
    "Guitar Harmonics",
    "Acoustic Bass",
    "Electric Bass (finger)",
    "Electric Bass (pick)",
    "Fretless Bass",
    "Slap Bass 1",
    "Slap Bass 2",
    "Synth Bass 1",
    "Synth Bass 2",
    "Violin",
    "Viola",
    "Cello",
    "Contrabass",
    "Tremolo Strings",
    "Pizzicato Strings",
    "Orchestral Harp",
    "Timpani",
    "String Ensemble 1",
    "String Ensemble 2",
    "Synth Strings 1",
    "Synth Strings 2",
    "Choir Aahs",
    "Voice Oohs",
    "Synth Voice",
    "Orchestra Hit",
    "Trumpet",
    "Trombone",
    "Tuba",
    "Muted Trumpet",
    "French Horn",
    "Brass Section",
    "Synth Brass 1",
    "Synth Brass 2",
    "Soprano Sax",
    "Alto Sax",
    "Tenor Sax",
    "Baritone Sax",
    "Oboe",
    "English Horn",
    "Bassoon",
    "Clarinet",
    "Piccolo",
    "Flute",
    "Recorder",
    "Pan Flute",
    "Blown Bottle",
    "Shakuhachi",
    "Whistle",
    "Ocarina",
    "Lead 1 (square)",
    "Lead 2 (sawtooth)",
    "Lead 3 (calliope)",
    "Lead 4 (chiff)",
    "Lead 5 (charang)",
    "Lead 6 (voice)",
    "Lead 7 (fifths)",
    "Lead 8 (bass+lead)",
    "Pad 1 (new age)",
    "Pad 2 (warm)",
    "Pad 3 (polysynth)",
    "Pad 4 (choir)",
    "Pad 5 (bowed)",
    "Pad 6 (metallic)",
    "Pad 7 (halo)",
    "Pad 8 (sweep)",
    "FX 1 (rain)",
    "FX 2 (soundtrack)",
    "FX 3 (crystal)",
    "FX 4 (atmosphere)",
    "FX 5 (brightness)",
    "FX 6 (goblins)",
    "FX 7 (echoes)",
    "FX 8 (sci-fi)",
    "Sitar",
    "Banjo",
    "Shamisen",
    "Koto",
    "Kalimba",
    "Bagpipe",
    "Fiddle",
    "Shanai",
    "Tinkle Bell",
    "Agogo",
    "Steel Drums",
    "Woodblock",
    "Taiko Drum",
    "Melodic Tom",
    "Synth Drum",
    "Reverse Cymbal",
    "Guitar Fret Noise",
    "Breath Noise",
    "Seashore",
    "Bird Tweet",
    "Telephone Ring",
    "Helicopter",
    "Applause",
    "Gunshot",
]


@dataclass
class WopnOperator:
    dtfm_30: int = 0
    level_40: int = 0
    rsatk_50: int = 0
    amdecay1_60: int = 0
    decay2_70: int = 0
    susrel_80: int = 0
    ssgeg_90: int = 0


@dataclass
class WopnInstrument:
    rel_path: str
    index: int
    name: str
    bank_name: str
    bank_msb: int
    bank_lsb: int
    bank_index: int
    is_percussion: bool
    chip_type: int
    fbalg: int = 0
    lfosens: int = 0
    ops: list[WopnOperator] = field(default_factory=lambda: [WopnOperator() for _ in range(4)])

    @property
    def source_label(self) -> str:
        label = f"{self.rel_path} program {self.index}"
        if self.name.strip():
            label += f" ({self.name.strip()})"
        return label


def clean_inst_name(raw: str) -> str:
    text = raw.replace("\x00", " ").strip()
    if "*" in text:
        text = text.rsplit("*", 1)[-1].strip()
    text = re.sub(r"[^\x20-\x7e]+", " ", text)
    text = re.sub(r"\s+", " ", text).strip()
    return text


def parse_wopn(path: Path) -> list[WopnInstrument]:
    rel_path = path.relative_to(LIBRARY).as_posix()
    data = path.read_bytes()
    pos = 0

    if data[:11] == b"WOPN2-BANK\x00":
        version = 1
        pos = 11
    elif data[:11] == b"WOPN2-B2NK\x00":
        pos = 11
        version = struct.unpack_from("<H", data, pos)[0]
        pos += 2
    else:
        raise ValueError(f"{rel_path}: bad WOPN magic")

    head = data[pos : pos + 5]
    pos += 5
    mel_count = struct.unpack(">H", head[:2])[0]
    perc_count = struct.unpack(">H", head[2:4])[0]
    chip_type = (head[4] >> 4) & 1 if version >= 2 else 0
    inst_size = INST_SIZE_V2 if version > 1 else INST_SIZE_V1

    bank_headers: list[tuple[bool, int, str, int, int]] = []
    if version >= 2:
        for is_perc, count in ((False, mel_count), (True, perc_count)):
            for bank_index in range(count):
                bank_name = data[pos : pos + 32].decode("latin1", errors="replace").rstrip("\x00")
                lsb = data[pos + 32]
                msb = data[pos + 33]
                pos += 34
                bank_headers.append((is_perc, bank_index, bank_name, msb, lsb))

    instruments: list[WopnInstrument] = []
    header_idx = 0
    for is_perc, count in ((False, mel_count), (True, perc_count)):
        for _ in range(count):
            is_percussion, bank_index, bank_name, bank_msb, bank_lsb = bank_headers[header_idx]
            header_idx += 1
            for index in range(128):
                chunk = data[pos : pos + inst_size]
                pos += inst_size
                inst = WopnInstrument(
                    rel_path=rel_path,
                    index=index,
                    name=clean_inst_name(chunk[:32].decode("latin1", errors="replace")),
                    bank_name=clean_inst_name(bank_name),
                    bank_msb=bank_msb,
                    bank_lsb=bank_lsb,
                    bank_index=bank_index,
                    is_percussion=is_percussion,
                    chip_type=chip_type,
                    fbalg=chunk[35],
                    lfosens=chunk[36],
                )
                for op_idx in range(4):
                    off = 37 + op_idx * 7
                    inst.ops[op_idx] = WopnOperator(
                        dtfm_30=chunk[off + 0],
                        level_40=chunk[off + 1],
                        rsatk_50=chunk[off + 2],
                        amdecay1_60=chunk[off + 3],
                        decay2_70=chunk[off + 4],
                        susrel_80=chunk[off + 5],
                        ssgeg_90=chunk[off + 6],
                    )
                instruments.append(inst)
    return instruments


def load_fmmidi_gm() -> list[WopnInstrument]:
    path = LIBRARY / SOURCE_BANK
    if not path.is_file():
        raise FileNotFoundError(path)

    by_index: dict[int, WopnInstrument] = {}
    for inst in parse_wopn(path):
        if (
            not inst.is_percussion
            and inst.bank_msb == 0
            and inst.bank_lsb == 0
            and inst.bank_index == 0
            and 0 <= inst.index < 128
        ):
            by_index[inst.index] = inst

    missing = [i for i in range(128) if i not in by_index]
    if missing:
        raise ValueError(f"{SOURCE_BANK}: missing melodic bank 0 programs: {missing}")

    return [by_index[i] for i in range(128)]


def wopn_to_tone(inst: WopnInstrument) -> list[int]:
    # WOPN operators are native OPN register bytes (M1, C1, M2, C2 order).
    dt_multi = [op.dtfm_30 for op in inst.ops]
    tl = [op.level_40 for op in inst.ops]
    ks_ar = [op.rsatk_50 for op in inst.ops]
    dr = [op.amdecay1_60 for op in inst.ops]
    sr = [op.decay2_70 for op in inst.ops]
    sl_rr = [op.susrel_80 for op in inst.ops]
    ssg = [op.ssgeg_90 for op in inst.ops]
    tone = dt_multi + tl + ks_ar + dr + sr + sl_rr + ssg + [inst.fbalg]
    if len(tone) != 29:
        raise ValueError(f"unexpected tone length {len(tone)} for {inst.source_label}")
    return tone


def format_file_header() -> str:
    lines = [
        "// FM tone parameter table (General MIDI Level 1, 128 programs)",
        "// Program Change N -> fm_tone_table[N] (29-byte OPN register values per program)",
        "//",
        "// Instrument data provenance:",
        "//   (1) fmmidi — hardcoded GM instruments in program.h",
        "//       https://github.com/supercatexpert/fmmidi/blob/master/program.h",
        "//   (2) libOPNMIDI — fm_banks/fmmidi.wopn (bank MSB=0, LSB=0), converted from (1)",
        "//       https://github.com/Wohlstand/libOPNMIDI/blob/master/fm_banks/fmmidi.wopn",
        "//       https://github.com/Wohlstand/libOPNMIDI/blob/master/fm_banks/fmmidi-readme.txt",
        "//   (3) This file — OPN register bytes extracted from (2) by",
        "//       tools/gen_tone_table/generate_tone_table.py",
        "//",
        "// License for instrument data in (1)–(3): BSD 3-Clause License",
        "//   (also stated in fmmidi-readme.txt for the fmmidi.wopn bank)",
        "// Full text (from https://github.com/supercatexpert/fmmidi/blob/master/license.txt):",
    ]
    for line in FMMIDI_BSD3_LICENSE_LINES:
        lines.append("//" if line == "" else f"// {line}")
    lines.append("static constexpr uint8_t fm_tone_table[][29] = {")
    return "\n".join(lines)


def format_tone(no: int, gm_name: str, inst: WopnInstrument, tone: list[int]) -> str:
    lines = [
        f"    {{// No.{no} GM:{gm_name}",
        f"        // source: libOPNMIDI/{inst.source_label}",
    ]
    groups = [
        ("0x30 DT_MULTI", tone[0:4]),
        ("0x40 TL", tone[4:8]),
        ("0x50 KS_AR", tone[8:12]),
        ("0x60 DR", tone[12:16]),
        ("0x70 SR", tone[16:20]),
        ("0x80 SL_RR", tone[20:24]),
        ("0x90 SSG-EG", tone[24:28]),
    ]
    for label, values in groups:
        hex_values = ", ".join(f"0x{v:02x}" for v in values)
        lines.append(f"        {hex_values}, // {label}")
    lines.append(f"        0x{tone[28]:02x}                    // 0xb0 FB_CONNECT")
    lines.append("    },")
    return "\n".join(lines)


def main() -> int:
    if not LIBRARY.is_dir():
        print(f"library not found: {LIBRARY}", file=sys.stderr)
        print("from tools/gen_tone_table, clone it with:", file=sys.stderr)
        print(
            "  git clone https://github.com/Wohlstand/libOPNMIDI.git ../../extern/libOPNMIDI",
            file=sys.stderr,
        )
        return 1
    if len(GM_NAMES) != 128:
        print("internal GM table size mismatch", file=sys.stderr)
        return 1

    try:
        mapping = load_fmmidi_gm()
    except (FileNotFoundError, ValueError) as exc:
        print(exc, file=sys.stderr)
        return 1

    entries: list[str] = []
    for pc, inst in enumerate(mapping):
        entries.append(format_tone(pc, GM_NAMES[pc], inst, wopn_to_tone(inst)))

    header = format_file_header()
    OUTPUT.write_text(header + "\n" + "\n".join(entries) + "\n};\n", encoding="utf-8")
    print(f"wrote {OUTPUT} (128 programs from {SOURCE_BANK})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
