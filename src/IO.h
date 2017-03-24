#ifndef IO_H_
#define IO_H_

#define JOY_SUB 1
#define JOY_COUNT 1

#include <array>

namespace trickfire {

class IO {
public:
	static double JoyX(unsigned int stick);
	static double JoyY(unsigned int stick);
	static bool JoyButton(unsigned int stick, unsigned int button);
	static bool JoyButtonTrig(unsigned int stick, unsigned int button);
	static bool JoyButtonUntrig(unsigned int stick, unsigned int button);
	static bool IsJoyConnected(unsigned int stick);

	static void UpdateButtonStates();

private:
	static std::array<bool, JOY_COUNT * 11> prevButtonStates;
	static std::array<bool, prevButtonStates.size()> currButtonStates;
};

}

#endif
