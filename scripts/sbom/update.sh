#!/usr/bin/env bash
# sbom/<board>.cdx.json と THIRD_PARTY_NOTICES.md を生成する。
#
#   scripts/sbom/update.sh                       全ボードをビルドして生成し、上書きする
#   scripts/sbom/update.sh --board pico2         指定ボードだけ（複数指定可）
#   scripts/sbom/update.sh --build-dir build     ビルド済みのディレクトリを使う（ボードは CMakeCache から）
#   scripts/sbom/update.sh --check ...           書き込まずに、コミット済みのファイルと比べる（CI 用）
#
# ビルドは build-sbom/<board>/base で行い、普段の build/ には触らない。--build-dir を
# 指定したときはそのディレクトリをそのまま使う（CMakeLists.txt の既定オプションでビルド済みであること）。
# 各 option() を 1 つずつ反転した configure を build-sbom/<board>/variant-<OPTION> で行い、
# ソース・インクルードディレクトリ・pio・リンカスクリプトの差分から構成への依存を判定する。
# 反転構成はビルドしない（configure だけで判定できる情報に限っているため）。
set -euo pipefail

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
GEN="$REPO/scripts/sbom/gen_sbom.py"
BOARDS_ALL=(pico2 pico)

check=0
boards=()
build_dir=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --check) check=1 ;;
        --board) boards+=("$2"); shift ;;
        --build-dir) build_dir="$2"; shift ;;
        -h|--help) sed -n '2,13p' "$0"; exit 0 ;;
        *) echo "unknown argument: $1" >&2; exit 2 ;;
    esac
    shift
done

if [[ -n "$build_dir" ]]; then
    build_dir="$(cd "$build_dir" && pwd)"
    boards=("$(python3 "$GEN" cache "$build_dir" PICO_BOARD)")
elif [[ ${#boards[@]} -eq 0 ]]; then
    boards=("${BOARDS_ALL[@]}")
fi

out_dir="$REPO/sbom"
notices="$REPO/THIRD_PARTY_NOTICES.md"
if [[ $check -eq 1 ]]; then
    work="$(mktemp -d)"
    trap 'rm -rf "$work"' EXIT
    # 対象外のボードはコミット済みの SBOM をそのまま NOTICES の入力に使う
    mkdir -p "$work/sbom"
    cp "$REPO"/sbom/*.cdx.json "$work/sbom/" 2>/dev/null || true
    out_dir="$work/sbom"
    notices="$work/THIRD_PARTY_NOTICES.md"
fi
mkdir -p "$out_dir"

# configure を静かに実行し、失敗したときだけログを出す
run_logged() {
    local log="$1"; shift
    if ! "$@" >"$log" 2>&1; then
        cat "$log" >&2
        echo "error: command failed: $*" >&2
        exit 1
    fi
}

last_base=""
for board in "${boards[@]}"; do
    work_root="$REPO/build-sbom/$board"
    mkdir -p "$work_root"
    if [[ -n "$build_dir" ]]; then
        base="$build_dir"
    else
        base="$work_root/base"
        echo "[$board] configure + build: $base"
        run_logged "$work_root/base-configure.log" \
            cmake -S "$REPO" -B "$base" -G Ninja -DPICO_BOARD="$board" -DCMAKE_BUILD_TYPE=Release
        run_logged "$work_root/base-build.log" cmake --build "$base" --parallel
    fi

    # 反転 configure には基準ビルドと同じ SDK・ツールの場所を渡す
    extra=()
    for var in PICO_SDK_PATH picotool_DIR CMAKE_BUILD_TYPE; do
        val="$(python3 "$GEN" cache "$base" "$var")"
        [[ -n "$val" ]] && extra+=("-D$var=$val")
    done

    variant_args=()
    while read -r opt flipped; do
        vdir="$work_root/variant-$opt"
        echo "[$board] configure $opt=$flipped"
        run_logged "$work_root/variant-$opt.log" \
            cmake -S "$REPO" -B "$vdir" -G Ninja -DPICO_BOARD="$board" "-D$opt=$flipped" ${extra[@]+"${extra[@]}"}
        variant_args+=(--variant "$opt=$vdir")
    done < <(python3 "$GEN" options)

    echo "[$board] generate sbom/$board.cdx.json"
    python3 "$GEN" sbom --build-dir "$base" "${variant_args[@]}" --out "$out_dir/$board.cdx.json"
    last_base="$base"
done

echo "generate THIRD_PARTY_NOTICES.md"
python3 "$GEN" notices --build-dir "$last_base" --sbom "$out_dir"/*.cdx.json --out "$notices"

if [[ $check -eq 1 ]]; then
    status=0
    for board in "${boards[@]}"; do
        if ! diff -u "$REPO/sbom/$board.cdx.json" "$out_dir/$board.cdx.json"; then
            status=1
        fi
    done
    if ! diff -u "$REPO/THIRD_PARTY_NOTICES.md" "$notices"; then
        status=1
    fi
    if [[ $status -ne 0 ]]; then
        echo "error: sbom/ or THIRD_PARTY_NOTICES.md is out of date. Run scripts/sbom/update.sh and commit the result." >&2
        exit 1
    fi
    echo "sbom/ and THIRD_PARTY_NOTICES.md are up to date."
fi
