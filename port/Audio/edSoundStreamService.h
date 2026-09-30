#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace Audio
{

struct StreamInfo
{
	bool ready = false;
	bool playing = false;
	float volume = 1.0f;
	std::uint32_t blockSize = 0;
	std::uint32_t channels = 1;
	std::uint32_t sampleRate = 0;
	std::uint64_t position = 0;
};

// Plays one fully decoded stream buffer that the service keeps alive until the
// voice is destroyed. All calls, including polling, run on the game thread.
class StreamVoice
{
public:
	virtual ~StreamVoice() = default;
	// Plays from the start again if the buffer has finished.
	virtual bool Start() = 0;
	virtual void Stop() = 0;
	// Stops and drops playback progress so the next Start plays from the beginning.
	virtual void Rewind() = 0;
	virtual bool SetVolume(float volume) = 0;
	virtual std::uint64_t GetFramesPlayed() = 0;
	virtual bool IsFinished() = 0;
};

void RegisterStream(std::uint32_t streamId, std::uint32_t blockSize, float sampleRate, std::uint32_t channels = 1);
void PrepareStream(std::uint32_t streamId);
bool StartStream(std::uint32_t streamId);
bool SetStreamVolume(std::uint32_t streamId, float volume);
bool StopStream(std::uint32_t streamId);
bool GetStreamInfo(std::uint32_t streamId, StreamInfo& out);
bool UnregisterStream(std::uint32_t streamId);
void ResetStreams();

bool LoadStream(std::uint32_t streamId, const char* path);
bool DecodeVag(const std::uint8_t* data, std::size_t size, std::vector<std::int16_t>& samples, std::uint32_t& sampleRate);
bool DecodeRawAdpcm(const std::uint8_t* data, std::size_t size, std::vector<std::int16_t>& samples);
bool DecodeMib(const std::uint8_t* data, std::size_t size, std::uint32_t channels,
	std::uint32_t interleaveBlockSize, std::uint32_t sampleRate, std::vector<std::int16_t>& samples);

} // namespace Audio
