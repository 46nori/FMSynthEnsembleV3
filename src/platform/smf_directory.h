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
 * @return ルートディレクトリを開けなければfalse（走査自体の可否。個々のファイルの
 *         成否はvisitorが判断する）
 */
bool ForEachSmfFile(SmfFileVisitor visitor, void* context);

}  // namespace Platform
