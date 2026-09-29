#include "input_functions.h"
#include "gamepad.h"
#include <GLFW/glfw3.h>

// Same behaviour as the WinRT backend (port/Windows/Input/gamepad.cpp): first connected gamepad,
// sticks in [-1, 1] with Y positive up, triggers in [0, 1], same press thresholds.
namespace GamepadImpl
{
	struct Reading
	{
		bool bConnected = false;
		unsigned char buttons[GLFW_GAMEPAD_BUTTON_LAST + 1] = {};
		float leftX = 0.0f;
		float leftY = 0.0f;
		float rightX = 0.0f;
		float rightY = 0.0f;
		float leftTrigger = 0.0f;
		float rightTrigger = 0.0f;
	};

	static Reading GetReading()
	{
		Reading reading;

		for (int joystick = GLFW_JOYSTICK_1; joystick <= GLFW_JOYSTICK_LAST; joystick++) {
			GLFWgamepadstate state;
			if (glfwJoystickIsGamepad(joystick) && glfwGetGamepadState(joystick, &state)) {
				reading.bConnected = true;
				for (int i = 0; i <= GLFW_GAMEPAD_BUTTON_LAST; i++) {
					reading.buttons[i] = state.buttons[i];
				}

				// ponytail: GLFW reports stick Y positive down; flipped to match WinRT. Verify on a real pad.
				reading.leftX = state.axes[GLFW_GAMEPAD_AXIS_LEFT_X];
				reading.leftY = -state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y];
				reading.rightX = state.axes[GLFW_GAMEPAD_AXIS_RIGHT_X];
				reading.rightY = -state.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y];

				// GLFW triggers rest at -1.
				reading.leftTrigger = (state.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] + 1.0f) * 0.5f;
				reading.rightTrigger = (state.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] + 1.0f) * 0.5f;
				break;
			}
		}

		return reading;
	}

	// GLFW button for each digital route, -1 for analog routes.
	static int GetRouteButton(uint32_t routeId)
	{
		switch (routeId)
		{
		case ROUTE_UP: return GLFW_GAMEPAD_BUTTON_DPAD_UP;
		case ROUTE_LEFT: return GLFW_GAMEPAD_BUTTON_DPAD_LEFT;
		case ROUTE_DOWN: return GLFW_GAMEPAD_BUTTON_DPAD_DOWN;
		case ROUTE_RIGHT: return GLFW_GAMEPAD_BUTTON_DPAD_RIGHT;
		case ROUTE_L1: return GLFW_GAMEPAD_BUTTON_LEFT_BUMPER;
		case ROUTE_R1: return GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER;
		case ROUTE_TRIANGLE: return GLFW_GAMEPAD_BUTTON_Y;
		case ROUTE_CIRCLE: return GLFW_GAMEPAD_BUTTON_B;
		case ROUTE_CROSS: return GLFW_GAMEPAD_BUTTON_A;
		case ROUTE_SQUARE: return GLFW_GAMEPAD_BUTTON_X;
		case ROUTE_SELECT: return GLFW_GAMEPAD_BUTTON_BACK;
		case ROUTE_START: return GLFW_GAMEPAD_BUTTON_START;
		case ROUTE_L3: return GLFW_GAMEPAD_BUTTON_LEFT_THUMB;
		case ROUTE_R3: return GLFW_GAMEPAD_BUTTON_RIGHT_THUMB;
		default: return -1;
		}
	}

	// Analog routes return the stick half remapped to positive, like the WinRT backend.
	static float GetRouteAnalog(const Reading& reading, uint32_t routeId)
	{
		switch (routeId)
		{
		case ROUTE_L2: return reading.leftTrigger;
		case ROUTE_R2: return reading.rightTrigger;
		case ROUTE_L_ANALOG_UP: return reading.leftY;
		case ROUTE_L_ANALOG_DOWN: return -reading.leftY;
		case ROUTE_L_ANALOG_LEFT: return -reading.leftX;
		case ROUTE_L_ANALOG_RIGHT: return reading.leftX;
		case ROUTE_R_ANALOG_UP: return reading.rightY;
		case ROUTE_R_ANALOG_DOWN: return -reading.rightY;
		case ROUTE_R_ANALOG_LEFT: return -reading.rightX;
		case ROUTE_R_ANALOG_RIGHT: return reading.rightX;
		default: return 0.0f;
		}
	}

	static bool IsButtonCurrentlyPressed(uint32_t routeId)
	{
		const Reading reading = GetReading();
		if (!reading.bConnected) {
			return false;
		}

		const int button = GetRouteButton(routeId);
		if (button >= 0) {
			return reading.buttons[button] == GLFW_PRESS;
		}

		const float threshold = (routeId == ROUTE_L2 || routeId == ROUTE_R2) ? 0.1f : 0.5f;
		return GetRouteAnalog(reading, routeId) > threshold;
	}

	bool GetGamepadPressed(uint32_t routeId)
	{
		static bool previousButtonState[ROUTE_END] = {};

		const bool currentlyPressed = IsButtonCurrentlyPressed(routeId);
		const bool wasPressed = previousButtonState[routeId];
		previousButtonState[routeId] = currentlyPressed;

		return !wasPressed && currentlyPressed;
	}

	bool GetGamepadReleased(uint32_t routeId)
	{
		static bool previousButtonState[ROUTE_END] = {};

		const bool currentlyPressed = IsButtonCurrentlyPressed(routeId);
		const bool wasPressed = previousButtonState[routeId];
		previousButtonState[routeId] = currentlyPressed;

		return wasPressed && !currentlyPressed;
	}

	float GetGamepadAnalog(uint32_t routeId)
	{
		const Reading reading = GetReading();
		if (!reading.bConnected) {
			return 0.0f;
		}

		const int button = GetRouteButton(routeId);
		if (button >= 0) {
			// WinRT returns nothing for L3/R3 here; keep that.
			if (routeId == ROUTE_L3 || routeId == ROUTE_R3) {
				return 0.0f;
			}
			return reading.buttons[button] == GLFW_PRESS ? 1.0f : 0.0f;
		}

		return GetRouteAnalog(reading, routeId);
	}

	bool IsAnyGamepadButtonPressed()
	{
		const Reading reading = GetReading();
		if (!reading.bConnected) {
			return false;
		}

		for (unsigned char button : reading.buttons) {
			if (button == GLFW_PRESS) {
				return true;
			}
		}

		return reading.leftTrigger > 0.0f || reading.rightTrigger > 0.0f ||
			reading.leftX != 0.0f || reading.leftY != 0.0f ||
			reading.rightX != 0.0f || reading.rightY != 0.0f;
	}
}

void KyaGamepad::AddGamepadSupport()
{
	Input::gInputFunctions.controllerPressed = GamepadImpl::GetGamepadPressed;
	Input::gInputFunctions.controllerReleased = GamepadImpl::GetGamepadReleased;
	Input::gInputFunctions.controllerAnalog = GamepadImpl::GetGamepadAnalog;
	Input::gInputFunctions.controllerAnyPressed = GamepadImpl::IsAnyGamepadButtonPressed;
}
