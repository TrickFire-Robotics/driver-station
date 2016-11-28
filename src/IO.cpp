#include "IO.h"
#include <SFML/System.hpp>
#include <SFML/Window.hpp>

using namespace sf;

namespace trickfire {
double IO::JoyX(unsigned int stick) {
	if (IsJoyConnected(stick)) {
		return Joystick::getAxisPosition(stick, Joystick::X) / 100;
	}
#if defined(JOY_SUB) and JOY_SUB == 1
	else {
		if (Keyboard::isKeyPressed(Keyboard::A)) {
			return -1.0;
		} else if (Keyboard::isKeyPressed(Keyboard::D)) {
			return 1.0;
		} else {
			return 0.0;
		}
	}
	return 0.0;
#endif
}

double IO::JoyY(unsigned int stick) {
	if (IsJoyConnected(stick)) {
		return -Joystick::getAxisPosition(stick, Joystick::Y) / 100;
	}
#if defined(JOY_SUB) and JOY_SUB == 1
	else {
		if (Keyboard::isKeyPressed(Keyboard::S)) {
			return -1.0;
		} else if (Keyboard::isKeyPressed(Keyboard::W)) {
			return 1.0;
		} else {
			return 0.0;
		}
	}
	return 0.0;
#endif
}

bool IO::JoyButton(unsigned int stick, unsigned int button) {
	return Joystick::isButtonPressed(stick, button);
}

bool IO::IsJoyConnected(unsigned int stick) {
	return Joystick::isConnected(stick);
}
}
