"""outdated.py / impact.py が共通で使う、コミット済み SBOM の読み込みと git 操作。"""

import glob
import json
import os
import subprocess
import sys

REPO = os.path.realpath(os.path.join(os.path.dirname(__file__), "..", ".."))
PROP = "fmsynth:"
CACHE_DIR = os.path.join(REPO, ".cache", "sbom-repos")


def default_sbom_paths():
    return sorted(glob.glob(os.path.join(REPO, "sbom", "*.cdx.json")))


def _walk(components, parent=None):
    for c in components:
        yield c, parent
        yield from _walk(c.get("components", []), c)


def load_components(paths):
    """全ボードの SBOM を読み、コンポーネントごとにまとめた dict を返す。

    返り値: {key: {"name", "version", "vcs", "props", "parent",
                   "boards": {board: {"scope", "condition", "occurrences"}}}}
    """
    result = {}
    for path in paths:
        with open(path, encoding="utf-8") as f:
            bom = json.load(f)
        board = next((p["value"] for p in bom["metadata"].get("properties", [])
                      if p["name"] == PROP + "board"), os.path.basename(path))
        for c, parent in _walk(bom["components"]):
            key = c["bom-ref"].split(":", 1)[1]
            props = {p["name"][len(PROP):]: p["value"] for p in c.get("properties", [])
                     if p["name"].startswith(PROP)}
            vcs = next((r["url"] for r in c.get("externalReferences", []) if r["type"] == "vcs"), None)
            entry = result.setdefault(key, {
                "key": key, "name": c["name"], "version": c.get("version"),
                "vcs": vcs, "props": props, "boards": {},
                "parent": parent["bom-ref"].split(":", 1)[1] if parent else None,
            })
            entry["boards"][board] = {
                "scope": c.get("scope"),
                "condition": props.get("condition"),
                "occurrences": c.get("evidence", {}).get("occurrences", []),
            }
    return result


def is_used(entry):
    return any(b["scope"] in ("required", "optional") for b in entry["boards"].values())


def find_component(components, query):
    q = query.lower()
    for key, c in components.items():
        if key.lower() == q or c["name"].lower() == q:
            return c
    names = ", ".join(sorted(components))
    sys.exit(f"error: component '{query}' not found. Known keys: {names}")


def git(*args, cwd=None, check=True):
    r = subprocess.run(["git", *args], cwd=cwd, capture_output=True, text=True)
    if check and r.returncode != 0:
        sys.exit(f"error: git {' '.join(args)}\n{r.stderr.strip()}")
    return r.stdout


def mirror(key, url):
    """上流の bare clone（blob なし）を .cache/sbom-repos/ に用意して最新化する。

    submodule 自体には触らない。ファイル名の差分と履歴だけ使うので blob は取得しない。
    """
    path = os.path.join(CACHE_DIR, f"{key}.git")
    if not os.path.isdir(path):
        os.makedirs(CACHE_DIR, exist_ok=True)
        print(f"cloning {url} (tree only) ...", file=sys.stderr)
        git("clone", "--bare", "--filter=blob:none", "--quiet", url, path)
    else:
        git("-C", path, "fetch", "--quiet", "--force", "--tags", "--filter=blob:none",
            "origin", "+refs/heads/*:refs/heads/*")
    return path
