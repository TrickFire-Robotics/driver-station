#ifndef IO_H_
#define IO_H_

#define JOY_SUB 1
#define JOY_COUNT 2

#include <array>
#include <cmath>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <SFML/System.hpp>
#include <SFML/Window.hpp>
#include <Logger.h>

// ANALOGS
#define L_STAGE2SPEED 0
#define CM_SPEED 1

// DIGITALS
// LIFT
#define L_STAGE2SPEEDOVERRIDE 0
#define L_STAGE2POSOVERRIDE 1
#define L_STAGE1POSOVERRIDE 2
#define L_STAGE2POSU 3
#define L_STAGE2POSD 4
#define L_STAGE1POSU 5
#define L_STAGE1POSD 6
#define L_STAGE2LIFTL 7
#define L_STAGE2LIFTR 8
#define L_STAGE1LIFTL 9
#define L_STAGE1LIFTR 10
#define L_LIFTTODUMP 11
#define L_LIFTTODRIVE 12

// COAL MINER
#define CM_DUMP 13
#define CM_LEVELCM 14
#define CM_DIG 15
#define CM_SPEEDOVERRIDE 16

// BIN
#define B_TOCOLLECT 17
#define B_TODUMP 18
#define B_POSOVERRIDE 19
#define B_POSIN 20
#define B_POSOUT 21

// CONVEYOR
#define C_DUMP 22
#define C_REV 23

namespace trickfire {

class IO {
public:
	static double JoyX(unsigned int stick);
	static double JoyY(unsigned int stick);
	static bool JoyButton(unsigned int stick, unsigned int button);
	static bool JoyButtonTrig(unsigned int stick, unsigned int button);
	static bool JoyButtonUntrig(unsigned int stick, unsigned int button);
	static bool IsJoyConnected(unsigned int stick);

	static bool OIButton(unsigned int button);
	static bool OIButtonTrig(unsigned int stick);
	static bool OIButtonUntrig(unsigned int stick);

	static void UpdateButtonStates();

	static void StartOI();
	static void StopOI();

private:
	static std::array<bool, JOY_COUNT * 11> prevButtonStates;
	static std::array<bool, prevButtonStates.size()> currButtonStates;

	static std::array<bool, C_REV + 1> prevOIButtonStates;
	static std::array<bool, prevOIButtonStates.size()> currOIButtonStates;

	static int oiFD;
	static sf::Mutex mutex_OIvalues;
	static sf::Thread oiThread;
	static bool oiRunning;

	static void ThreadLoop();
};

}

#endif
