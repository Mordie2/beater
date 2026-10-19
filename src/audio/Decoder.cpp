#include "audio/Decoder.h"

#include <miniaudio.h>

#if defined(__APPLE__)
#include <AudioToolbox/AudioToolbox.h>
#endif

namespace decoder {
namespace {

// Interleaved stereo float at sampleRate.
bool decodeInterleavedStereo(const std::string &path, uint32_t sampleRate, std::vector<float> &out, std::string *error)
{
    out.clear();

#if defined(__APPLE__)
    {
        CFURLRef url = CFURLCreateFromFileSystemRepresentation(
            nullptr, reinterpret_cast<const UInt8 *>(path.c_str()), CFIndex(path.size()), false);
        ExtAudioFileRef file = nullptr;
        OSStatus st = url ? ExtAudioFileOpenURL(url, &file) : -1;
        if (url)
            CFRelease(url);
        if (st == noErr && file) {
            AudioStreamBasicDescription client{};
            client.mSampleRate = sampleRate;
            client.mFormatID = kAudioFormatLinearPCM;
            client.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
            client.mBitsPerChannel = 32;
            client.mChannelsPerFrame = 2;
            client.mFramesPerPacket = 1;
            client.mBytesPerFrame = 8;
            client.mBytesPerPacket = 8;
            st = ExtAudioFileSetProperty(file, kExtAudioFileProperty_ClientDataFormat, sizeof(client), &client);
            if (st == noErr) {
                SInt64 fileFrames = 0;
                UInt32 sz = sizeof(fileFrames);
                AudioStreamBasicDescription fileFmt{};
                UInt32 fsz = sizeof(fileFmt);
                if (ExtAudioFileGetProperty(file, kExtAudioFileProperty_FileLengthFrames, &sz, &fileFrames) == noErr
                    && ExtAudioFileGetProperty(file, kExtAudioFileProperty_FileDataFormat, &fsz, &fileFmt) == noErr
                    && fileFmt.mSampleRate > 0) {
                    out.reserve(size_t(double(fileFrames) * sampleRate / fileFmt.mSampleRate + 4096) * 2);
                }
                const UInt32 chunk = 32768;
                std::vector<float> tmp(chunk * 2);
                for (;;) {
                    AudioBufferList abl;
                    abl.mNumberBuffers = 1;
                    abl.mBuffers[0].mNumberChannels = 2;
                    abl.mBuffers[0].mDataByteSize = chunk * 8;
                    abl.mBuffers[0].mData = tmp.data();
                    UInt32 frames = chunk;
                    if (ExtAudioFileRead(file, &frames, &abl) != noErr) {
                        out.clear();
                        break;
                    }
                    if (frames == 0)
                        break;
                    out.insert(out.end(), tmp.begin(), tmp.begin() + frames * 2);
                }
            }
            ExtAudioFileDispose(file);
            if (!out.empty())
                return true;
        }
    }
#endif

    ma_decoder_config cfg = ma_decoder_config_init(ma_format_f32, 2, sampleRate);
    ma_decoder dec;
    if (ma_decoder_init_file(path.c_str(), &cfg, &dec) != MA_SUCCESS) {
        if (error)
            *error = "Unsupported or unreadable audio file";
        return false;
    }
    ma_uint64 len = 0;
    if (ma_decoder_get_length_in_pcm_frames(&dec, &len) == MA_SUCCESS && len > 0)
        out.reserve(size_t(len) * 2);
    const ma_uint64 chunk = 32768;
    std::vector<float> tmp(chunk * 2);
    for (;;) {
        ma_uint64 got = 0;
        ma_result r = ma_decoder_read_pcm_frames(&dec, tmp.data(), chunk, &got);
        if (got > 0)
            out.insert(out.end(), tmp.begin(), tmp.begin() + size_t(got) * 2);
        if (r != MA_SUCCESS || got < chunk)
            break;
    }
    ma_decoder_uninit(&dec);
    if (out.empty()) {
        if (error)
            *error = "File decoded to zero frames";
        return false;
    }
    return true;
}

} // namespace

std::shared_ptr<TrackBuffer> decodeStereo(const std::string &path, uint32_t sampleRate, std::string *error)
{
    std::vector<float> inter;
    if (!decodeInterleavedStereo(path, sampleRate, inter, error))
        return nullptr;
    auto tb = std::make_shared<TrackBuffer>();
    tb->sampleRate = sampleRate;
    const size_t n = inter.size() / 2;
    tb->left.resize(n);
    tb->right.resize(n);
    for (size_t i = 0; i < n; ++i) {
        tb->left[i] = inter[2 * i];
        tb->right[i] = inter[2 * i + 1];
    }
    tb->computeOverview();
    return tb;
}

bool decodeMono(const std::string &path, uint32_t sampleRate, std::vector<float> &out, std::string *error)
{
    std::vector<float> inter;
    if (!decodeInterleavedStereo(path, sampleRate, inter, error))
        return false;
    const size_t n = inter.size() / 2;
    out.resize(n);
    for (size_t i = 0; i < n; ++i)
        out[i] = 0.5f * (inter[2 * i] + inter[2 * i + 1]);
    return true;
}

} // namespace decoder
