# ADR 0004: no-OS-FatFS の SDIO ドライバをビルドから外す

- 状態: 採用
- 日付: 2026-11-23（#PR141、Issue #138）

## 1. 背景

SBOM を生成したところ、SD カードは SPI でしか使っていないのに、no-OS-FatFS の SDIO ドライバ（`sd_driver/SDIO/*.c`）がファームウェアにリンクされていた。共通部の `sd_card.c` が `sd_sdio_ctor()` と `rp2040_sdio_get_sd_status()` を参照しているためで、どちらも SPI 接続のカードでは呼ばれない。

上流の README によれば、SDIO ドライバは ZuluSCSI-firmware（GPL-3.0-or-later）に由来する。

## 2. 決定

- `extern/CMakeLists.txt` で、上流ターゲットのソース一覧から `sd_driver/SDIO/*.c` を除外する。submodule は編集しない
- 共通部が参照する 2 関数は `src/drivers/storage/sdio_disabled.c` で定義する
- SBOM の生成（`gen_sbom.py`）では、`<name>_program` が ELF にリンクされていない pio を未使用として扱う

## 3. 根拠

- 使わないコードを、ライセンス上の義務ごとファームウェアに持ち込む理由がない
- submodule を編集せずにビルド側で外せるので、上流の更新に追従しやすい

## 4. 影響

- SDIO ドライバは SBOM で `scope: excluded` になり、THIRD_PARTY_NOTICES から ZuluSCSI の表記が消えた
- RP2350 の text が約 10KB 減った
- SD カードを SDIO で接続する構成に変える場合は、この決定を見直す必要がある

## 5. 関連

- ストレージドライバの構成: [architecture.md](../architecture/architecture.md) の「drivers/storage/」
- SBOM の生成と検査: [sbom.md](../process/sbom.md)
