"""SBOM に載せるサードパーティコンポーネントの定義表。

ファイルがどのコンポーネントに属するかは root の最長一致で決める。入れ子のコンポーネント
（pico-sdk 内の TinyUSB など）は親より長い root を持つので、そちらが優先される。

root の起点:
    repo       リポジトリのルート
    sdk        PICO_SDK_PATH
    toolchain  クロスコンパイラのインストール先（ファイル単位ではなく、リンクされたアーカイブで判定する）

version の取り方:
    ("lock",)                         extern/submodules.lock の「バージョンの目安」列
    ("sdk",)                          pico_sdk_version.cmake
    ("macros", file, [names...])      C マクロの数値を "." で連結する
    ("regex", file, pattern)          最初に一致したグループ 1
    ("compiler",)                     CMake が検出したコンパイラのバージョン
    None                              バージョンを持たない（git のコミットだけで識別する）

license_texts は THIRD_PARTY_NOTICES.md に載せる本文の取得元。
    "path"            root からの相対パス。ファイル全体
    ("header", path)  そのファイル先頭のコメントブロック
    ("inline", text)  固定文（ツールチェーンのように本文がリポジトリ外にあるもの）

ライセンスの判断はここで人が行う。gen_sbom.py はファイル先頭の SPDX-License-Identifier を
走査して evidence に記録するだけで、declared の値は変えない。
"""

COMPONENTS = [
    # ------------------------------------------------------------------ pico-sdk
    {
        "key": "pico-sdk",
        "name": "pico-sdk",
        "supplier": "Raspberry Pi Ltd",
        "root": ("sdk", ""),
        "git": "https://github.com/raspberrypi/pico-sdk",
        "version": ("sdk",),
        "licenses": ["BSD-3-Clause"],
        "license_texts": ["LICENSE.TXT"],
    },
    {
        "key": "tinyusb",
        "name": "TinyUSB",
        "parent": "pico-sdk",
        "supplier": "hathach",
        "root": ("sdk", "lib/tinyusb"),
        "git": "https://github.com/hathach/tinyusb",
        "version": ("macros", "src/tusb_option.h",
                    ["TUSB_VERSION_MAJOR", "TUSB_VERSION_MINOR", "TUSB_VERSION_REVISION"]),
        "licenses": ["MIT"],
        "license_texts": ["LICENSE"],
    },
    {
        "key": "cmsis",
        "name": "CMSIS",
        "parent": "pico-sdk",
        "supplier": "Arm Limited",
        "root": ("sdk", "src/rp2_common/cmsis/stub/CMSIS"),
        "purl": "pkg:github/ARM-software/CMSIS_6",
        "version": ("macros", "Core/Include/cmsis_version.h",
                    ["__CM_CMSIS_VERSION_MAIN", "__CM_CMSIS_VERSION_SUB"]),
        "licenses": ["Apache-2.0"],
        "license_texts": ["LICENSE.txt"],
    },
    {
        "key": "pico-printf",
        "name": "printf (Marco Paland)",
        "parent": "pico-sdk",
        "supplier": "Marco Paland",
        "root": ("sdk", "src/rp2_common/pico_printf"),
        "purl": "pkg:github/mpaland/printf",
        "version": None,
        "licenses": ["MIT"],
        "license_texts": [("header", "printf.c")],
    },
    {
        "key": "btstack",
        "name": "BTstack",
        "parent": "pico-sdk",
        "supplier": "BlueKitchen GmbH",
        "root": ("sdk", "lib/btstack"),
        "git": "https://github.com/bluekitchen/btstack",
        "version": ("regex", "CHANGELOG.md", r"^## Release v(\S+)"),
        "license_name": "BTstack License (Raspberry Pi exception, see LICENSE)",
        "license_texts": [],
    },
    {
        "key": "cyw43-driver",
        "name": "cyw43-driver",
        "parent": "pico-sdk",
        "supplier": "George Robotics",
        "root": ("sdk", "lib/cyw43-driver"),
        "git": "https://github.com/georgerobotics/cyw43-driver",
        "version": ("macros", "src/cyw43.h",
                    ["CYW43_VERSION_MAJOR", "CYW43_VERSION_MINOR", "CYW43_VERSION_MICRO"]),
        "license_name": "cyw43-driver License (Raspberry Pi exception, see LICENSE.RP)",
        "license_texts": [],
    },
    {
        "key": "lwip",
        "name": "lwIP",
        "parent": "pico-sdk",
        "supplier": "lwIP developers",
        "root": ("sdk", "lib/lwip"),
        "git": "https://github.com/lwip-tcpip/lwip",
        "version": ("macros", "src/include/lwip/init.h",
                    ["LWIP_VERSION_MAJOR", "LWIP_VERSION_MINOR", "LWIP_VERSION_REVISION"]),
        "licenses": ["BSD-3-Clause"],
        "license_texts": ["COPYING"],
    },
    {
        "key": "mbedtls",
        "name": "Mbed TLS",
        "parent": "pico-sdk",
        "supplier": "Mbed TLS contributors",
        "root": ("sdk", "lib/mbedtls"),
        "git": "https://github.com/Mbed-TLS/mbedtls",
        "version": ("regex", "include/mbedtls/build_info.h",
                    r'#define\s+MBEDTLS_VERSION_STRING\s+"([^"]+)"'),
        "licenses": ["Apache-2.0 OR GPL-2.0-or-later"],
        "license_texts": [],
    },

    # ----------------------------------------------------------- FreeRTOS-Kernel
    {
        "key": "freertos-kernel",
        "name": "FreeRTOS-Kernel",
        "supplier": "Amazon.com, Inc.",
        "root": ("repo", "extern/FreeRTOS-Kernel"),
        "git": "https://github.com/FreeRTOS/FreeRTOS-Kernel",
        "version": ("lock",),
        "licenses": ["MIT"],
        "license_texts": ["LICENSE.md"],
    },
    {
        "key": "freertos-port-rp2040",
        "name": "FreeRTOS-Kernel RP2040 port",
        "parent": "freertos-kernel",
        "supplier": "Raspberry Pi Ltd",
        "root": ("repo", "extern/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2040"),
        "version": None,
        "licenses": ["MIT AND BSD-3-Clause"],
        "license_texts": ["LICENSE.md"],
    },
    {
        "key": "freertos-community-ports",
        "name": "FreeRTOS-Kernel-Community-Supported-Ports",
        "parent": "freertos-kernel",
        "supplier": "FreeRTOS community",
        "root": ("repo", "extern/FreeRTOS-Kernel/portable/ThirdParty/Community-Supported-Ports"),
        "git": "https://github.com/FreeRTOS/FreeRTOS-Kernel-Community-Supported-Ports",
        "version": None,
        # ディレクトリごとにライセンスが異なるリポジトリ。使っている移植は下の子コンポーネントで扱う
        "license_name": "Per-directory licenses (see LICENSE in each directory)",
        "license_texts": [],
    },
    {
        "key": "freertos-port-rp2350",
        "name": "FreeRTOS-Kernel RP2350_ARM_NTZ port",
        "parent": "freertos-community-ports",
        "supplier": "Raspberry Pi Ltd",
        "root": ("repo", "extern/FreeRTOS-Kernel/portable/ThirdParty/Community-Supported-Ports/GCC/RP2350_ARM_NTZ"),
        "version": None,
        "licenses": ["MIT AND BSD-3-Clause"],
        "license_texts": ["LICENSE.md"],
    },
    {
        "key": "freertos-partner-ports",
        "name": "FreeRTOS-Kernel-Partner-Supported-Ports",
        "parent": "freertos-kernel",
        "supplier": "FreeRTOS partners",
        "root": ("repo", "extern/FreeRTOS-Kernel/portable/ThirdParty/Partner-Supported-Ports"),
        "git": "https://github.com/FreeRTOS/FreeRTOS-Kernel-Partner-Supported-Ports",
        "version": None,
        "license_name": "Per-directory licenses (see LICENSE in each directory)",
        "license_texts": [],
    },

    # ---------------------------------------------------------------- no-OS-FatFS
    {
        "key": "no-os-fatfs",
        "name": "no-OS-FatFS-SD-SDIO-SPI-RPi-Pico",
        "supplier": "Carl John Kugler III",
        "root": ("repo", "extern/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico"),
        "git": "https://github.com/carlk3/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico",
        "version": ("lock",),
        "licenses": ["Apache-2.0"],
        "license_texts": ["LICENSE"],
    },
    {
        "key": "fatfs",
        "name": "FatFs",
        "parent": "no-os-fatfs",
        "supplier": "ChaN",
        "root": ("repo", "extern/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico/src/ff15"),
        "purl": "pkg:generic/fatfs",
        "version": ("regex", "source/ff.h", r"FatFs - Generic FAT Filesystem module\s+(R\S+)"),
        "licenses": ["BSD-1-Clause"],
        "license_texts": ["LICENSE.txt"],
    },
    {
        "key": "no-os-fatfs-sdio",
        "name": "no-OS-FatFS SDIO driver",
        "parent": "no-os-fatfs",
        "supplier": "Carl John Kugler III",
        "root": ("repo", "extern/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico/src/sd_driver/SDIO"),
        "version": None,
        # ファイルにライセンス表記がなく、リポジトリ全体は Apache-2.0 と宣言されている。
        # 上流 README は ZuluSCSI-firmware (GPL-3.0-or-later) 由来と明記しているので、由来を pedigree に残す
        "licenses": ["Apache-2.0"],
        "license_texts": [],
        "ancestors": [{
            "name": "ZuluSCSI-firmware",
            "supplier": "Rabbit Hole Computing",
            "purl": "pkg:github/ZuluSCSI/ZuluSCSI-firmware",
            "licenses": ["GPL-3.0-or-later"],
            "license_texts": ["../../ZuluSCSI-firmware/LICENSE"],
        }],
        "review": "Derived from ZuluSCSI-firmware (GPL-3.0-or-later) according to the upstream README, "
                  "while the upstream repository declares Apache-2.0. License compatibility needs review.",
    },
    {
        "key": "no-os-fatfs-spi",
        "name": "no-OS-FatFS SPI driver",
        "parent": "no-os-fatfs",
        "supplier": "Carl John Kugler III",
        "root": ("repo", "extern/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico/src/sd_driver/SPI"),
        "version": None,
        "licenses": ["Apache-2.0"],
        "license_texts": [],
        "ancestors": [{
            "name": "Mbed OS SDBlockDevice",
            "supplier": "Arm Limited",
            "purl": "pkg:github/ARMmbed/mbed-os",
            "licenses": ["Apache-2.0"],
            "license_texts": [],
        }],
    },

    # -------------------------------------------------------------- その他 extern
    {
        "key": "lcdmenu",
        "name": "LcdMenu",
        "supplier": "Forntoh Thomas",
        "root": ("repo", "extern/LcdMenu"),
        "git": "https://github.com/forntoh/LcdMenu",
        "version": ("lock",),
        "licenses": ["MIT"],
        "license_texts": ["LICENSE"],
    },
    {
        "key": "nju72343",
        "name": "NJU72343-library",
        "supplier": "46nori",
        "root": ("repo", "extern/NJU72343-library"),
        "git": "https://github.com/46nori/NJU72343-library",
        "version": ("lock",),
        "licenses": ["MIT"],
        "license_texts": ["LICENSE"],
    },

    # ------------------------------------------------------------- ツールチェーン
    # 静的リンクされる実行時ライブラリ。リンカ map の "Archive member included" に
    # 現れたアーカイブだけをビルド対象とする
    {
        "key": "newlib",
        "name": "newlib",
        "supplier": "Red Hat, Inc. and contributors",
        "root": ("toolchain", ""),
        "archives": ["libc.a", "libg.a", "libm.a", "libnosys.a"],
        "purl": "pkg:generic/newlib",
        "version": ("regex", "arm-none-eabi/include/_newlib_version.h",
                    r'#define\s+_NEWLIB_VERSION\s+"([^"]+)"'),
        "license_name": "newlib licenses (COPYING.NEWLIB)",
        "license_texts": [("inline",
            "newlib is distributed under a collection of BSD-style and similar permissive licenses.\n"
            "The full list is in COPYING.NEWLIB of the newlib source distribution:\n"
            "https://sourceware.org/git/?p=newlib-cygwin.git;a=blob;f=COPYING.NEWLIB\n")],
    },
    {
        "key": "libgcc",
        "name": "libgcc",
        "supplier": "Free Software Foundation",
        "root": ("toolchain", ""),
        "archives": ["libgcc.a"],
        "purl": "pkg:generic/gcc",
        "version": ("compiler",),
        "licenses": ["GPL-3.0-or-later WITH GCC-exception-3.1"],
        "license_texts": [("inline",
            "Linked under the GCC Runtime Library Exception, version 3.1, which permits distributing\n"
            "the resulting firmware under terms of the developer's choice:\n"
            "https://www.gnu.org/licenses/gcc-exception-3.1.html\n")],
    },
    {
        "key": "libstdcxx",
        "name": "libstdc++",
        "supplier": "Free Software Foundation",
        "root": ("toolchain", ""),
        "archives": ["libstdc++.a", "libsupc++.a"],
        "purl": "pkg:generic/gcc",
        "version": ("compiler",),
        "licenses": ["GPL-3.0-or-later WITH GCC-exception-3.1"],
        "license_texts": [("inline",
            "Linked under the GCC Runtime Library Exception, version 3.1, which permits distributing\n"
            "the resulting firmware under terms of the developer's choice:\n"
            "https://www.gnu.org/licenses/gcc-exception-3.1.html\n")],
    },
]

# 本体（SBOM の metadata.component）
PROJECT = {
    "name": "FMSynthEnsembleV3",
    "supplier": "46nori",
    "licenses": ["MIT"],
    "license_texts": ["LICENSE"],
}
