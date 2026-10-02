#!/usr/bin/env bash
# extern/submodules.lock に記録したコミットで submodule を取得し直すツール。
#
# 使い方:
#   scripts/restore-submodules.sh
#
# git init し直したリポジトリ（ZIP を展開して git init した場合を含む）では submodule の
# コミット ID が失われ、`git submodule update --init` では取得できない。このスクリプトは
# .gitmodules の URL と extern/submodules.lock のコミットから submodule を登録し直し、
# インデックスに追加する（コミットはしない）。登録済みの submodule も lock のコミットへ合わせる。
# 通常の clone では `git submodule update --init --recursive` で足りる。
set -euo pipefail

REPO_ROOT=$(git rev-parse --show-toplevel)
cd "$REPO_ROOT"

LOCK=extern/submodules.lock
[[ -f "$LOCK" ]] || { echo "error: $LOCK not found" >&2; exit 1; }
[[ -f .gitmodules ]] || { echo "error: .gitmodules not found" >&2; exit 1; }

# 最終行に改行が無くても読み落とさないよう、read 失敗時も path が残っていれば処理する
while read -r path commit _ <&3 || [[ -n "$path" ]]; do
  path="${path%$'\r'}"
  commit="${commit%$'\r'}"
  [[ -z "$path" || "$path" == \#* ]] && continue

  url=$(git config -f .gitmodules --get "submodule.${path}.url") || {
    echo "error: no url for '$path' in .gitmodules" >&2
    exit 1
  }

  echo "==> $path @ $commit"
  if [[ "$(git ls-files -s -- "$path")" == 160000\ * ]]; then
    git submodule update --init --quiet -- "$path"
  else
    if [[ -d "$path" && -n "$(ls -A "$path")" ]]; then
      echo "error: '$path' already has files but is not a registered submodule." >&2
      echo "       remove the directory and run this script again." >&2
      exit 1
    fi
    # GitHub の ZIP などは submodule の位置に空ディレクトリを残す。残っていると add が失敗する
    [[ -d "$path" ]] && rmdir "$path"
    git submodule add --force --quiet -- "$url" "$path"
  fi

  git -C "$path" checkout --quiet --detach "$commit"
  git -C "$path" submodule update --init --recursive --quiet
  git add -- "$path"
done 3< "$LOCK"

git add .gitmodules
echo "==> Done. Submodules are staged at the commits in $LOCK."
git submodule status
