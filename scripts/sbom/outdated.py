#!/usr/bin/env python3
"""SBOM に載っている git 管理のコンポーネントについて、上流に新しいリリースがあるか調べる。

    scripts/sbom/outdated.py           ビルドに使われているコンポーネントだけ
    scripts/sbom/outdated.py --all     ビルド対象外（btstack など）も含める

上流への `git ls-remote` を使う（ネットワークが必要。リポジトリの状態は変えない）。
リリースタグは数字のバージョンを含むものとし、rc / beta / alpha などのプレリリースは除外する。
"""

import argparse
import re
from concurrent.futures import ThreadPoolExecutor

import sbom_io

PRERELEASE = re.compile(r"(rc|beta|alpha|pre|dev|test)", re.I)


def version_key(tag):
    """タグからバージョンの数値タプルを取り出す。リリースタグでなければ None。"""
    if PRERELEASE.search(tag):
        return None
    # STABLE-2_2_1_RELEASE（lwIP）のような区切りも数字列として扱う
    m = re.fullmatch(r"(?:v|V|release-|STABLE-)?(\d+(?:[._]\d+)*)(?:_RELEASE)?", tag)
    if not m:
        return None
    return tuple(int(x) for x in re.split(r"[._]", m.group(1)))


def remote_refs(url):
    out = sbom_io.git("ls-remote", "--tags", url)
    tags = {}
    for line in out.splitlines():
        sha, ref = line.split("\t")
        name = ref[len("refs/tags/"):]
        if name.endswith("^{}"):
            tags[name[:-3]] = sha          # 注釈付きタグはコミットを指す peeled 値を優先
        else:
            tags.setdefault(name, sha)
    head = sbom_io.git("ls-remote", url, "HEAD").split("\t")[0] or None
    return tags, head


def check(entry):
    commit = entry["props"].get("commit")
    try:
        tags, head = remote_refs(entry["vcs"])
    except SystemExit as e:
        return entry, None, None, f"取得失敗: {e}"
    releases = sorted(((version_key(t), t) for t in tags if version_key(t)), reverse=True)
    current = [t for t, sha in tags.items() if sha == commit]
    current_rel = sorted(((version_key(t), t) for t in current if version_key(t)), reverse=True)

    if not releases:
        if commit == head:
            return entry, "-", "-", "最新（既定ブランチの HEAD）"
        return entry, "-", head[:12] if head else "-", "既定ブランチに新しいコミットあり"

    latest_key, latest = releases[0]
    if current_rel:
        cur_key, cur = current_rel[0]
        status = "最新" if cur_key >= latest_key else "更新あり"
        return entry, cur, latest, status
    # リリースタグ以外のコミットを使っている（例: v3.6.2 から 16 コミット先）。
    # SBOM の version に含まれる数字のバージョンで比べる
    base = entry.get("version") or commit[:12]
    base_key = version_key_from_text(base)
    if base_key is None:
        status = "要確認（リリースタグ以外のコミット）"
    elif base_key >= latest_key:
        status = "最新（リリース後のコミット）"
    else:
        status = "更新あり"
    return entry, base, latest, status


def version_key_from_text(text):
    m = re.search(r"(\d+(?:\.\d+)+)", text or "")
    return tuple(int(x) for x in m.group(1).split(".")) if m else None


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--all", action="store_true", help="ビルド対象外のコンポーネントも含める")
    ap.add_argument("--sbom", nargs="+", default=None, help="既定は sbom/*.cdx.json")
    args = ap.parse_args()

    comps = sbom_io.load_components(args.sbom or sbom_io.default_sbom_paths())
    targets = [c for c in comps.values() if c["vcs"] and (args.all or sbom_io.is_used(c))]
    with ThreadPoolExecutor(max_workers=8) as ex:
        results = list(ex.map(check, targets))

    def managed_by(entry):
        # 親が別の git リポジトリなら、親の更新に合わせて上がるもの（例: pico-sdk 同梱の TinyUSB）
        parent = comps.get(entry["parent"]) if entry["parent"] else None
        while parent and not parent["vcs"]:
            parent = comps.get(parent["parent"]) if parent["parent"] else None
        return f"{parent['name']} が固定" if parent else "-"

    rows = [("コンポーネント", "現在", "最新リリース", "状態", "管理元")]
    for entry, cur, latest, status in results:
        rows.append((entry["name"], cur or "-", latest or "-", status, managed_by(entry)))
    widths = [max(len(r[i]) for r in rows) for i in range(len(rows[0]))]
    for i, r in enumerate(rows):
        print("  ".join(s.ljust(w) for s, w in zip(r, widths)).rstrip())
        if i == 0:
            print("  ".join("-" * w for w in widths))
    print("\n影響の確認: scripts/sbom/impact.py <component> <新しいタグ>")


if __name__ == "__main__":
    main()
