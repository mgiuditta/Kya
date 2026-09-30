#pragma once

#if defined(_WIN32) || defined(__APPLE__)
namespace Audio
{
// Call only after sample, stream and music voices have been destroyed.
void ShutdownAudioDevice();
}
#endif

#ifdef _WIN32
struct IXAudio2;
struct IXAudio2MasteringVoice;
namespace Audio
{
IXAudio2* GetAudioEngine();
IXAudio2MasteringVoice* GetAudioMasteringVoice();
}
#elif defined(__APPLE__)
#include <cstdint>
#include <memory>
#include <vector>
namespace Audio
{
struct DecodedSample;
class SampleVoice;
class StreamVoice;
class MusicOutput;
// miniaudio adapters (edSoundDeviceMiniaudio.cpp); each returns null on failure.
std::unique_ptr<SampleVoice> CreateDeviceSampleVoice(const DecodedSample& sample);
std::unique_ptr<StreamVoice> CreateDeviceStreamVoice(const std::vector<std::int16_t>& samples, std::uint32_t channels,
	std::uint32_t sampleRate, float volume);
std::unique_ptr<MusicOutput> CreateDeviceMusicOutput();
}
#endif
