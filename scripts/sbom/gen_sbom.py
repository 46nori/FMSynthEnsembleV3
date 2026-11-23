#!/usr/bin/env python3
"""ビルドディレクトリから CycloneDX 1.6 の SBOM と THIRD_PARTY_NOTICES.md を生成する。

    gen_sbom.py sbom    --build-dir DIR [--variant OPTION=DIR ...] --out FILE
    gen_sbom.py notices --build-dir DIR --sbom FILE [FILE ...] --out FILE
    gen_sbom.py options                    (update.sh 用の補助)
    gen_sbom.py cache DIR NAME             (update.sh 用の補助)

sbom:
    --build-dir はビルド済み（Ninja）で、全オプションが CMakeLists.txt の既定値のもの。
    --variant はオプションを 1 つだけ既定値から反転して configure だけ行ったディレクトリ。
    コンパイル対象のソース・インクルードディレクトリ・pio・リンカスクリプトの差分から、
    各項目・コンポーネントがどのオプションに依存するかを判定する。
    ツールチェーンのライブラリはリンカ map から判定するので、既定構成のみが対象。

notices:
    SBOM でビルド対象（scope が required / optional）のコンポーネントについて、
    catalog.py の license_texts からライセンス本文を集める。--build-dir は pico-sdk の場所を知るために使う。

出力は決定的にする（タイムスタンプ・serialNumber・絶対パスを含めない）。CI はコミット済みの
ファイルと再生成結果を比べるため。
"""

import argparse
import json
import os
import re
import shlex
import subprocess
import sys
from collections import Counter

import catalog

REPO = os.path.realpath(os.path.join(os.path.dirname(__file__), "..", ".."))
PROP = "fmsynth:"


# ---------------------------------------------------------------------- 入力の読み取り

def read_cmake_cache(build_dir):
    path = os.path.join(build_dir, "CMakeCache.txt")
    cache = {}
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = re.match(r"^([A-Za-z0-9_.+-]+):[A-Z]+=(.*)$", line.rstrip("\n"))
            if m:
                cache[m.group(1)] = m.group(2)
    return cache


def option_defaults():
    """ルート CMakeLists.txt の fmse_build_option()（または option()）宣言から既定値を得る。"""
    with open(os.path.join(REPO, "CMakeLists.txt"), encoding="utf-8") as f:
        text = f.read()
    opts = {}
    for m in re.finditer(r'^\s*(?:fmse_build_)?option\(\s*(\w+)\s+"[^"]*"\s+(ON|OFF)\s*\)', text, re.M):
        opts[m.group(1)] = m.group(2)
    return opts


def cache_bool(value):
    return "ON" if value.upper() in ("ON", "1", "TRUE", "YES", "Y") else "OFF"


def compile_db(build_dir):
    """compile_commands.json からコンパイル対象のソースとインクルードディレクトリを返す。"""
    with open(os.path.join(build_dir, "compile_commands.json"), encoding="utf-8") as f:
        entries = json.load(f)
    sources, include_dirs = set(), set()
    for e in entries:
        p = e["file"]
        if not os.path.isabs(p):
            p = os.path.join(e["directory"], p)
        sources.add(os.path.realpath(p))
        args = e["arguments"] if "arguments" in e else shlex.split(e["command"])
        for i, tok in enumerate(args):
            for flag in ("-I", "-isystem", "-iquote"):
                if tok == flag and i + 1 < len(args):
                    d = args[i + 1]
                elif tok.startswith(flag) and len(tok) > len(flag):
                    d = tok[len(flag):]
                else:
                    continue
                if not os.path.isabs(d):
                    d = os.path.join(e["directory"], d)
                include_dirs.add(os.path.realpath(d))
                break
    return sources, include_dirs


def ninja(build_dir, *args):
    return subprocess.run(["ninja", "-C", build_dir, *args], check=True,
                          capture_output=True, text=True).stdout


def linked_pio(build_dir, elf, pio_files):
    """pio_files のうち、プログラムがファームにリンクされた .pio を返す。

    pico_generate_pio_header() はヘッダを生成するだけなので、どこからも使われない pio
    プログラムはファームに入らない（例: SDIO ドライバをビルドから外したあとの rp2040_sdio.pio）。
    pioasm は .program <name> ごとに <name>_program を static const で生成するので、
    そのシンボルが ELF にあるかで判定する（C++ から使うと _ZL<len> 付きの名前になる）。
    """
    nm = read_cmake_cache(build_dir).get("CMAKE_NM") or "nm"
    out = subprocess.run([nm, os.path.join(build_dir, elf)], check=True,
                         capture_output=True, text=True).stdout
    symbols = {re.sub(r"^_ZL\d+", "", l.split()[-1]) for l in out.splitlines() if l.strip()}
    result = set()
    for path in pio_files:
        with open(path, encoding="utf-8", errors="replace") as f:
            names = re.findall(r"^\s*\.program\s+(\w+)", f.read(), re.M)
        if any(f"{n}_program" in symbols for n in names):
            result.add(path)
    return result


def build_inputs(build_dir, elf, suffixes):
    """ファームウェアのビルドグラフに含まれる入力のうち、指定拡張子のものを返す（ビルド不要）。"""
    out = ninja(build_dir, "-t", "inputs", elf)
    return {os.path.realpath(l.strip()) for l in out.splitlines() if l.strip().endswith(suffixes)}


def linker_scripts(build_dir, elf):
    """リンクコマンドの -T / --script と、そこから INCLUDE されるスクリプトを返す。"""
    cmd = ninja(build_dir, "-t", "commands", elf).strip().splitlines()[-1]
    search, roots = [], []
    for tok in cmd.split():
        tok = tok[len("-Wl,"):] if tok.startswith("-Wl,") else tok
        if tok.startswith("-L"):
            search.append(tok[2:])
        elif tok.startswith("-T"):
            roots.append(tok[2:])
        elif tok.startswith("--script="):
            roots.append(tok[len("--script="):])
    seen = set()
    stack = [os.path.realpath(r) for r in roots]
    while stack:
        path = stack.pop()
        if path in seen or not os.path.isfile(path):
            continue
        seen.add(path)
        with open(path, encoding="utf-8", errors="replace") as f:
            text = f.read()
        for name in re.findall(r'^\s*INCLUDE\s+"?([^"\s]+)"?', text, re.M):
            for d in [os.path.dirname(path)] + search:
                cand = os.path.join(d, name)
                if os.path.isfile(cand):
                    stack.append(os.path.realpath(cand))
                    break
    return seen


def linked_archives(map_path):
    """リンカ map の "Archive member included" 節から、メンバーが取り込まれたアーカイブを返す。"""
    archives = set()
    in_section = False
    with open(map_path, encoding="utf-8", errors="replace") as f:
        for line in f:
            if line.startswith("Archive member included"):
                in_section = True
                continue
            if in_section and re.match(r"^[A-Z][a-z]", line):
                break
            if in_section:
                m = re.match(r"^(\S+\.a)\(", line)
                if m:
                    archives.add(os.path.realpath(m.group(1)) if os.path.isabs(m.group(1)) else m.group(1))
    return archives


# ---------------------------------------------------------------------- 環境とコンポーネント

class Env:
    def __init__(self, build_dir):
        self.build_dir = os.path.realpath(build_dir)
        self.cache = read_cmake_cache(build_dir)
        sdk = self.cache.get("PICO_SDK_PATH") or os.environ.get("PICO_SDK_PATH")
        if not sdk:
            sys.exit("error: PICO_SDK_PATH is not found in CMakeCache.txt")
        self.roots = {"repo": REPO, "sdk": os.path.realpath(sdk)}
        compiler = self.cache.get("CMAKE_C_COMPILER", "")
        self.roots["toolchain"] = os.path.realpath(os.path.join(os.path.dirname(compiler), "..")) if compiler else ""
        self.lock = read_lock()

    def root_of(self, comp):
        base, rel = comp["root"]
        return os.path.join(self.roots[base], rel) if rel else self.roots[base]

    def compiler_version(self):
        for d in sorted(os.listdir(os.path.join(self.build_dir, "CMakeFiles"))):
            p = os.path.join(self.build_dir, "CMakeFiles", d, "CMakeCCompiler.cmake")
            if os.path.isfile(p):
                with open(p, encoding="utf-8") as f:
                    m = re.search(r'CMAKE_C_COMPILER_VERSION "([^"]+)"', f.read())
                if m:
                    return m.group(1)
        return None


def read_lock():
    lock = {}
    with open(os.path.join(REPO, "extern", "submodules.lock"), encoding="utf-8") as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            parts = line.split(None, 2)
            if len(parts) >= 2:
                lock[parts[0]] = (parts[1], parts[2].strip() if len(parts) > 2 else "")
    return lock


def read_text(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        return f.read()


def component_version(env, comp):
    spec = comp.get("version")
    if not spec:
        return None
    root = env.root_of(comp)
    kind = spec[0]
    if kind == "lock":
        return env.lock.get(comp["root"][1], (None, None))[1] or None
    if kind == "sdk":
        text = read_text(os.path.join(root, "pico_sdk_version.cmake"))
        parts = [re.search(rf"set\(PICO_SDK_VERSION_{k} (\d+)\)", text).group(1)
                 for k in ("MAJOR", "MINOR", "REVISION")]
        return ".".join(parts)
    if kind == "macros":
        text = read_text(os.path.join(root, spec[1]))
        nums = []
        for name in spec[2]:
            m = re.search(rf"#define\s+{re.escape(name)}\s+\(?\s*(\d+)", text)
            if not m:
                return None
            nums.append(m.group(1))
        return ".".join(nums)
    if kind == "regex":
        path = os.path.join(root, spec[1])
        if not os.path.isfile(path):
            return None
        m = re.search(spec[2], read_text(path), re.M)
        return m.group(1) if m else None
    if kind == "compiler":
        return env.compiler_version()
    raise ValueError(f"unknown version spec: {spec}")


def git_commit(path):
    if not os.path.exists(os.path.join(path, ".git")):
        return None
    r = subprocess.run(["git", "-C", path, "rev-parse", "HEAD"], capture_output=True, text=True)
    return r.stdout.strip() if r.returncode == 0 else None


def github_slug(url):
    m = re.match(r"https://github\.com/([^/]+/[^/]+?)(?:\.git)?$", url)
    return m.group(1) if m else None


def licenses_of(comp):
    if comp.get("license_name"):
        return [{"license": {"name": comp["license_name"], "acknowledgement": "declared"}}]
    ids = comp.get("licenses", [])
    if len(ids) == 1 and re.search(r"\s(AND|OR|WITH)\s", ids[0]):
        return [{"expression": ids[0], "acknowledgement": "declared"}]
    return [{"license": {"id": i, "acknowledgement": "declared"}} for i in ids]


def file_license_marker(path):
    try:
        with open(path, encoding="utf-8", errors="replace") as f:
            head = f.read(4096)
    except OSError:
        return "unreadable"
    m = re.search(r"SPDX-License-Identifier:\s*([^\n*]+?)\s*(?:\*/)?\s*$", head, re.M)
    if m:
        return m.group(1).strip()
    if "Permission is hereby granted, free of charge" in head:
        return "MIT (text)"
    return "none"


# ---------------------------------------------------------------------- sbom サブコマンド

def analyze_config(build_dir, elf):
    """configure 済みのディレクトリから、ビルドに使われるファイルとディレクトリ {path: kind} を返す。

    ヘッダはファイル単位でなくインクルードディレクトリで記録する。ソースの #include を
    1 行変えただけで SBOM が変わらないようにするためで、どちらも CMake の構成だけで決まる。
    """
    sources, include_dirs = compile_db(build_dir)
    items = {d: "include-dir" for d in include_dirs}
    items.update({f: "source" for f in sources})
    items.update({f: "pio" for f in build_inputs(build_dir, elf, (".pio",))})
    scripts = build_inputs(build_dir, elf, (".ld",)) | linker_scripts(build_dir, elf)
    items.update({f: "linker-script" for f in scripts})
    return items


def check_catalog_coverage(env):
    """submodule が catalog.py に登録されているか確かめる。

    未登録の submodule のファイルは本体のコードと区別できず、SBOM から黙って漏れるため。
    """
    known = {c["root"] for c in catalog.COMPONENTS}
    missing = [p for p in env.lock if ("repo", p) not in known]
    gitmodules = os.path.join(env.roots["sdk"], ".gitmodules")
    if os.path.isfile(gitmodules):
        for p in re.findall(r"^\s*path\s*=\s*(\S+)", read_text(gitmodules), re.M):
            if ("sdk", p) not in known:
                missing.append(f"<pico-sdk>/{p}")
    if missing:
        sys.exit("error: not registered in scripts/sbom/catalog.py: " + ", ".join(missing))


def build_sbom(args):
    env = Env(args.build_dir)
    cache = env.cache
    defaults = option_defaults()

    for name, value in defaults.items():
        actual = cache_bool(cache.get(name, value))
        if actual != value:
            sys.exit(f"error: {args.build_dir}: {name}={actual}, but the SBOM is defined for the "
                     f"CMakeLists.txt default ({name}={value}). Use a build directory with default options.")

    check_catalog_coverage(env)
    elf = f"{cache.get('CMAKE_PROJECT_NAME', catalog.PROJECT['name'])}.elf"
    toolchain = env.roots["toolchain"]
    base_files = analyze_config(env.build_dir, elf)
    # プログラムがリンクされていない pio は使われていない。リンク結果はビルド済みの
    # 基準構成でしか分からないので、ここで判定して反転構成にも同じ除外を適用する
    base_pio = {f for f, kind in base_files.items() if kind == "pio"}
    unused_pio = base_pio - linked_pio(env.build_dir, elf, base_pio)
    for f in unused_pio:
        del base_files[f]
    # ツールチェーンのライブラリはリンク結果からしか分からないので、既定構成のビルドだけで判定する
    map_path = os.path.join(env.build_dir, f"{elf}.map")
    if not os.path.isfile(map_path):
        sys.exit(f"error: {map_path} not found. Build the firmware first.")
    base_archives = {os.path.basename(a) for a in linked_archives(map_path)
                     if toolchain and a.startswith(toolchain + os.sep)}

    # variants[opt] = (反転後の値, files)
    variants = {}
    for spec in args.variant or []:
        opt, _, d = spec.partition("=")
        if opt not in defaults:
            sys.exit(f"error: unknown option in --variant: {opt}")
        flipped = "OFF" if defaults[opt] == "ON" else "ON"
        if cache_bool(read_cmake_cache(d).get(opt, defaults[opt])) != flipped:
            sys.exit(f"error: {d}: {opt} is not {flipped}")
        vfiles = analyze_config(os.path.realpath(d), elf)
        for f in unused_pio:
            vfiles.pop(f, None)
        variants[opt] = (flipped, vfiles)

    comps = [dict(c) for c in catalog.COMPONENTS]
    by_key = {c["key"]: c for c in comps}
    file_roots = sorted(((os.path.realpath(env.root_of(c)), c["key"]) for c in comps
                         if c["root"][0] != "toolchain"), key=lambda x: -len(x[0]))
    archive_owner = {a: c["key"] for c in comps for a in c.get("archives", [])}

    def owner(path):
        for root, key in file_roots:
            if path == root or path.startswith(root + os.sep):
                return key, os.path.relpath(path, root)
        return None, None

    # 各構成で使われた項目を (コンポーネント, 相対パス) に揃える
    def items(files):
        result = {}
        for f, kind in files.items():
            key, rel = owner(f)
            if key:
                result[(key, rel)] = kind
        return result

    base_items = items(base_files)
    var_items = {o: items(v[1]) for o, v in variants.items()}

    # occ[key][rel] = (kind, 条件)。条件 None は全構成で使われることを表す
    occ = {c["key"]: {} for c in comps}
    for (key, rel), kind in base_items.items():
        missing = [o for o in variants if (key, rel) not in var_items[o]]
        occ[key][rel] = (kind, " AND ".join(f"{o}={defaults[o]}" for o in missing) or None)
    for o, vi in var_items.items():
        for (key, rel), kind in vi.items():
            if (key, rel) in base_items:
                continue
            only = [p for p in variants if (key, rel) in var_items[p]]
            occ[key][rel] = (kind, " OR ".join(f"{p}={variants[p][0]}" for p in only))
    for a in sorted(base_archives):
        if a in archive_owner:
            occ[archive_owner[a]][a] = ("archive", None)

    def presence(key):
        default = any(k == key for k, _ in base_items)
        per_variant = {o: any(k == key for k, _ in vi) for o, vi in var_items.items()}
        return default, per_variant

    def scope_and_condition(key):
        if by_key[key]["root"][0] == "toolchain":
            if occ[key]:
                return "required", "linked in the default configuration"
            return "excluded", None
        default, per_variant = presence(key)
        if default:
            absent = [o for o, p in per_variant.items() if not p]
            return "required", " AND ".join(f"{o}={defaults[o]}" for o in absent) or "always"
        only = [o for o, p in per_variant.items() if p]
        if only:
            return "optional", " OR ".join(f"{o}={variants[o][0]}" for o in only)
        return "excluded", None

    def git_owner(key):
        c = by_key[key]
        while c and not c.get("git"):
            c = by_key.get(c.get("parent"))
        return c

    def to_component(c):
        key = c["key"]
        root = env.root_of(c)
        out = {
            "type": "library",
            "bom-ref": f"component:{key}",
            "supplier": {"name": c["supplier"]},
            "name": c["name"],
        }
        version = component_version(env, c)
        if version:
            out["version"] = version

        props = []
        is_toolchain = c["root"][0] == "toolchain"
        scope, condition = scope_and_condition(key)

        # 自身のファイルが使われなくても、子が使われていれば親もビルドに含まれる
        children = [to_component(ch) for ch in comps if ch.get("parent") == key]
        if scope != "required":
            used = [ch for ch in children if ch["scope"] != "excluded"]
            if used:
                scope = "required" if any(ch["scope"] == "required" for ch in used) else "optional"
                conds = sorted({p["value"] for ch in used for p in ch["properties"]
                                if p["name"] == PROP + "condition"})
                condition = "always" if "always" in conds else " OR ".join(conds)
        out["scope"] = scope
        out["licenses"] = licenses_of(c)

        commit = git_commit(root) if c.get("git") else None
        if c.get("git"):
            slug = github_slug(c["git"])
            out["purl"] = f"pkg:github/{slug}@{commit}" if commit else f"pkg:github/{slug}"
            out["externalReferences"] = [{"type": "vcs", "url": c["git"]}]
        elif c.get("purl"):
            out["purl"] = f"{c['purl']}@{version}" if version and c["purl"].startswith("pkg:generic/") else c["purl"]

        base, rel = c["root"]
        props.append((PROP + "root", f"{base}:{rel}" if rel else base))
        if commit:
            props.append((PROP + "commit", commit))
        g = git_owner(key)
        if g and g is not c:
            props.append((PROP + "git-component", g["key"]))
            props.append((PROP + "git-path", os.path.relpath(root, env.root_of(g))))
        if condition:
            props.append((PROP + "condition", condition))
        if c.get("review"):
            props.append((PROP + "license-review", c["review"]))

        occurrences = []
        markers = Counter()
        for rel_path in sorted(occ[key]):
            kind, cond = occ[key][rel_path]
            ctx = kind + (f"; only when {cond}" if cond else "")
            occurrences.append({"location": rel_path, "additionalContext": ctx})
            if kind in ("source", "pio", "linker-script"):
                markers[file_license_marker(os.path.join(root, rel_path))] += 1
        if markers:
            props.append((PROP + "source-licenses",
                          ", ".join(f"{k}: {v}" for k, v in sorted(markers.items()))))
        out["properties"] = [{"name": n, "value": v} for n, v in props]
        if occurrences:
            out["evidence"] = {"occurrences": occurrences}

        if c.get("ancestors"):
            out["pedigree"] = {"ancestors": [{
                "type": "library",
                "bom-ref": f"ancestor:{key}:{a['name']}",
                "supplier": {"name": a["supplier"]},
                "name": a["name"],
                "purl": a["purl"],
                "licenses": licenses_of(a),
            } for a in c["ancestors"]]}

        if children:
            out["components"] = children
        return out

    top = [to_component(c) for c in comps if not c.get("parent")]

    meta_props = [
        {"name": PROP + "board", "value": cache.get("PICO_BOARD", "")},
        {"name": PROP + "platform", "value": cache.get("PICO_PLATFORM", "")},
    ] + [{"name": f"{PROP}option:{k}", "value": v} for k, v in sorted(defaults.items())] \
      + [{"name": PROP + "variants-analyzed", "value": ", ".join(sorted(variants))}]

    bom = {
        "$schema": "http://cyclonedx.org/schema/bom-1.6.schema.json",
        "bomFormat": "CycloneDX",
        "specVersion": "1.6",
        "version": 1,
        "metadata": {
            "tools": {"components": [{
                "type": "application",
                "name": "scripts/sbom/gen_sbom.py",
            }]},
            "component": {
                "type": "firmware",
                "bom-ref": "firmware",
                "supplier": {"name": catalog.PROJECT["supplier"]},
                "name": catalog.PROJECT["name"],
                "licenses": licenses_of(catalog.PROJECT),
            },
            "properties": meta_props,
        },
        "components": top,
        "dependencies": [{
            "ref": "firmware",
            "dependsOn": [c["bom-ref"] for c in top if c["scope"] != "excluded"],
        }],
    }
    write_text(args.out, json.dumps(bom, indent=2, ensure_ascii=False) + "\n")


# ---------------------------------------------------------------------- notices サブコマンド

def leading_comment(path):
    """ファイル先頭のコメントブロック（// 連続行か /* ... */）を返す。"""
    text = read_text(path).lstrip("﻿")
    m = re.match(r"\s*(/\*.*?\*/)", text, re.S)
    if m:
        return m.group(1)
    lines = []
    for line in text.splitlines():
        if line.lstrip().startswith("//"):
            lines.append(line)
        elif lines:
            break
    return "\n".join(lines)


def license_text(env, comp, spec):
    root = env.root_of(comp)
    if isinstance(spec, tuple):
        kind, value = spec
        if kind == "inline":
            return value
        if kind == "header":
            return leading_comment(os.path.join(root, value))
        raise ValueError(spec)
    return read_text(os.path.normpath(os.path.join(root, spec)))


def walk(components):
    for c in components:
        yield c
        yield from walk(c.get("components", []))


def build_notices(args):
    env = Env(args.build_dir)
    used = {}
    for path in args.sbom:
        with open(path, encoding="utf-8") as f:
            bom = json.load(f)
        board = next((p["value"] for p in bom["metadata"].get("properties", [])
                      if p["name"] == PROP + "board"), os.path.basename(path))
        for c in walk(bom["components"]):
            if c.get("scope") in ("required", "optional"):
                key = c["bom-ref"].split(":", 1)[1]
                used.setdefault(key, {"versions": set(), "boards": set()})
                used[key]["versions"].add(c.get("version", ""))
                used[key]["boards"].add(board)

    out = [
        "# Third-Party Notices",
        "",
        f"{catalog.PROJECT['name']} のファームウェアに含まれるサードパーティソフトウェアの一覧と",
        "ライセンス表記。`scripts/sbom/update.sh` が `sbom/*.cdx.json` から生成する（手で編集しない）。",
        "",
        "| コンポーネント | バージョン | ライセンス | 対象ボード | 備考 |",
        "|---|---|---|---|---|",
    ]
    comps = [c for c in catalog.COMPONENTS if c["key"] in used]
    for c in comps:
        u = used[c["key"]]
        lic = c.get("license_name") or " / ".join(c.get("licenses", []))
        versions = ", ".join(sorted(v for v in u["versions"] if v)) or "-"
        note = "要確認: 下記参照" if c.get("review") else ""
        out.append(f"| {c['name']} | {versions} | {lic} | {', '.join(sorted(u['boards']))} | {note} |")
    out.append("")

    for c in comps:
        out += [f"## {c['name']}", ""]
        if c.get("review"):
            out += [f"> {c['review']}", ""]
        texts = list(c.get("license_texts", []))
        for spec in texts:
            out += ["```text", license_text(env, c, spec).rstrip("\n"), "```", ""]
        for a in c.get("ancestors", []):
            for spec in a.get("license_texts", []):
                out += [f"Derived from {a['name']}:", "",
                        "```text", license_text(env, c, spec).rstrip("\n"), "```", ""]
        if not texts and not c.get("ancestors"):
            parent = c.get("parent")
            out += [f"親コンポーネント（{parent}）のライセンスに従う。" if parent else "ライセンス本文なし。", ""]
    write_text(args.out, "\n".join(out).rstrip("\n") + "\n")


def write_text(path, text):
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("sbom")
    s.add_argument("--build-dir", required=True)
    s.add_argument("--variant", action="append", metavar="OPTION=DIR")
    s.add_argument("--out", required=True)
    n = sub.add_parser("notices")
    n.add_argument("--build-dir", required=True)
    n.add_argument("--sbom", nargs="+", required=True)
    n.add_argument("--out", required=True)
    o = sub.add_parser("options", help="print 'NAME FLIPPED_VALUE' for each option() in CMakeLists.txt")
    c = sub.add_parser("cache", help="print one CMakeCache.txt value")
    c.add_argument("build_dir")
    c.add_argument("name")
    args = ap.parse_args()
    if args.cmd == "sbom":
        build_sbom(args)
    elif args.cmd == "notices":
        build_notices(args)
    elif args.cmd == "options":
        for k, v in option_defaults().items():
            print(k, "OFF" if v == "ON" else "ON")
    else:
        print(read_cmake_cache(args.build_dir).get(args.name, ""))


if __name__ == "__main__":
    main()
