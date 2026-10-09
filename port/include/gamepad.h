#pragma once

#include <atomic>

namespace KyaGamepad
{
	void AddGamepadSupport();

	// Camera (right stick) inversion, set from the debug Input settings.
	// ponytail: applied by the GLFW backend only; the WinRT backend ignores it until someone asks.
	inline std::atomic<bool> gInvertCameraX = false;
	inline std::atomic<bool> gInvertCameraY = false;
}