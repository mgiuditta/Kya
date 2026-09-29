#pragma once

#include <cstdint>
#include <string>

#include "VulkanIncludes.h"
#include "GIFReg.h"

namespace Renderer
{
	struct SimpleTexture;
	struct SimpleMesh;

	namespace Native
	{
		struct BlendingState
		{
			uint32_t hwBlendMode;
		};

		struct ResolvedBlendState
		{
			uint8_t blendIndex = 0;
			uint32_t hwBlendMode = 0;
			VkPipelineColorBlendAttachmentState colorBlendAttachment{};
		};

		ResolvedBlendState ResolveBlendState(const GIFReg::GSAlpha& alpha, bool bAlphaBlendEnabled);

	}
}