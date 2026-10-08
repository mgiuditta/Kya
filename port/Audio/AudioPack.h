#pragma once

#include <cstdint>
#include <vector>

// Audio replacement keyed by a hash of the decoded PCM, mirroring the texture pack.
// Kinds: "stream" (cutscene voice), "sample" (SFX), "music" (instrument samples).
// Packs live in mods/audio/<kind>/<hash>.wav as 16-bit PCM at any rate; the length should
// scale with the rate so loop points and cutscene timing stay where they were.
// KYA_AUDIO_DUMP=1 writes every decoded original to audio-dump/<kind>/<hash>.wav.
namespace AudioPack
{
	std::uint64_t Hash(const std::vector<std::int16_t>& pcm, std::uint32_t channels, std::uint32_t sampleRate);
	void DumpIfEnabled(const char* kind, std::uint64_t hash, const std::vector<std::int16_t>& pcm, std::uint32_t channels, std::uint32_t sampleRate);
	bool LoadReplacement(const char* kind, std::uint64_t hash, std::vector<std::int16_t>& pcm, std::uint32_t& channels, std::uint32_t& sampleRate);
	// Maps a frame index at oldRate to newRate.
	std::uint32_t ScaleFrame(std::uint32_t frame, std::uint32_t oldRate, std::uint32_t newRate);
}
