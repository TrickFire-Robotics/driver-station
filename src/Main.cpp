#include <iostream>
#include <SFML/Network.hpp>
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <pthread.h>

#include <opencv.hpp>

#include "Server.h"
#include "Logger.h"
#include "DrawingUtil.h"
#include "IO.h"
#include "NetworkingConstants.h"

#define JOY_L 0
#define JOY_R 1

#define JOY_MIN_DELTA 0.01

#define COL1 128
#define COL2 300
#define COL3 472

#define ROW1 8
#define ROW2 64
#define ROW3 128
#define ROW4 ROW3+264+16

#define CAM_COUNT 5

using namespace std;
using namespace trickfire;

double prevJoyL, prevJoyR;

// Key States (previous and current)
bool prevKeyT, currKeyT;
bool prevNum0, currNum0;
bool prevNum1, currNum1;

// Whether camera images are rotated 180 degrees or not
// (to fix physical camera rotation)
bool img0Flip, img1Flip;

// Whether or not we should transmit camera images
sf::Mutex mut_Transmit;
bool transmit = true;

// Camera feed components
sf::Mutex mutex_cameraVars;
cv::Mat frameRGB[CAM_COUNT], frameRGBA[CAM_COUNT];
sf::Image image[CAM_COUNT];
sf::Texture texture[CAM_COUNT];
sf::Sprite sprite[CAM_COUNT];

/**
 * Updates the camera feed variables for display to the window
 *
 * @param cam The camera feed index to update
 */
void UpdateCameraFeedGraphics(int cam) {
	if (!frameRGB[cam].empty()) {
		cv::cvtColor(frameRGB[cam], frameRGBA[cam], cv::COLOR_BGR2RGBA);
		image[cam].create(frameRGBA[cam].cols, frameRGBA[cam].rows,
				frameRGBA[cam].ptr());
		if (texture[cam].loadFromImage(image[cam])) {
			sprite[cam].setTexture(texture[cam]);
		}
	}
}

/**
 * Called whenever a packet is received
 *
 * @param packet The packet received
 */
void PacketReceived(Packet& packet) {
	while (!packet.endOfPacket()) {
		int type = -1;
		packet >> type;

		switch (type) {
		case CAMERA_PACKET: {
			// Output the received camera data to our local varables
			int cam;
			int rows, cols;
			packet >> cam >> rows >> cols;
			mutex_cameraVars.lock();
			frameRGB[cam] = cv::Mat(rows, cols, 16);
			uint8_t* newdata = (uint8_t*) frameRGB[cam].data;

			for (int y = 0; y < frameRGB[0].rows; y++) {
				for (int x = 0; x < frameRGB[0].cols; x++) {
					packet >> newdata[y * frameRGB[cam].cols * 3 + x * 3 + 0];
					packet >> newdata[y * frameRGB[cam].cols * 3 + x * 3 + 1];
					packet >> newdata[y * frameRGB[cam].cols * 3 + x * 3 + 2];
				}
			}
			if ((cam == 0 && img0Flip) || (cam == 1 && img1Flip))
				cv::flip(frameRGB[cam], frameRGB[cam], -1);

			mutex_cameraVars.unlock();
			break;
		}
		default:
			break;
		}
	}
}

/**
 * Draws the TrickFire Driver Station header to the window
 *
 * @param font The font to write in
 * @param window The window to draw the header to
 */
void DrawTrickFireHeader(Font& font, RenderWindow& window) {
	Text header;
	header.setFont(font);
	header.setCharacterSize(60);
	header.setColor(Color::Green);
	header.setStyle(Text::Italic);
	header.setString("TrickFire Driver Station");
	header.setOrigin(0, 2 * header.getLocalBounds().height);
	header.setRotation(90);
	window.draw(header);
}

/**
 * Updates the GUI of the window with all of the necessary information
 *
 * @param font The font to draw text in
 * @param server The server to draw information from
 * @param winow The window to draw to
 */
void UpdateGUI(Font& font, Server * server, RenderWindow& window) {
	window.clear(Color::Black);

	DrawTrickFireHeader(font, window);

	Color background(64, 64, 64); // The background color for bars

	// Draw if the server is connected or not
	if (server->IsConnected()) {
		DrawingUtil::DrawGenericHeader("Connected", Vector2f(COL1, ROW1), false,
				font, Color::Green, window);
	} else {
		DrawingUtil::DrawGenericHeader("Not Connected", Vector2f(COL1, ROW1),
				false, font, Color::Red, window);
	}

	// Draw the joystick input values
	Vector2f joyLLabelSize = DrawingUtil::DrawGenericHeader("Joy L",
			Vector2f(COL1, ROW2), false, font, Color::Green, window);
	DrawingUtil::DrawCenteredAxisBar(IO::JoyY(JOY_L),
			Vector2f(COL1 + (joyLLabelSize.x / 2) - 20, ROW3),
			Vector2f(40, 264), Vector2f(4, 4), background, Color::Green,
			window);

	Vector2f joyRLabelSize = DrawingUtil::DrawGenericHeader("Joy R",
			Vector2f(COL2, ROW2), false, font, Color::Green, window);
	DrawingUtil::DrawCenteredAxisBar(IO::JoyY(JOY_R),
			Vector2f(COL2 + (joyRLabelSize.x / 2) - 20, ROW3),
			Vector2f(40, 264), Vector2f(4, 4), background, Color::Green,
			window);

	// Camera feed
	mutex_cameraVars.lock();
	for (int i = 0; i < CAM_COUNT; i++) {
		UpdateCameraFeedGraphics(i);
	}

	// If we're not transmitting a camera feed don't display it on the window
	mut_Transmit.lock();
	bool dispCam = transmit;
	mut_Transmit.unlock();

	if (dispCam) {
		int targetSize = 512;
		sprite[0].setScale((double) targetSize / texture[0].getSize().x,
				(double) targetSize / texture[0].getSize().x);
		sprite[0].setPosition(COL3, ROW2);
		window.draw(sprite[0]);
		sprite[1].setScale((double) targetSize / texture[0].getSize().x,
				(double) targetSize / texture[0].getSize().x);
		sprite[1].setPosition(COL1, ROW4);
		window.draw(sprite[1]);
		sprite[2].setScale((double) targetSize / texture[0].getSize().x,
				(double) targetSize / texture[0].getSize().x);
		sprite[2].setPosition(COL3, ROW4);
		window.draw(sprite[2]);
	}
	mutex_cameraVars.unlock();
}

/**
 * The method called once the window thread starts
 *
 * @param serv The server to dispay information from
 */
void * WindowThread(void * serv) {
	Server* server = (Server*) serv;

	Font wlmCarton;
	if (!wlmCarton.loadFromFile("wlm_carton.ttf")) {
		cerr << "Error loading font" << endl;
	}

	RenderWindow window(VideoMode(1000, 768), "TrickFire Robotics - Server");

	while (window.isOpen()) {
		Event event;
		while (window.pollEvent(event)) {
			// Handle system windon events
			if (event.type == sf::Event::Closed) {
				window.close();
			}
		}

		// Tell IO to track button states/changes
		IO::UpdateButtonStates();

		// Update the GUI
		UpdateGUI(wlmCarton, server, window);

		// Input updates
		prevKeyT = currKeyT;
		prevNum0 = currNum0;
		prevNum1 = currNum1;

		currKeyT = Keyboard::isKeyPressed(Keyboard::T);
		currNum0 = Keyboard::isKeyPressed(Keyboard::Num0);
		currNum1 = Keyboard::isKeyPressed(Keyboard::Num1);

		// Flip images if necessary
		if (prevNum0 && !currNum0) {
			img0Flip = !img0Flip;
		}

		if (prevNum1 && !currNum1) {
			img1Flip = !img1Flip;
		}

		// Draw the changes to the window
		window.display();

		// Handle input if the robot is actually connected
		if (server->IsConnected()) {
			double joyDL = IO::JoyY(JOY_L) - prevJoyL;
			double joyDR = IO::JoyY(JOY_R) - prevJoyR;
			double driveScale = 1.0;

			// If the joystick has changed enough (prevents spam if used correctly)
			if (sqrt(joyDL * joyDL + joyDR * joyDR) >= JOY_MIN_DELTA) {
				prevJoyL = IO::JoyY(JOY_L) * driveScale;
				prevJoyR = IO::JoyY(JOY_R) * driveScale;

				// Get individual wheel control inputs
				bool fl_raw = IO::JoyButton(JOY_L, 3);
				bool rl_raw = IO::JoyButton(JOY_L, 4);
				bool fr_raw = IO::JoyButton(JOY_R, 3);
				bool rr_raw = IO::JoyButton(JOY_R, 4);

				bool fl = (fl_raw == rl_raw) || fl_raw;
				bool rl = (fl_raw == rl_raw) || rl_raw;
				bool fr = (fr_raw == rr_raw) || fr_raw;
				bool rr = (fr_raw == rr_raw) || rr_raw;

				Packet packet;
				packet << DRIVE_PACKET;
				packet << IO::JoyY(JOY_L) * driveScale;
				packet << IO::JoyY(JOY_R) * driveScale;
				packet << fl << rl << fr << rr;
				server->Send(packet);
			}

			if (IO::OIButton(L_STAGE1POSU)) {
				// If anything changes/changed send refresh packet
				if (IO::OIButtonTrig(L_STAGE1POSU)
						|| IO::OIButtonTrig(L_STAGE1LIFTL)
						|| IO::OIButtonTrig(L_STAGE1LIFTR)
						|| IO::OIButtonUntrig(L_STAGE1LIFTL)
						|| IO::OIButtonUntrig(L_STAGE1LIFTR)) {
					// Send refresh packet
					Packet packet;
					packet << MINER_MOVE_S1_PACKET << 1
							<< !IO::OIButton(L_STAGE1LIFTL) * 1.0
							<< !IO::OIButton(L_STAGE1LIFTR) * 1.0;
					server->Send(packet);
				}
			} else {
				if (IO::OIButtonUntrig(L_STAGE1POSU)) {
					// Just stopped moving, send a stop packet
					Packet packet;
					packet << MINER_MOVE_S1_PACKET << 0 << 0.0 << 0.0;
					server->Send(packet);
				}
			}

			if (IO::OIButton(L_STAGE1POSD)) {
				// If anything changes send refresh packet
				if (IO::OIButtonTrig(L_STAGE1POSD)
						|| IO::OIButtonTrig(L_STAGE1LIFTL)
						|| IO::OIButtonTrig(L_STAGE1LIFTR)
						|| IO::OIButtonUntrig(L_STAGE1LIFTL)
						|| IO::OIButtonUntrig(L_STAGE1LIFTR)) {
					// Send refresh packet
					Packet packet;
					packet << MINER_MOVE_S1_PACKET << -1
							<< !IO::OIButton(L_STAGE1LIFTL) * -1.0
							<< !IO::OIButton(L_STAGE1LIFTR) * -1.0;
					server->Send(packet);
				}
			} else {
				if (IO::OIButtonUntrig(L_STAGE1POSD)) {
					// Just stopped moving, send a stop packet
					Packet packet;
					packet << MINER_MOVE_S1_PACKET << 0 << 0.0 << 0.0;
					server->Send(packet);
				}
			}

			if (IO::OIButton(L_STAGE2POSU)) {
				// If anything changes/changed send refresh packet
				if (IO::OIButtonTrig(L_STAGE2POSU)
						|| IO::OIButtonTrig(L_STAGE2LIFTL)
						|| IO::OIButtonTrig(L_STAGE2LIFTR)
						|| IO::OIButtonUntrig(L_STAGE2LIFTL)
						|| IO::OIButtonUntrig(L_STAGE2LIFTR)) {
					// Send refresh packet
					Packet packet;
					packet << MINER_MOVE_S2_PACKET << 1
							<< !IO::OIButton(L_STAGE2LIFTL) * 1.0
							<< !IO::OIButton(L_STAGE2LIFTR) * 1.0;
					server->Send(packet);
				}
			} else {
				if (IO::OIButtonUntrig(L_STAGE2POSU)) {
					// Just stopped moving, send a stop packet
					Packet packet;
					packet << MINER_MOVE_S2_PACKET << 0 << 0.0 << 0.0;
					server->Send(packet);
				}
			}

			if (IO::OIButton(L_STAGE2POSD)) {
				// If anything changes send refresh packet
				if (IO::OIButtonTrig(L_STAGE2POSD)
						|| IO::OIButtonTrig(L_STAGE2LIFTL)
						|| IO::OIButtonTrig(L_STAGE2LIFTR)
						|| IO::OIButtonUntrig(L_STAGE2LIFTL)
						|| IO::OIButtonUntrig(L_STAGE2LIFTR)) {
					// Send refresh packet
					Packet packet;
					packet << MINER_MOVE_S2_PACKET << -1
							<< !IO::OIButton(L_STAGE2LIFTL) * -1.0
							<< !IO::OIButton(L_STAGE2LIFTR) * -1.0;
					server->Send(packet);
				}
			} else {
				if (IO::OIButtonUntrig(L_STAGE2POSD)) {
					// Just stopped moving, send a stop packet
					Packet packet;
					packet << MINER_MOVE_S2_PACKET << 0 << 0.0 << 0.0;
					server->Send(packet);
				}
			}

			if (IO::OIButtonTrig(CM_DUMP)) {
				Packet packet;
				packet << MINER_SPIN_PACKET << -1;
				server->Send(packet);
			} else if (IO::OIButtonUntrig(CM_DUMP)) {
				Packet packet;
				packet << MINER_SPIN_PACKET << 0;
				server->Send(packet);
			}

			if (IO::OIButtonTrig(CM_DIG)) {
				Packet packet;
				packet << MINER_SPIN_PACKET << 1;
				server->Send(packet);
			} else if (IO::OIButtonUntrig(CM_DIG)) {
				Packet packet;
				packet << MINER_SPIN_PACKET << 0;
				server->Send(packet);
			}

			// ----- Bin Sliding -----
			if (IO::OIButton(B_POSOVERRIDE)) {
				// If it was just activated kill the slide
				if (IO::OIButtonTrig(B_POSOVERRIDE)) {
					// Send a stop command
					Packet packet;
					packet << BIN_SLIDE_PACKET << 0;
					server->Send(packet);
				}

				// Enable override buttons
				if (IO::OIButtonTrig(B_TOCOLLECT)) { // TODO: CHANGE TO MANUAL
					Packet packet;
					packet << BIN_SLIDE_PACKET << -1;
					server->Send(packet);
				} else if (IO::OIButtonUntrig(B_TOCOLLECT)) { // TODO: CHANGE TO MANUAL
					Packet packet;
					packet << BIN_SLIDE_PACKET << 0;
					server->Send(packet);
				}

				if (IO::OIButtonTrig(B_TODUMP)) { // TODO: CHANGE TO MANUAL
					Packet packet;
					packet << BIN_SLIDE_PACKET << 1;
					server->Send(packet);
				} else if (IO::OIButtonUntrig(B_TODUMP)) { // TODO: CHANGE TO MANUAL
					Packet packet;
					packet << BIN_SLIDE_PACKET << 0;
					server->Send(packet);
				}
			} else {
				if (IO::OIButtonUntrig(B_POSOVERRIDE)) {
					// Send a stop command
					Packet packet;
					packet << BIN_SLIDE_PACKET << 0;
					server->Send(packet);
				}

				if (IO::OIButtonTrig(B_TODUMP)) {
					Packet packet;
					packet << BIN_SLIDE_PACKET << 2;
					server->Send(packet);
				}
				if (IO::OIButtonTrig(B_TOCOLLECT)) {
					Packet packet;
					packet << BIN_SLIDE_PACKET << -2;
					server->Send(packet);
				}
			}

			if (IO::OIButtonTrig(C_DUMP)) {
				Packet packet;
				packet << CONVEYOR_PACKET << 1;
				server->Send(packet);
			} else if (IO::OIButtonUntrig(C_DUMP)) {
				Packet packet;
				packet << CONVEYOR_PACKET << 0;
				server->Send(packet);
			}

			if (IO::OIButtonTrig(C_REV)) {
				Packet packet;
				packet << CONVEYOR_PACKET << -1;
				server->Send(packet);
			} else if (IO::OIButtonUntrig(C_REV)) {
				Packet packet;
				packet << CONVEYOR_PACKET << 0;
				server->Send(packet);
			}

			// Camera feed toggling
			if (IO::OIButtonTrig(CM_LEVELCM) || (prevKeyT && !currKeyT)) {
				sf::Lock lock(mut_Transmit);
				transmit = !transmit;
				Packet packet;
				packet << CONVEYOR_PACKET + 1 << transmit;
				server->Send(packet);
			}
		}
	}

	return NULL;
}

int main() {
	Logger::SetLoggingLevel(Logger::LEVEL_INFO_FINE);

	IO::StartOI();

	// Start the server
	Server server(25565);
	server.SetMessageCallback(PacketReceived);


	pthread_t windowThread;
	pthread_create(&windowThread, NULL, WindowThread, (void *) &server);

	pthread_join(windowThread, NULL);

	server.Disconnect();

	IO::StopOI();

	return 0;
}
