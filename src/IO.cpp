#include "IO.h"

using namespace sf;

#define BIT(val, bit) ((val & (int)pow(2, bit)) == (int)pow(2, bit))

namespace trickfire {

std::array<bool, JOY_COUNT * 11> IO::prevButtonStates;
std::array<bool, IO::prevButtonStates.size()> IO::currButtonStates;

std::array<bool, C_REV + 1> IO::prevOIButtonStates;
std::array<bool, IO::prevOIButtonStates.size()> IO::currOIButtonStates;
sf::Mutex IO::mutex_OIvalues;
sf::Thread IO::oiThread(&IO::ThreadLoop);
bool IO::oiRunning;
int IO::oiFD;

void IO::StartOI() {
	oiFD = open("/dev/ttyACM0", O_RDWR);

	if (oiFD == -1) {
		Logger::Log(Logger::LEVEL_ERROR_CRITICAL, "Failed to open OI port");
		return;
	}

	if (!isatty(oiFD)) {
		Logger::Log(Logger::LEVEL_ERROR_CRITICAL, "OI port not TTY");
		return;
	}

	struct termios tio;
	if (tcgetattr(oiFD, &tio) < 0) {
		Logger::Log(Logger::LEVEL_ERROR_CRITICAL,
				"Error getting OI attributes");
		return;
	}

	tio.c_iflag = 0;
	tio.c_oflag = 0;
	tio.c_cflag = CS8 | CREAD | CLOCAL; // 8n1, see termios.h for more information
	tio.c_lflag = 0;
	tio.c_cc[VMIN] = 1;
	tio.c_cc[VTIME] = 5;

	if (cfsetispeed(&tio, B9600) < 0 || cfsetospeed(&tio, B9600) < 0) {
		Logger::Log(Logger::LEVEL_ERROR_CRITICAL, "Error setting OI baud");
		return;
	}

	if (tcsetattr(oiFD, TCSAFLUSH, &tio) < 0) {
		Logger::Log(Logger::LEVEL_ERROR_CRITICAL,
				"Error setting OI attributes");
		return;
	}

	oiRunning = true;
	oiThread.launch();
}

void IO::StopOI() {
	oiRunning = false;
}

void IO::ThreadLoop() {
	int index = 0;
	int max = 4;
	char values[max];
	for (int i = 0; i < 4; i++) {
		values[i] = 0;
	}

	while (oiRunning) {
		unsigned char val = 0;
		if (read(oiFD, &val, 1) > 0) {
			if (val == 255) {
				// This is the divider
				index = 0;

				mutex_OIvalues.lock();

				// Update state array
				for (unsigned int i = 0; i < prevOIButtonStates.size(); i++) {
					prevOIButtonStates[i] = currOIButtonStates[i];
				}
				for (unsigned int i = 0; i < currOIButtonStates.size(); i++) {
					currOIButtonStates[i] = BIT(values[(int )floor(i / 8)],
							i % 8);
				}

				mutex_OIvalues.unlock();
			} else {
				values[index] = val;
				index++;
				if (index >= max)
					index = max - 1;
			}

			/*for (unsigned int i = 0; i < currOIButtonStates.size(); i++) {
				std::cout << currOIButtonStates[i];
			}

			std::cout << std::endl;*/

		}
	}
}

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
	return currButtonStates[stick * 11 + button];
}

bool IO::JoyButtonTrig(unsigned int stick, unsigned int button) {
	return currButtonStates[stick * 11 + button]
			&& !prevButtonStates[stick * 11 + button];
}

bool IO::JoyButtonUntrig(unsigned int stick, unsigned int button) {
	return !currButtonStates[stick * 11 + button]
			&& prevButtonStates[stick * 11 + button];
}

bool IO::IsJoyConnected(unsigned int stick) {
	return Joystick::isConnected(stick);
}

bool IO::OIButton(unsigned int button) {
	sf::Lock oiLock(mutex_OIvalues);
	return currOIButtonStates[button];
}

bool IO::OIButtonTrig(unsigned int button) {
	sf::Lock oiLock(mutex_OIvalues);
	return !prevOIButtonStates[button] && currOIButtonStates[button];

}

bool IO::OIButtonUntrig(unsigned int button) {
	sf::Lock oiLock(mutex_OIvalues);
	return prevOIButtonStates[button] && !currOIButtonStates[button];

}

void IO::UpdateButtonStates() {
	for (unsigned int i = 0; i < prevButtonStates.size(); i++) {
		prevButtonStates[i] = currButtonStates[i];
	}
	for (unsigned int i = 0; i < currButtonStates.size(); i++) {
		unsigned int stick = i / 11;
		unsigned int button = i % 11;
		currButtonStates[i] = Joystick::isButtonPressed(stick, button);
	}
}
}
