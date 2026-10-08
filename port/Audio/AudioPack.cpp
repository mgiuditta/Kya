#include "AudioPack.h"

#define XXH_INLINE_ALL 1
#include <xxhash.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

namespace AudioPack
{
	static std::string PackPath(const char* root, const char* kind, std::uint64_t hash)
	{
		char name[17];
		snprintf(name, sizeof(name), "%016llx", static_cast<unsigned long long>(hash));
		return std::string(root) + "/" + kind + "/" + name + ".wav";
	}

	static void PutU32(std::ofstream& file, std::uint32_t value) { file.write(reinterpret_cast<const char*>(&value), 4); }
	static void PutU16(std::ofstream& file, std::uint16_t value) { file.write(reinterpret_cast<const char*>(&value), 2); }

	std::uint64_t Hash(const std::vector<std::int16_t>& pcm, std::uint32_t channels, std::uint32_t sampleRate)
	{
		const std::uint64_t seed = (static_cast<std::uint64_t>(channels) << 32) | sampleRate;
		return XXH3_64bits_withSeed(pcm.data(), pcm.size() * sizeof(std::int16_t), seed);
	}

	void DumpIfEnabled(const char* kind, std::uint64_t hash, const std::vector<std::int16_t>& pcm, std::uint32_t channels, std::uint32_t sampleRate)
	{
		static const bool bEnabled = getenv("KYA_AUDIO_DUMP") != nullptr;
		if (!bEnabled || pcm.empty() || channels == 0) {
			return;
		}

		static std::mutex mutex;
		std::lock_guard<std::mutex> lock(mutex);

		const std::string path = PackPath("audio-dump", kind, hash);
		if (std::filesystem::exists(path)) {
			return;
		}
		std::filesystem::create_directories(std::filesystem::path(path).parent_path());

		// ponytail: little-endian host assumed (arm64, x64).
		const std::uint32_t dataSize = static_cast<std::uint32_t>(pcm.size() * sizeof(std::int16_t));
		std::ofstream file(path, std::ios::binary);
		file.write("RIFF", 4); PutU32(file, 36 + dataSize); file.write("WAVE", 4);
		file.write("fmt ", 4); PutU32(file, 16); PutU16(file, 1); PutU16(file, static_cast<std::uint16_t>(channels));
		PutU32(file, sampleRate); PutU32(file, sampleRate * channels * 2); PutU16(file, static_cast<std::uint16_t>(channels * 2)); PutU16(file, 16);
		file.write("data", 4); PutU32(file, dataSize);
		file.write(reinterpret_cast<const char*>(pcm.data()), dataSize);
	}

	bool LoadReplacement(const char* kind, std::uint64_t hash, std::vector<std::int16_t>& pcm, std::uint32_t& channels, std::uint32_t& sampleRate)
	{
		const std::string path = PackPath("mods/audio", kind, hash);
		std::error_code error;
		const auto size = std::filesystem::file_size(path, error);
		// Mod files are untrusted: 16-bit PCM WAV only, at most 256 MB.
		if (error || size < 44 || size > (256u << 20)) {
			return false;
		}

		std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
		std::ifstream file(path, std::ios::binary);
		if (!file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()))) {
			return false;
		}
		if (memcmp(data.data(), "RIFF", 4) != 0 || memcmp(data.data() + 8, "WAVE", 4) != 0) {
			return false;
		}

		std::uint16_t format = 0, fileChannels = 0, bits = 0;
		std::uint32_t rate = 0;
		for (std::size_t offset = 12; offset + 8 <= data.size();) {
			std::uint32_t chunkSize;
			memcpy(&chunkSize, data.data() + offset + 4, 4);
			const std::size_t body = offset + 8;
			if (chunkSize > data.size() - body) {
				return false;
			}
			if (memcmp(data.data() + offset, "fmt ", 4) == 0 && chunkSize >= 16) {
				memcpy(&format, data.data() + body, 2);
				memcpy(&fileChannels, data.data() + body + 2, 2);
				memcpy(&rate, data.data() + body + 4, 4);
				memcpy(&bits, data.data() + body + 14, 2);
			}
			else if (memcmp(data.data() + offset, "data", 4) == 0) {
				if (format != 1 || bits != 16 || fileChannels == 0 || fileChannels > 2 || rate < 1000 || rate > 192000) {
					return false;
				}
				pcm.resize(chunkSize / 2 / fileChannels * fileChannels);
				memcpy(pcm.data(), data.data() + body, pcm.size() * 2);
				channels = fileChannels;
				sampleRate = rate;
				return !pcm.empty();
			}
			offset = body + chunkSize + (chunkSize & 1);
		}
		return false;
	}

	std::uint32_t ScaleFrame(std::uint32_t frame, std::uint32_t oldRate, std::uint32_t newRate)
	{
		return static_cast<std::uint32_t>((static_cast<std::uint64_t>(frame) * newRate + oldRate / 2) / oldRate);
	}
}
