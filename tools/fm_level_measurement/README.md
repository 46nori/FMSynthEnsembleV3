# FM Program Level Measurement

FM音色ごとの音量差を測定し、Program別のTL補正テーブルを作るためのツール。

この手順は macOS + Behringer U-PHORIA UMC204HD を想定する。

## 構成

```text
Mac
  USB -> FMSynthEnsembleV3        MIDI送信用
  USB -> UMC204HD                 録音用

FMSynthEnsembleV3 audio out L/R
  -> UMC204HD INPUT 1/2
```

UMC204HDの入力は以下を推奨する。

- `+48V`: OFF
- `INST`: OFF、LINE入力として使う
- `PAD`: クリップする場合だけON
- `MIX`: 測定値には影響しないが、確認用には `IN` 側でよい
- `GAIN 1/2`: 左右で同じ位置にし、測定中は動かさない

FMSynthEnsembleV3がステレオ3.5mm出力の場合は、`3.5mm TRS stereo -> 6.3mm TS x2` などで INPUT 1/2 に入れる。モノラル測定でも使えるが、最終的にステレオ出力で使うならステレオ録音を推奨する。

## macOS設定

`Audio MIDI設定.app` でUMC204HDを選び、録音フォーマットを以下にする。

```text
48,000 Hz
24-bit
2ch
```

録音ソフトは GarageBand、Audacity、REAPER などを推奨する。Mac内蔵マイク、AirPods、Bluetooth入力、自動ゲイン付きUSBキャプチャは測定に向かない。

## 1. 測定用MIDIを生成

`tools/fm_level_measurement` で実行する。出力は `out/` に書き込まれる。

```sh
cd tools/fm_level_measurement
python3 generate_measurement_midi.py
```

デフォルトでは以下の条件で全Programを測定する。

```text
Program: 0-127
Notes: C4(60), G4(67), C5(72)
CC#7 Volume: 100
CC#11 Expression: 127
Velocity: 100
```

特定Programだけ測る場合:

```sh
python3 generate_measurement_midi.py --programs 0,4,8-15
```

## 2. MIDIを実機へ送って録音

REAPERやMIDI Player Xなどで `out/fm_level_measurement.mid` を読み込み、MIDI出力先をFMSynthEnsembleV3にする。同時にUMC204HD INPUT 1/2をステレオ録音する。

GarageBandはMIDI再生には向かないため、MIDI送信は別アプリで行い、GarageBandは音声の録音・書き出しだけに使う。

録音開始後にMIDI再生を開始する。MIDI先頭には同期用の短いクリック音が3回入っているため、録音開始タイミングが少し前後しても後段で補正できる。

入力レベルは、最も大きい音色でもピークが `-12 dBFS` から `-6 dBFS` 程度に収まるようにする。`0 dBFS` に当たった録音はクリップしているため、測定に使わない。

録音が終わったらWAVで `out/` に書き出す。

```text
Format: WAV
Sample rate: 48,000 Hz
Bit depth: 24-bit PCM
Channels: Stereo
```

例:

```text
out/fm_level_recording.wav
```

### GarageBandで録音する例

1. `Audio MIDI設定.app` でUMC204HDを入出力デバイスにする（[macOS設定](#macos設定)を参照）。
2. GarageBandを起動し、空のプロジェクトを作成する（「新規プロジェクト」→「空のプロジェクト」）。
3. 最初に表示されるトラック選択で「オーディオ」を選び、入力をUMC204HDのステレオ入力（INPUT 1/2）にする。
   - トラックヘッダの入力ソースが `UMC204HD` / `Input 1-2` になっていることを確認する。
4. `GarageBand` → `設定`（または `環境設定`）→ `オーディオ/ MIDI` で、入力・出力デバイスがUMC204HDであることを確認する。
5. プロジェクトのサンプルレートを48 kHzにする。
   - `GarageBand` → `設定` → `オーディオ/ MIDI` → `詳細`、またはプロジェクト作成時のテンプレート設定で `48.0 kHz` を選ぶ。
6. トラックのモニタリングをオフにするか、ヘッドフォン音量を下げてフィードバックを避ける。
7. 別アプリで `out/fm_level_measurement.mid` のMIDI再生を準備する（出力先はFMSynthEnsembleV3）。
8. GarageBandで録音ボタン（●）を押してから、すぐにMIDI再生を開始する。
9. 全Programの演奏が終わったら録音を停止する。
10. `共有` → `曲を書き出す`（または `プロジェクトを書き出す`）→ `オーディオファイルを書き出す` を選ぶ。
    - 圧縮: `未圧縮`
    - 形式がWAVを選べる場合はWAV、AIFFのみの場合は一度AIFFで書き出してから変換する（下記）。

AIFFで書き出した場合は、`tools/fm_level_measurement` でWAVに変換する。

```sh
afconvert -f WAVE -d LEI24@48000 ~/Desktop/書き出し名.aiff out/fm_level_recording.wav
```

`analyze_recording.py` はWAVのみ読み込む。サンプルレートは48,000 Hz、ステレオ2chで書き出す。

## 3. WAVを解析

`tools/fm_level_measurement` で実行する。同期クリックから録音開始オフセットを自動推定する場合:

```sh
cd tools/fm_level_measurement
python3 analyze_recording.py out/fm_level_recording.wav --auto-offset
```

自動推定がうまくいかない場合は、録音波形上のMIDIタイムライン0秒位置との差を手で指定する。

```sh
python3 analyze_recording.py out/fm_level_recording.wav --offset-seconds 1.234
```

出力は以下。

- `out/fm_level_trim.csv`: Programごとの測定値と補正step
- `out/fm_level_trim_notes.csv`: Note単位の測定値
- `out/fm_tl_trim.inc`: C++用の `int8_t[128]` テーブル

ファームウェアへ反映するには、`out/fm_tl_trim.inc` を `src/drivers/fm/tone/fm_tl_trim.inc` にコピーする。

## 補正値の読み方

出力される `trim_step` はYM2608 FM TL stepである。FM TLは値が大きいほど音量が下がる。

```text
+4: 約3.0 dB下げる
 0: 補正なし
-4: 約3.0 dB上げる
```

計算式:

```text
delta_db  = measured_dbfs - target_dbfs
trim_step = round(delta_db / 0.75)
```

デフォルトの `target_dbfs` は測定したProgram音量の中央値である。基準を固定したい場合は `--target-dbfs -24.0` のように指定する。

## 実装に入れるときの注意

負の補正値は「音色を上げる」方向だが、キャリアTLが既に `0` の場合はそれ以上上げられない。実装では全体に数stepのヘッドルームを足してから、Program補正を加えると扱いやすい。

例:

```text
effective_tl = tone_tl + midi_attenuation + global_headroom + fm_tl_trim[program]
```

`global_headroom` を `4` 程度にしておくと、`fm_tl_trim = -4` までの持ち上げ余地を作れる。

## よくある問題

`Could not detect sync click` が出る場合:

- `--auto-offset-threshold-dbfs -55` のように閾値を下げる
- 録音波形を見て `--offset-seconds` を手動指定する
- 同期クリックが録音されているか確認する

測定値が全体に低すぎる場合:

- UMC204HDの入力ゲインを少し上げる
- FMSynthEnsembleV3側の出力レベルを上げる
- ただしピークが `0 dBFS` に届かないようにする

左右で値が大きく違う場合:

- INPUT 1/2のGAIN位置をそろえる
- ケーブルがステレオから左右に正しく分岐しているか確認する
- Panや出力L/R設定が測定中に変化していないか確認する
