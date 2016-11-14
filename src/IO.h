#ifndef IO_H_
#define IO_H_

namespace trickfire {

class IO {
public:
	static double JoyX(unsigned int stick);
	static double JoyY(unsigned int stick);
	static bool JoyButton(unsigned int stick, unsigned int button);
	static bool IsJoyConnected(unsigned int stick);
};

}

#endif
