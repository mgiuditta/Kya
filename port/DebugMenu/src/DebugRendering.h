#pragma once

namespace Debug {
	namespace Rendering {
		void DrawContents();
		void ShowMenu(bool* bOpen);
		void Init();
		bool GetEnableEmulatedRendering();
		float GetGameAspectRatio();
		void UpdateGameResolution(float imageWidth, float imageHeight);
	}
}
