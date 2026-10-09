#pragma once

#include <cstdint>
#include <string>

#include "VulkanIncludes.h"
#include "Objects/Pipeline.h"

namespace Renderer
{
	namespace Native
	{
		namespace PostProcessing
		{
			enum class Effect
			{
				Greyscale,
				AlphaFix,
				Fade,
			};

			void Setup();
			void Resize();
			void AddPostProcessEffect(const VkCommandBuffer& cmd, Effect effect);
			const VkImageView& GetColorImageView();
			// KYA_FRAME_DUMP=<seconds>: once, after that many seconds, writes the final native frame at full resolution to frame-dump.png.
			// Call between frames. Used to inspect 4K output on smaller displays.
			void DumpFrameIfRequested();
			void UpdateDescriptorSets(const Effect effect, const int frameIndex, const DescriptorWriteList& writeList);

			std::string GetEffectName(Effect effect);
		}
	}
}