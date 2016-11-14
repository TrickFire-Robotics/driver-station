#include "IO.h"
#include <SFML/System.hpp>
#include <SFML/Window.hpp>

using namespace sf;

namespace trickfire {
	double IO::JoyX(unsigned int stick) {
		return Joystick::getAxisPosition(stick, Joystick::X) / 100;
	}

	double IO::JoyY(unsigned int stick) {
		return -Joystick::getAxisPosition(stick, Joystick::Y) / 100;
	}

	bool IO::JoyButton(unsigned int stick, unsigned int button) {
		return Joystick::isButtonPressed(stick, button);
	}

	bool IO::IsJoyConnected(unsigned int stick) {
		return Joystick::isConnected(stick);
	}
}
