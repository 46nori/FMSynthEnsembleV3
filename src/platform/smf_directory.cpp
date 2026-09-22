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
bool VisitDirectory(const char* dir_path, SmfFileVisitor visitor, void* context, int depth,
                    bool* stop) {
    if (depth > kMaxDirectoryDepth) {
        return true;
    }

    DIR& dir = g_dir_stack[depth];
    if (f_opendir(&dir, dir_path) != FR_OK) {
        return false;
    }

    bool success = true;
    FILINFO& fno = g_fno_stack[depth];
    char* child_path = g_path_stack[depth];
    for (;;) {
        const FRESULT read_result = f_readdir(&dir, &fno);
        if (read_result != FR_OK) {
            success = false;
            break;
        }
        if (fno.fname[0] == 0) {
            break;  // 正常な走査終了
        }

        const int n = std::snprintf(child_path, kMaxPathLength, "%s/%s", dir_path, fno.fname);
        if (n <= 0 || static_cast<size_t>(n) >= kMaxPathLength) {
            continue;  // パスが長すぎるものはスキップ
        }

        if (IsAppleDoubleFile(fno.fname)) {
            continue;
        }

        if (fno.fattrib & AM_DIR) {
            if (!VisitDirectory(child_path, visitor, context, depth + 1, stop)) {
                success = false;
                break;
            }
        } else if (HasSmfExtension(fno.fname)) {
            if (!visitor(context, child_path)) {
                *stop = true;
            }
        }

        if (*stop) {
            break;
        }
    }
    if (f_closedir(&dir) != FR_OK) {
        success = false;
    }
    return success;
}

// Playlist用の作業表。名前昇順に並べたファイル名を保持する
constexpr const char* kPlaylistDirectory = "0:/playlist";
char g_playlist_names[kPlaylistMaxFiles][kPlaylistNameMax + 1];
DIR g_playlist_dir;
FILINFO g_playlist_fno;
char g_playlist_path[kMaxPathLength];

// ASCIIの大文字小文字を区別しない比較。同値なら通常のバイト順
int ComparePlaylistNames(const char* a, const char* b) {
    const char* pa = a;
    const char* pb = b;
    while (*pa != '\0' && *pb != '\0') {
        char ca = *pa;
        char cb = *pb;
        if (ca >= 'A' && ca <= 'Z') {
            ca = static_cast<char>(ca - 'A' + 'a');
        }
        if (cb >= 'A' && cb <= 'Z') {
            cb = static_cast<char>(cb - 'A' + 'a');
        }
        if (ca != cb) {
            return static_cast<unsigned char>(ca) < static_cast<unsigned char>(cb) ? -1 : 1;
        }
        ++pa;
        ++pb;
    }
    if (*pa != *pb) {
        return *pa == '\0' ? -1 : 1;
    }
    return std::strcmp(a, b);
}

// 名前をg_playlist_names[0..count)へ昇順を保って挿入する。満杯なら捨てる
void InsertPlaylistName(const char* name, int* count) {
    if (*count >= kPlaylistMaxFiles) {
        return;
    }
    int pos = *count;
    while (pos > 0 && ComparePlaylistNames(g_playlist_names[pos - 1], name) > 0) {
        std::memcpy(g_playlist_names[pos], g_playlist_names[pos - 1], kPlaylistNameMax + 1);
        --pos;
    }
    std::memcpy(g_playlist_names[pos], name, std::strlen(name) + 1);
    ++*count;
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
    if (f_closedir(&root) != FR_OK) {
        return false;
    }

    bool stop = false;
    return VisitDirectory("0:", visitor, context, 0, &stop);
}

bool ForEachPlaylistFile(SmfFileVisitor visitor, void* context) {
    FRESULT fr = f_opendir(&g_playlist_dir, kPlaylistDirectory);
    if (fr != FR_OK) {
        // フォルダが無い(FR_NO_PATH等)場合は再マウントしても変わらない。
        // カード未準備のときだけ、ForEachSmfFileと同様に1度だけ再マウントを試みる
        if (!IndicatesCardNotReady(fr) || !RemountSdCard()) {
            return false;
        }
        fr = f_opendir(&g_playlist_dir, kPlaylistDirectory);
        if (fr != FR_OK) {
            return false;
        }
    }

    int count = 0;
    bool success = true;
    for (;;) {
        const FRESULT read_result = f_readdir(&g_playlist_dir, &g_playlist_fno);
        if (read_result != FR_OK) {
            success = false;
            break;
        }
        if (g_playlist_fno.fname[0] == 0) {
            break;
        }
        if ((g_playlist_fno.fattrib & AM_DIR) || IsAppleDoubleFile(g_playlist_fno.fname) ||
            !HasSmfExtension(g_playlist_fno.fname) ||
            std::strlen(g_playlist_fno.fname) > static_cast<size_t>(kPlaylistNameMax)) {
            continue;
        }
        InsertPlaylistName(g_playlist_fno.fname, &count);
    }
    if (f_closedir(&g_playlist_dir) != FR_OK) {
        success = false;
    }
    if (!success) {
        return false;
    }

    for (int i = 0; i < count; ++i) {
        const int n = std::snprintf(g_playlist_path, kMaxPathLength, "%s/%s",
                                    kPlaylistDirectory, g_playlist_names[i]);
        if (n <= 0 || static_cast<size_t>(n) >= kMaxPathLength) {
            continue;
        }
        if (!visitor(context, g_playlist_path)) {
            break;
        }
    }
    return true;
}

}  // namespace Platform
