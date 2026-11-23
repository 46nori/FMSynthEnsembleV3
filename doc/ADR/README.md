# 設計判断の記録（ADR）

設計を決めたときの背景・選択肢・理由を残す。設計書（`design_*.md` / `spec_*.md`）は「現在どうなっているか」を書き、なぜそうしたかの経緯はここに書く。

## 一覧

| ADR | 内容 | 状態 |
|---|---|---|
| [0001](0001-single-midi-event-fifo.md) | 演奏イベントの Core 間転送を単一 FIFO にする | 採用 |
| [0002](0002-gm2-modulation-depth-range-unsupported.md) | GM2 RPN 0,0,5（Modulation Depth Range）をサポートしない | 採用 |
| [0003](0003-freertos-kernel-as-submodule.md) | FreeRTOS-Kernel を submodule で持ち、コミットを lock で固定する | 採用 |
| [0004](0004-exclude-sdio-driver.md) | no-OS-FatFS の SDIO ドライバをビルドから外す | 採用 |

## 書き方

- ファイル名は `NNNN-<kebab-case の要約>.md`。番号は通し番号で、再利用しない
- 見出しは次の 5 つにそろえる

```markdown
# ADR NNNN: <決定内容を一文で>

- 状態: 採用 / 廃止 / ADR NNNN で置き換え
- 日付: YYYY-MM-DD（関連する PR・Issue）

## 1. 背景
## 2. 決定
## 3. 根拠
## 4. 影響
## 5. 関連
```

- 採用した ADR の本文は書き換えない。判断を変えるときは新しい ADR を書き、古い方の状態を「ADR NNNN で置き換え」にする
- 計測値など判断の根拠になった数値は残してよい。デバッグの生ログは入れない
- 設計書からは、判断の要点を一文で書き、詳細は ADR へリンクする
