#!/usr/bin/env python3
"""依存ライブラリを新しいバージョンに上げたときに、変更がこのファームに影響しうるかを調べる。

    scripts/sbom/impact.py <component> <new-ref> [--from REF] [--list-unused]

    例: scripts/sbom/impact.py tinyusb 0.21.0
        scripts/sbom/impact.py freertos-kernel V11.3.3

<component> は SBOM の bom-ref のキー（tinyusb, freertos-kernel など）か名前。
現在のコミット（SBOM の fmsynth:commit）と <new-ref> の差分ファイルを、SBOM に記録された
ビルドで使われる項目と突き合わせ、次の 3 つに分ける。

    影響候補      コンパイルされるソース、pio、リンカスクリプト、インクルードディレクトリ配下のファイル、
                  コンパイルされるソースと同じディレクトリのファイル
    ビルド設定    CMakeLists.txt / *.cmake（ビルド対象そのものが変わりうる）
    対象外        上記以外（このファームのビルドでは使われない）

ヘッダはインクルードディレクトリ単位で判定するので、影響候補は実際より多めに出る（見落としはしない方向）。
上流の履歴は .cache/sbom-repos/ に blob なしの bare clone を作って調べる（submodule には触らない）。
"""

import argparse
import os
import posixpath
import sys

import sbom_io


def used_items(components, git_key):
    """git リポジトリ git_key に属する全コンポーネントの、ビルドで使われる項目を返す。

    返り値: [(repo 内のパス, kind, 説明)]
    説明は「ボード: 条件」の形。ボード間で同じ項目はまとめる。
    """
    items = {}
    for key, c in components.items():
        if key == git_key:
            prefix = ""
        elif c["props"].get("git-component") == git_key:
            prefix = c["props"].get("git-path", "")
        else:
            continue
        for board, b in c["boards"].items():
            for o in b["occurrences"]:
                kind, _, cond = o["additionalContext"].partition("; only when ")
                path = posixpath.normpath(posixpath.join(prefix, o["location"])) if prefix else o["location"]
                note = f"{board}" + (f" ({cond})" if cond else "")
                items.setdefault((path, kind), set()).add(note)
    return [(p, k, ", ".join(sorted(n))) for (p, k), n in sorted(items.items())]


# インクルードされうるファイル。拡張子なしは C++ 標準ヘッダ風の名前のため含める
HEADER_EXT = {".h", ".hh", ".hpp", ".hxx", ".inc", ".inl", ".def", ".tcc", ""}


def classify(path, items):
    """変更されたファイルの分類と、該当した項目の説明を返す。

    判定順: 項目と完全一致 → インクルードディレクトリ配下のヘッダ → コンパイル対象と同じディレクトリのヘッダ
    """
    for item, kind, note in items:
        if kind != "include-dir" and path == item:
            return "used", f"{note}: {kind}"
    base = posixpath.basename(path)
    if posixpath.splitext(base)[1] in HEADER_EXT:
        for item, kind, note in items:
            if kind == "include-dir" and (item == "." or path.startswith(item.rstrip("/") + "/")):
                return "used", f"{note}: include-dir {item}"
        for item, kind, note in items:
            if kind == "source" and posixpath.dirname(path) == posixpath.dirname(item):
                return "used", f"{note}: same directory as {item}"
    if base == "CMakeLists.txt" or base.endswith(".cmake"):
        return "build", ""
    return "unused", ""


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("component")
    ap.add_argument("new_ref", help="比較先のタグ・ブランチ・コミット")
    ap.add_argument("--from", dest="from_ref", help="比較元（既定は SBOM に記録された現在のコミット）")
    ap.add_argument("--sbom", nargs="+", default=None, help="既定は sbom/*.cdx.json")
    ap.add_argument("--list-unused", action="store_true", help="対象外の変更ファイルも一覧する")
    args = ap.parse_args()

    comps = sbom_io.load_components(args.sbom or sbom_io.default_sbom_paths())
    target = sbom_io.find_component(comps, args.component)
    if not target["vcs"]:
        owner = target["props"].get("git-component")
        hint = f"（git リポジトリは {owner}。そちらを指定する）" if owner else ""
        sys.exit(f"error: {target['name']} has no git repository of its own{hint}")

    repo = sbom_io.mirror(target["key"], target["vcs"])
    old = args.from_ref or target["props"].get("commit")
    if not old:
        sys.exit("error: current commit is unknown; pass --from")
    old_sha = sbom_io.git("-C", repo, "rev-parse", "--verify", f"{old}^{{commit}}").strip()
    new_sha = sbom_io.git("-C", repo, "rev-parse", "--verify", f"{args.new_ref}^{{commit}}").strip()

    items = used_items(comps, target["key"])
    diff = sbom_io.git("-C", repo, "diff", "--name-status", "--no-renames", old_sha, new_sha)
    groups = {"used": [], "build": [], "unused": [], "submodule": []}
    tree = sbom_io.git("-C", repo, "ls-tree", "-r", new_sha)
    gitlinks = {line.split("\t", 1)[1] for line in tree.splitlines() if line.startswith("160000 ")}
    for line in diff.splitlines():
        status, path = line.split("\t", 1)
        if path in gitlinks:
            groups["submodule"].append((status, path, ""))
            continue
        group, why = classify(path, items)
        groups[group].append((status, path, why))

    print(f"{target['name']}: {old_sha[:12]} ({target.get('version') or '-'}) -> {new_sha[:12]} ({args.new_ref})")
    changed = sum(len(v) for v in groups.values())
    print(f"変更ファイル {changed} 件: 影響候補 {len(groups['used'])} / ビルド設定 {len(groups['build'])} "
          f"/ submodule {len(groups['submodule'])} / 対象外 {len(groups['unused'])}")
    if not any(b["scope"] != "excluded" for b in target["boards"].values()):
        print("注意: このコンポーネントはどのボードでもビルド対象外。")

    def show(title, rows, with_why=True):
        if not rows:
            return
        print(f"\n## {title}")
        for status, path, why in rows:
            print(f"  {status}  {path}" + (f"    [{why}]" if with_why and why else ""))

    show("影響候補", groups["used"])
    show("ビルド設定（ビルド対象が変わりうる）", groups["build"])
    show("submodule の更新（そのコンポーネントで impact.py を実行する）", groups["submodule"])
    if args.list_unused:
        show("対象外", groups["unused"], with_why=False)

    paths = [p for _, p, _ in groups["used"] + groups["build"]]
    if paths:
        log = sbom_io.git("-C", repo, "log", "--oneline", "--no-decorate", f"{old_sha}..{new_sha}", "--", *paths)
        print("\n## 影響候補・ビルド設定に触れたコミット")
        print(log.rstrip() or "  (なし)")


if __name__ == "__main__":
    main()
