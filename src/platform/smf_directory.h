//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

#include <cstdint>

namespace Platform {

/**
 * @brief SDカード上のSMF（.mid/.midi/.smf）ファイルを1件訪問するたびに呼ばれる
 * @param context ForEachSmfFile()に渡したのと同じポインタ
 * @param path SDボリューム上のフルパス（呼び出し中のみ有効。保持する場合はコピーすること）
 * @return 走査を続けるならtrue、打ち切るならfalse
 */
using SmfFileVisitor = bool (*)(void* context, const char* path);

/**
 * @brief SDカードのルートから再帰的に .mid/.midi/.smf ファイルを走査する
 * @details `Ls`はvisitorで直接標準出力へ列挙し、`Play <index>`は目的のインデックスに
 *          到達したらパスを控えて走査を打ち切る、という形でどちらもこの1つの関数を使う。
 *          インデックスはキャッシュせず、Playのたびにこの走査をやり直す。
 * @param visitor 見つかったファイルごとに呼ぶコールバック
 * @param context visitorへ渡す任意のポインタ
 * @return ルート/子ディレクトリを開けない、または走査中にI/Oエラーが起きた場合はfalse
 */
bool ForEachSmfFile(SmfFileVisitor visitor, void* context);

/** @brief Playlistに載せられる最大ファイル数 */
constexpr int kPlaylistMaxFiles = 64;

/** @brief Playlistのファイル名（拡張子込み、UTF-8）の最大バイト数（終端NULを除く） */
constexpr int kPlaylistNameMax = 63;

/**
 * @brief SDカードの`playlist`フォルダ直下のSMFファイルを、ファイル名の昇順で走査する
 * @details `0:/playlist`直下の.mid/.midi/.smfのみが対象で、サブフォルダは辿らない。
 *          並び順はASCIIの大文字小文字を区別しない名前順（同値ならバイト順）。
 *          kPlaylistMaxFiles件を超えるファイルと、名前がkPlaylistNameMaxバイトを超える
 *          ファイルは列挙されない。visitorには1始まりの位置順に呼ぶ（位置はvisitor側で数える）。
 *          内部の作業表は静的領域で共有するため、同時に複数の呼び出しはできない。
 * @param visitor 見つかったファイルごとに呼ぶコールバック
 * @param context visitorへ渡す任意のポインタ
 * @return `playlist`フォルダを開けない、または走査中にI/Oエラーが起きた場合はfalse
 *         （フォルダが無い場合を含む）。空のフォルダはtrueでvisitorは呼ばれない。
 */
bool ForEachPlaylistFile(SmfFileVisitor visitor, void* context);

}  // namespace Platform
