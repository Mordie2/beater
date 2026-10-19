#pragma once
#include "core/TrackBuffer.h"
#include <string>

namespace decoder {

// Decode a whole file to stereo float at the given sample rate.
// On macOS this uses ExtAudioFile (mp3, wav, aiff, flac, m4a/aac, alac...),
// with miniaudio as fallback (mp3, wav, flac). Returns nullptr on failure.
std::shared_ptr<TrackBuffer> decodeStereo(const std::string &path, uint32_t sampleRate, std::string *error = nullptr);

// Decode to mono float at a given (low) sample rate, for analysis.
bool decodeMono(const std::string &path, uint32_t sampleRate, std::vector<float> &out, std::string *error = nullptr);

} // namespace decoder
