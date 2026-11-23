# ADR 0003: FreeRTOS-Kernel を submodule で持ち、コミットを lock で固定する

- 状態: 採用
- 日付: 2026-10-03（#PR127、Issue #126）

## 1. 背景

2026-05 に FreeRTOS-Kernel を submodule から外し、Pico 拡張や pico-sdk が提供する FreeRTOS-Kernel を使う構成に変えた。ところが新しいマシンで手順書どおりに環境を作ると、`Configure CMake` が「Official FreeRTOS-Kernel not found」で失敗した。

調べると、FreeRTOS-Kernel は pico-sdk にも Pico 拡張（0.23.0）にも含まれていなかった。CI が通っていたのは、FreeRTOS-Kernel がたまたま pico-sdk の隣に置かれていたためだった。

## 2. 決定

- FreeRTOS-Kernel を `extern/FreeRTOS-Kernel` の submodule に戻し、リリースタグ（V11.3.1）のコミットに固定する
- 4 つの submodule すべてのパスとコミット ID を `extern/submodules.lock` に記録し、Configure 時に `extern/check_submodules.cmake` が照合する。未取得、バージョン違い、lock への登録漏れのいずれかで Configure を止める
- `git init` し直したリポジトリや ZIP から始めた場合のために、lock のコミットで submodule を登録し直す `scripts/restore-submodules.sh` を用意する

## 3. 根拠

- 外部の配置に頼ると、環境ごとに FreeRTOS-Kernel の有無やバージョンが変わり、ビルドの再現性がない
- git が持つ submodule のコミット ID は履歴の中にしかなく、`git init` し直すと失われる。追跡対象のファイル（lock）にも同じ情報を置けば、どの状態からでも同じコミットに戻せる
- RP2350 用ポートは V11.2.0 以降に含まれる。V11.3.1 はそれを満たし、CI が以前指定していたコミットもこの範囲に含まれていた

## 4. 影響

- submodule のバージョンを変えるときは `extern/submodules.lock` も同じコミットに更新する必要がある
- RP2350 用ポートは FreeRTOS-Kernel 内の submodule（Community-Supported-Ports）にあるため、取得時は `--recursive` が必須
- 以前の構成で作ったビルドディレクトリは `FREERTOS_KERNEL_PATH` をキャッシュしているので、作り直す必要がある

## 5. 関連

- 手順と制約: [architecture.md](../architecture/architecture.md) の「外部ライブラリのバージョンを変える」
- ビルド手順: [build_ja.md](../build_ja.md)
