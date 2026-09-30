#include "edSoundDevice.h"
#include "edMusicControls.h"
#include "edMusicService.h"
#include "edSoundSampleService.h"
#include "edSoundStreamService.h"
#include "log.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <mutex>

#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_RUNTIME_LINKING
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER
#include "miniaudio.h"

namespace Audio
{
namespace
{
std::mutex engineMutex;
ma_engine engine;
bool engineReady = false;
bool engineFailed = false;

// XAUDIO2_MIN_FREQ_RATIO and the 4.0 cap passed to CreateSourceVoice on Windows.
constexpr float MinPitch = 1.0f / 1024.0f;
constexpr float MaxPitch = 4.0f;
// Enough for the 3 queued 10 ms blocks the music service allows, with headroom.
constexpr ma_uint32 MusicRingFrames = MusicSampleRate / 10;

void LogAudioError(const char* message, ma_result result)
{
	Log::GetInstance().AddLog(LogLevel::Error, "Audio", "{} ({})", message, ma_result_description(result));
}

ma_engine* GetEngine()
{
	std::lock_guard lock(engineMutex);
	if (engineReady) return &engine;
	if (engineFailed) return nullptr;
	const ma_result result = ma_engine_init(nullptr, &engine);
	if (result != MA_SUCCESS) {
		LogAudioError("ma_engine_init failed", result);
		engineFailed = true;
		return nullptr;
	}
	engineReady = true;
	return &engine;
}

float FiniteClamp(float value, float low, float high, float fallback)
{
	return std::isfinite(value) ? std::clamp(value, low, high) : fallback;
}

// One ma_sound over caller-owned s16 PCM, shared by sample and stream voices.
struct PcmSound
{
	ma_audio_buffer_ref buffer{};
	ma_sound sound{};
	bool bufferReady = false;
	bool soundReady = false;

	~PcmSound()
	{
		if (soundReady) ma_sound_uninit(&sound);
		if (bufferReady) ma_audio_buffer_ref_uninit(&buffer);
	}

	bool Initialize(const std::int16_t* pcm, std::uint64_t frames, std::uint32_t channels, std::uint32_t sampleRate,
		std::uint64_t loopBegin, std::uint64_t loopLength)
	{
		ma_engine* pEngine = GetEngine();
		if (!pEngine) return false;
		ma_result result = ma_audio_buffer_ref_init(ma_format_s16, channels, pcm, frames, &buffer);
		if (result != MA_SUCCESS) {
			LogAudioError("ma_audio_buffer_ref_init failed", result);
			return false;
		}
		bufferReady = true;
		// The init call has no rate parameter; the ref would otherwise play at the engine rate.
		buffer.sampleRate = sampleRate;
		if (loopLength) ma_data_source_set_loop_point_in_pcm_frames(&buffer, loopBegin, loopBegin + loopLength);

		result = ma_sound_init_from_data_source(pEngine, &buffer, MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, &sound);
		if (result != MA_SUCCESS) {
			LogAudioError("ma_sound_init_from_data_source failed", result);
			return false;
		}
		soundReady = true;
		ma_sound_set_looping(&sound, loopLength != 0);
		return true;
	}

	std::uint64_t Cursor()
	{
		ma_uint64 cursor = 0;
		ma_sound_get_cursor_in_pcm_frames(&sound, &cursor);
		return cursor;
	}
};

class MiniaudioSampleVoice final : public SampleVoice
{
public:
	PcmSound pcm;

	bool SetControls(const SampleControls& controls) override
	{
		// ponytail: volume + balance pan drops opposite-sign L/R gains (SPU phase inversion);
		// add a 2x1 gain ma_node if logs from edSoundPlay.cpp show negative left=/right=.
		const float left = std::fabs(FiniteClamp(controls.left, -1, 1, 0));
		const float right = std::fabs(FiniteClamp(controls.right, -1, 1, 0));
		const float volume = (std::max)(left, right);
		ma_sound_set_volume(&pcm.sound, volume);
		ma_sound_set_pan(&pcm.sound, volume > 0 ? (right - left) / volume : 0);
		ma_sound_set_pitch(&pcm.sound, FiniteClamp(controls.pitch, MinPitch, MaxPitch, 1.0f));
		return true;
	}
	bool SetPlaying(bool playing) override
	{
		return (playing ? ma_sound_start(&pcm.sound) : ma_sound_stop(&pcm.sound)) == MA_SUCCESS;
	}
	bool IsFinished() override { return ma_sound_at_end(&pcm.sound); }
	std::uint64_t GetFramesPlayed() override { return pcm.Cursor(); }
};

class MiniaudioStreamVoice final : public StreamVoice
{
public:
	PcmSound pcm;

	bool Start() override { return ma_sound_start(&pcm.sound) == MA_SUCCESS; }
	void Stop() override { ma_sound_stop(&pcm.sound); }
	void Rewind() override
	{
		ma_sound_stop(&pcm.sound);
		ma_sound_seek_to_pcm_frame(&pcm.sound, 0);
	}
	bool SetVolume(float volume) override
	{
		ma_sound_set_volume(&pcm.sound, volume);
		return true;
	}
	std::uint64_t GetFramesPlayed() override { return pcm.Cursor(); }
	bool IsFinished() override { return ma_sound_at_end(&pcm.sound); }
};

// The music worker writes 10 ms blocks; the audio thread drains them. ma_pcm_rb is
// single-producer single-consumer and pads underruns with silence.
class MiniaudioMusicOutput final : public MusicOutput
{
	ma_pcm_rb ring{};
	ma_sound sound{};
	bool ringReady = false;
	bool soundReady = false;

public:
	~MiniaudioMusicOutput() override
	{
		if (soundReady) ma_sound_uninit(&sound);
		if (ringReady) ma_pcm_rb_uninit(&ring);
	}
	bool Initialize()
	{
		ma_engine* pEngine = GetEngine();
		if (!pEngine) return false;
		ma_result result = ma_pcm_rb_init(ma_format_f32, 2, MusicRingFrames, nullptr, nullptr, &ring);
		if (result != MA_SUCCESS) {
			LogAudioError("ma_pcm_rb_init failed", result);
			return false;
		}
		ringReady = true;
		ma_pcm_rb_set_sample_rate(&ring, MusicSampleRate);
		result = ma_sound_init_from_data_source(pEngine, &ring, MA_SOUND_FLAG_NO_SPATIALIZATION | MA_SOUND_FLAG_NO_PITCH,
			nullptr, &sound);
		if (result != MA_SUCCESS) {
			LogAudioError("ma_sound_init_from_data_source failed", result);
			return false;
		}
		soundReady = true;
		return true;
	}
	unsigned Queued() override
	{
		const ma_uint32 blockFrames = MusicSampleRate / 100;
		return (ma_pcm_rb_available_read(&ring) + blockFrames - 1) / blockFrames;
	}
	bool Submit(const float* samples, unsigned count) override
	{
		ma_uint32 frames = count / 2;
		while (frames) {
			ma_uint32 mapped = frames;
			void* pOut = nullptr;
			if (ma_pcm_rb_acquire_write(&ring, &mapped, &pOut) != MA_SUCCESS || mapped == 0) return false;
			std::memcpy(pOut, samples, mapped * 2 * sizeof(float));
			ma_pcm_rb_commit_write(&ring, mapped);
			samples += mapped * 2;
			frames -= mapped;
		}
		return true;
	}
	bool Play(bool playing) override
	{
		return (playing ? ma_sound_start(&sound) : ma_sound_stop(&sound)) == MA_SUCCESS;
	}
};
} // namespace

std::unique_ptr<SampleVoice> CreateDeviceSampleVoice(const DecodedSample& sample)
{
	auto voice = std::make_unique<MiniaudioSampleVoice>();
	if (!voice->pcm.Initialize(sample.pcm.data(), sample.pcm.size(), 1, sample.sampleRate, sample.loopBegin, sample.loopLength))
		return nullptr;
	return voice;
}

std::unique_ptr<StreamVoice> CreateDeviceStreamVoice(const std::vector<std::int16_t>& samples, std::uint32_t channels,
	std::uint32_t sampleRate, float volume)
{
	auto voice = std::make_unique<MiniaudioStreamVoice>();
	if (!voice->pcm.Initialize(samples.data(), samples.size() / channels, channels, sampleRate, 0, 0))
		return nullptr;
	voice->SetVolume(volume);
	return voice;
}

std::unique_ptr<MusicOutput> CreateDeviceMusicOutput()
{
	auto output = std::make_unique<MiniaudioMusicOutput>();
	if (!output->Initialize()) return nullptr;
	return output;
}

void ShutdownAudioDevice()
{
	std::lock_guard lock(engineMutex);
	if (engineReady) ma_engine_uninit(&engine);
	engineReady = false;
	engineFailed = false;
}
} // namespace Audio
