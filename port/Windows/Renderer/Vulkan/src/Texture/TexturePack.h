#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Texture replacement keyed by a hash of the decoded RGBA, so palette variants get their own entry.
// Packs live in mods/textures/<hash>.png and may be any size; UVs are normalised.
// KYA_TEXTURE_DUMP=1 writes every decoded texture to textures-dump/<hash>.png plus a names.txt line.
// Alpha keeps the PS2 range (0x80 = opaque), so replacements must keep it too.
namespace Renderer::TexturePack
{
	// stride is the source row length in pixels; only width x height is hashed and dumped.
	uint64_t Hash(const uint8_t* pRgba, uint32_t width, uint32_t height, uint32_t stride);
	void DumpIfEnabled(uint64_t hash, const uint8_t* pRgba, uint32_t width, uint32_t height, uint32_t stride, const std::string& name);
	bool LoadReplacement(uint64_t hash, std::vector<uint8_t>& pixels, uint32_t& width, uint32_t& height);
}
