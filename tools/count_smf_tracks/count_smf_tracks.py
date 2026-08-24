import os
import sys
from collections import Counter

import mido

# ==============================================================================
# 【集計設定】
#
# この件数を超えるトラック数のファイルを「要注意」として一覧の末尾にまとめる。
# SmfPlayerTask がトラックごとに保持する FatFs ハンドル数の上限（FF_FS_LOCK）を
# 何件に設定すべきか、手持ちファイルの実測から検討するためのしきい値。
# ==============================================================================
THRESHOLD = 17


def scan_midi_file(file_path):
    """1つのMIDIファイルの (フォーマット種別, トラック数) を返す"""
    mid = mido.MidiFile(file_path)
    return mid.type, len(mid.tracks)


def print_report(results, scanned_count, fail_count, interrupted):
    """集計結果を表示する（正常終了時・中断時のどちらからも呼ぶ）"""
    if interrupted:
        print(f"\n中断されました。ここまでの集計（{scanned_count}件走査、{fail_count}件読み込み失敗）を表示します。",
              file=sys.stderr)

    if not results:
        print("MIDIファイルが見つかりませんでした。")
        return

    # トラック数が多い順に一覧表示
    results.sort(key=lambda r: r[0], reverse=True)
    print(f"{'Tracks':>6}  {'Fmt':>3}  ファイル")
    for n_tracks, fmt, file_path in results:
        print(f"{n_tracks:>6}  {fmt:>3}  {file_path}")

    # トラック数のヒストグラム
    print()
    print("トラック数の分布:")
    histogram = Counter(n_tracks for n_tracks, _, _ in results)
    for n_tracks in sorted(histogram.keys()):
        count = histogram[n_tracks]
        marker = " *" if n_tracks > THRESHOLD else ""
        print(f"  {n_tracks:>3} トラック: {count:>4} 件{marker}")

    # しきい値超過ファイル
    over_threshold = [r for r in results if r[0] > THRESHOLD]
    max_tracks = results[0][0]

    print()
    print(f"走査ファイル数: {len(results)}（読み込み失敗 {fail_count}件）")
    print(f"最大トラック数: {max_tracks}")
    print(f"トラック数が{THRESHOLD}を超えるファイル: {len(over_threshold)}件（上の分布表で * を付けた行）")
    if over_threshold:
        for n_tracks, fmt, file_path in over_threshold:
            print(f"  {n_tracks:>3}  {file_path}")


def scan_midi_directory(target_dir):
    if not os.path.isdir(target_dir):
        print(f"エラー: 指定されたディレクトリが見つかりません: {target_dir}", file=sys.stderr)
        return

    results = []
    scanned_count = 0
    fail_count = 0
    interrupted = False

    try:
        # 指定ディレクトリ以下を再帰的に走査
        for root, _, files in os.walk(target_dir):
            for file in files:
                if not file.lower().endswith(('.mid', '.midi')):
                    continue
                file_path = os.path.join(root, file)
                try:
                    fmt, n_tracks = scan_midi_file(file_path)
                    results.append((n_tracks, fmt, file_path))
                    scanned_count += 1
                except Exception as e:
                    print(f"読み込み失敗: {file_path} ({e})", file=sys.stderr)
                    fail_count += 1

                # 進捗表示（同じ行を上書き。件数が多いディレクトリでも
                # スキャンが進んでいることが分かるようにする）
                print(f"\rスキャン中: {scanned_count}件（失敗 {fail_count}件） {file_path}",
                      end="", file=sys.stderr, flush=True)
    except KeyboardInterrupt:
        interrupted = True

    print(file=sys.stderr)  # 進捗表示の行を確定させる
    print_report(results, scanned_count, fail_count, interrupted)


if __name__ == "__main__":
    if len(sys.argv) > 1:
        search_path = sys.argv[1]
    else:
        search_path = input("スキャンするディレクトリのパスを入力してください: ").strip()
        search_path = search_path.strip('"')

    scan_midi_directory(search_path)
