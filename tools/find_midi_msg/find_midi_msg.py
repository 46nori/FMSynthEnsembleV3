import os
import sys
from collections import defaultdict

import mido

# ==============================================================================
# 【検出したいシーケンス（メッセージパターン）の設定】
# 
# あらゆるMIDIメッセージ（CC、ノート、SysEx、メタイベント）を組み合わせて指定できます。
# 
# 設定例（SysExの場合）:
#   {"type": "sysex", "data": (0x41, 0x10, 0x42, 0x12, 0x40, 0x00, 0x7F, 0x00, 0x41)}
#   ※ dataプロパティを指定する場合は [ ] ではなく ( ) （タプル）で記述してください。
# ==============================================================================
TARGET_SEQUENCE = [
    {"type": "control_change", "control": 99, "value": 1},  # NRPN MSB = 1
    {"type": "control_change", "control": 98, "value": 10} # NRPN LSB = 10
]


def match_message_criteria(msg, criteria):
    """MIDIメッセージが指定された条件（辞書）に合致するか判定する"""
    if msg.type != criteria["type"]:
        return False
        
    for key, expected_val in criteria.items():
        if key == "type":
            continue
        if not hasattr(msg, key) or getattr(msg, key) != expected_val:
            return False
            
    return True


def scan_midi_file(file_path):
    """1つのMIDIファイルをスキャンし、TARGET_SEQUENCEに一致すればファイルパスを出力する"""
    seq_len = len(TARGET_SEQUENCE)

    mid = mido.MidiFile(file_path)
    # 全トラックを絶対時間順にマージしてから走査する。
    # トラックごとに走査すると、対象シーケンスの各メッセージが
    # 別トラックに分かれて記録されている場合（Format 1 で起こり得る）に検出できないため。
    merged_track = mido.merge_tracks(mid.tracks)

    # チャンネル（0〜15）ごとに進捗を管理する。
    # channel属性を持たないSysEx/メタイベントは、実チャンネルと衝突しないよう
    # 専用キー（None）で管理する。
    channel_progress = defaultdict(int)

    for msg in merged_track:
        ch = getattr(msg, 'channel', None)
        current_step = channel_progress[ch]

        # 現在のステップの条件と一致するかチェック
        if match_message_criteria(msg, TARGET_SEQUENCE[current_step]):
            channel_progress[ch] += 1
            if channel_progress[ch] == seq_len:
                print(file_path)
                return
        else:
            # マッチしなかった場合、ステップ0の条件と再度一致するかチェック（取りこぼし防止）
            if match_message_criteria(msg, TARGET_SEQUENCE[0]):
                channel_progress[ch] = 1
            else:
                channel_progress[ch] = 0


def scan_midi_directory(target_dir):
    if not os.path.isdir(target_dir):
        print(f"エラー: 指定されたディレクトリが見つかりません: {target_dir}", file=sys.stderr)
        return

    if len(TARGET_SEQUENCE) == 0:
        print("エラー: TARGET_SEQUENCE が空です。", file=sys.stderr)
        return

    # 指定ディレクトリ以下を再帰的に走査
    for root, _, files in os.walk(target_dir):
        for file in files:
            if file.lower().endswith(('.mid', '.midi')):
                file_path = os.path.join(root, file)
                try:
                    scan_midi_file(file_path)
                except Exception:
                    pass


if __name__ == "__main__":
    if len(sys.argv) > 1:
        search_path = sys.argv[1]
    else:
        search_path = input("スキャンするディレクトリのパスを入力してください: ").strip()
        search_path = search_path.strip('"')

    scan_midi_directory(search_path)
