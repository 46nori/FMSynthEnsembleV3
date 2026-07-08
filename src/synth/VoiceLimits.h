//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

namespace VoiceLimits {

// Static storage is sized for the maximum physical FM configuration:
// 4 docks populated with YM2608, 6 FM channels each, and one optional CSM voice.
constexpr int kMaxFmModules = 4;
constexpr int kMaxYm2608FmChannels = 6;
constexpr int kMaxNoteVoices = kMaxFmModules * kMaxYm2608FmChannels;
// CsmVoice is a single logical voice that internally drives CH3 on the
// required number of OPN modules, so only one Voice object is needed.
constexpr int kMaxCsmVoices = 1;
constexpr int kMaxVoices = kMaxNoteVoices + kMaxCsmVoices;

}  // namespace VoiceLimits
