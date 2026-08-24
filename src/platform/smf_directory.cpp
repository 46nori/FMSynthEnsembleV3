//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#include "smf_directory.h"

#include <cstdio>
#include <cstring>

#include "ff.h"
#include "init.h"

namespace Platform {

namespace {

// パスバッファ長・再帰の深さの上限。
constexpr size_t kMaxPathLength = 192;
constexpr int kMaxDirectoryDepth = 6;

// DIR/FILINFO/パスバッファは階層ごとに静的配列で持ち回す。再帰呼び出しのたびに
// スタック確保すると、FILINFOだけでFF_LFN_BUF+1=256バイトあり、kMaxDirectoryDepth
// 段の再帰で約4KBを消費してTASK_STACK_SMF_PLAYERを超えスタックオーバーフローする
// （実機で発生・vApplicationStackOverflowHookのfor(;;)ループで検出）。
// 呼び出し元はSmfPlayerTaskのみで走査も逐次処理のため、階層インデックスでの使い回しは安全。
DIR    g_dir_stack[kMaxDirectoryDepth + 1];
FILINFO g_fno_stack[kMaxDirectoryDepth + 1];
char   g_path_stack[kMaxDirectoryDepth + 1][kMaxPathLength];

// macOSがFAT/exFATボリュームへファイルをコピーする際に自動生成する
// AppleDouble companion file（拡張属性・リソースフォークの保存先）。
// "._元のファイル名" という形で、拡張子だけ見ると本物のSMFと区別がつかない
// ため、名前で明示的に除外する。
bool IsAppleDoubleFile(const char* name) {
    return name[0] == '.' && name[1] == '_';
}

// SDカード抜去等でFatFsがハードウェアと通信できない状態を示すエラーコード。
// hw_config.cにCard Detectピンの配線がなく能動検知ができないため、実際に
// I/Oが失敗した時点でリアクティブに再マウントを試みる。
bool IndicatesCardNotReady(FRESULT fr) {
    return fr == FR_DISK_ERR || fr == FR_NOT_READY;
}

bool HasSmfExtension(const char* name) {
    const size_t len = std::strlen(name);
    const char* candidates[] = {".mid", ".midi", ".smf"};
    for (const char* ext : candidates) {
        const size_t elen = std::strlen(ext);
        if (len < elen) {
            continue;
        }
        const char* tail = name + (len - elen);
        bool match = true;
        for (size_t i = 0; i < elen; ++i) {
            char a = tail[i];
            char b = ext[i];
            if (a >= 'A' && a <= 'Z') {
                a = static_cast<char>(a - 'A' + 'a');
            }
            if (a != b) {
                match = false;
                break;
            }
        }
        if (match) {
            return true;
        }
    }
    return false;
}

// dir_path配下を走査する。visitorがfalseを返したら*stopをtrueにして呼び出し元へ伝える。
// dir_pathは呼び出し元の g_path_stack[depth-1]（または"0:"リテラル）を指す。
// この関数は自分のスロット g_path_stack[depth] にしか書き込まないため、
// dir_pathが指す親スロットの内容は本呼び出しの間ずっと有効。
void VisitDirectory(const char* dir_path, SmfFileVisitor visitor, void* context, int depth,
                     bool* stop) {
    if (depth > kMaxDirectoryDepth) {
        return;
    }

    DIR& dir = g_dir_stack[depth];
    if (f_opendir(&dir, dir_path) != FR_OK) {
        return;
    }

    FILINFO& fno = g_fno_stack[depth];
    char* child_path = g_path_stack[depth];
    for (;;) {
        if (f_readdir(&dir, &fno) != FR_OK || fno.fname[0] == 0) {
            break;  // エラーまたは走査終了
        }

        const int n = std::snprintf(child_path, kMaxPathLength, "%s/%s", dir_path, fno.fname);
        if (n <= 0 || static_cast<size_t>(n) >= kMaxPathLength) {
            continue;  // パスが長すぎるものはスキップ
        }

        if (IsAppleDoubleFile(fno.fname)) {
            continue;
        }

        if (fno.fattrib & AM_DIR) {
            VisitDirectory(child_path, visitor, context, depth + 1, stop);
        } else if (HasSmfExtension(fno.fname)) {
            if (!visitor(context, child_path)) {
                *stop = true;
            }
        }

        if (*stop) {
            break;
        }
    }
    f_closedir(&dir);
}

}  // namespace

bool ForEachSmfFile(SmfFileVisitor visitor, void* context) {
    DIR root;
    FRESULT fr = f_opendir(&root, "0:");
    if (fr != FR_OK) {
        // カードの抜き挿しでマウントが失われた可能性があるため、1度だけ
        // 再マウントを試みてから再挑戦する
        if (!IndicatesCardNotReady(fr) || !RemountSdCard()) {
            return false;
        }
        fr = f_opendir(&root, "0:");
        if (fr != FR_OK) {
            return false;
        }
    }
    f_closedir(&root);

    bool stop = false;
    VisitDirectory("0:", visitor, context, 0, &stop);
    return true;
}

}  // namespace Platform
