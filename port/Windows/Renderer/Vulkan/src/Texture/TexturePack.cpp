#include "TexturePack.h"

#define XXH_INLINE_ALL 1
#include <xxhash.h>

// ponytail: stb copies already vendored by tracy and glfw; vendor our own if those move.
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <mutex>

namespace Renderer::TexturePack
{
	static std::string HashName(uint64_t hash)
	{
		char buffer[17];
		snprintf(buffer, sizeof(buffer), "%016llx", static_cast<unsigned long long>(hash));
		return buffer;
	}

	uint64_t Hash(const uint8_t* pRgba, uint32_t width, uint32_t height, uint32_t stride)
	{
		XXH3_state_t state;
		XXH3_64bits_reset(&state);
		for (uint32_t y = 0; y < height; y++) {
			XXH3_64bits_update(&state, pRgba + static_cast<size_t>(y) * stride * 4, static_cast<size_t>(width) * 4);
		}
		return XXH3_64bits_digest(&state);
	}

	void DumpIfEnabled(uint64_t hash, const uint8_t* pRgba, uint32_t width, uint32_t height, uint32_t stride, const std::string& name)
	{
		static const bool bEnabled = getenv("KYA_TEXTURE_DUMP") != nullptr;
		if (!bEnabled) {
			return;
		}

		static std::mutex mutex;
		std::lock_guard<std::mutex> lock(mutex);

		std::filesystem::create_directories("textures-dump");
		const std::string path = "textures-dump/" + HashName(hash) + ".png";
		if (std::filesystem::exists(path)) {
			return;
		}

		stbi_write_png(path.c_str(), static_cast<int>(width), static_cast<int>(height), 4, pRgba, static_cast<int>(stride * 4));

		if (FILE* pNames = fopen("textures-dump/names.txt", "a")) {
			fprintf(pNames, "%s %ux%u %s\n", HashName(hash).c_str(), width, height, name.c_str());
			fclose(pNames);
		}
	}

	bool LoadReplacement(uint64_t hash, std::vector<uint8_t>& pixels, uint32_t& width, uint32_t& height)
	{
		const std::string path = "mods/textures/" + HashName(hash) + ".png";
		if (!std::filesystem::exists(path)) {
			return false;
		}

		// Mod files are untrusted: refuse anything past the Vulkan 2D limit every target supports (8192 on Apple GPUs).
		int w = 0;
		int h = 0;
		int channels = 0;
		if (!stbi_info(path.c_str(), &w, &h, &channels) || w <= 0 || h <= 0 || w > 8192 || h > 8192) {
			return false;
		}

		stbi_uc* pData = stbi_load(path.c_str(), &w, &h, &channels, 4);
		if (pData == nullptr) {
			return false;
		}

		pixels.assign(pData, pData + static_cast<size_t>(w) * h * 4);
		stbi_image_free(pData);
		width = static_cast<uint32_t>(w);
		height = static_cast<uint32_t>(h);
		return true;
	}
}
