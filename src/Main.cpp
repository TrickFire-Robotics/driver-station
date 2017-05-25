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
bool prevKeyP, currKeyP;
bool prevKeySemi, currKeySemi;
bool prevKeyO, currKeyO;
bool prevKeyL, currKeyL;
bool prevKeyI, currKeyI;
bool prevKeyK, currKeyK;
bool prevKeyU, currKeyU;
bool prevKeyJ, currKeyJ;
bool prevKeyM, currKeyM;

bool prevKeyT, currKeyT;
bool prevKeyG, currKeyG;

bool prevNum0, currNum0;
bool prevNum1, currNum1;

bool img0Flip, img1Flip;

sf::Mutex mut_Transmit;
bool transmit = true;
double liftMod = 1.0;

sf::Mutex mutex_cameraVars;
cv::Mat frameRGB[CAM_COUNT], frameRGBA[CAM_COUNT];
sf::Image image[CAM_COUNT];
sf::Texture texture[CAM_COUNT];
sf::Sprite sprite[CAM_COUNT];

void UpdateCameraFrame(int cam) {
	//cap >> frameRGB;
	//double scale = 0.2;
	//cv::resize(frameRGB, frameRGB, cv::Size(0, 0), scale, scale);
	if (!frameRGB[cam].empty()) {
		cv::cvtColor(frameRGB[cam], frameRGBA[cam], cv::COLOR_BGR2RGBA);
		image[cam].create(frameRGBA[cam].cols, frameRGBA[cam].rows,
				frameRGBA[cam].ptr());
		if (texture[cam].loadFromImage(image[cam])) {
			sprite[cam].setTexture(texture[cam]);
		}
	}
}

void PacketReceived(Packet& packet) {
	while (!packet.endOfPacket()) {
		int type = -1;
		packet >> type;

		switch (type) {
		case CAMERA_PACKET: {
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

void UpdateGUI(Font& font, Server * server, RenderWindow& window) {
	window.clear(Color::Black);

	DrawTrickFireHeader(font, window);

	Color background(64, 64, 64);

	if (server->IsConnected()) {
		DrawingUtil::DrawGenericHeader("Connected", Vector2f(COL1, ROW1), false,
				font, Color::Green, window);
	} else {
		DrawingUtil::DrawGenericHeader("Not Connected", Vector2f(COL1, ROW1),
				false, font, Color::Red, window);
	}

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
		UpdateCameraFrame(i);
	}

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
			if (event.type == sf::Event::Closed) {
				window.close();
			}
		}

		IO::UpdateButtonStates();

		UpdateGUI(wlmCarton, server, window);

		prevKeyP = currKeyP;
		prevKeySemi = currKeySemi;
		prevKeyO = currKeyO;
		prevKeyL = currKeyL;
		prevKeyI = currKeyI;
		prevKeyK = currKeyK;
		prevKeyU = currKeyU;
		prevKeyJ = currKeyJ;
		prevKeyM = currKeyM;

		prevKeyT = currKeyT;
		prevKeyG = currKeyG;

		prevNum0 = currNum0;
		prevNum1 = currNum1;

		currKeyP = Keyboard::isKeyPressed(Keyboard::P);
		currKeySemi = Keyboard::isKeyPressed(Keyboard::SemiColon);
		currKeyO = Keyboard::isKeyPressed(Keyboard::O);
		currKeyL = Keyboard::isKeyPressed(Keyboard::L);
		currKeyI = Keyboard::isKeyPressed(Keyboard::I);
		currKeyK = Keyboard::isKeyPressed(Keyboard::K);
		currKeyU = Keyboard::isKeyPressed(Keyboard::U);
		currKeyJ = Keyboard::isKeyPressed(Keyboard::J);
		currKeyM = Keyboard::isKeyPressed(Keyboard::M);

		currKeyT = Keyboard::isKeyPressed(Keyboard::T);
		currKeyG = Keyboard::isKeyPressed(Keyboard::G);

		currNum0 = Keyboard::isKeyPressed(Keyboard::Num0);
		currNum1 = Keyboard::isKeyPressed(Keyboard::Num1);

		if (prevNum0 && !currNum0) {
			img0Flip = !img0Flip;
		}

		if (prevNum1 && !currNum1) {
			img1Flip = !img1Flip;
		}

		window.display();

		if (server->IsConnected()) {
			double joyDL = IO::JoyY(JOY_L) - prevJoyL;
			double joyDR = IO::JoyY(JOY_R) - prevJoyR;
			double driveScale = 1.0;

			if (sqrt(joyDL * joyDL + joyDR * joyDR) >= JOY_MIN_DELTA) {
				prevJoyL = IO::JoyY(JOY_L) * driveScale;
				prevJoyR = IO::JoyY(JOY_R) * driveScale;

				Packet packet;
				packet << DRIVE_PACKET;
				packet << IO::JoyY(JOY_L) * driveScale;
				packet << IO::JoyY(JOY_R) * driveScale;
				server->Send(packet);
			}

			/*if (IO::JoyButtonTrig(JOY_L, 10)
			 || (!IO::IsJoyConnected(JOY_L) && !prevKeyP && currKeyP)) {
			 Packet packet;
			 packet << MINER_MOVE_S2_PACKET << 1;
			 server->Send(packet);
			 } else if (IO::JoyButtonUntrig(JOY_L, 10)
			 || (!IO::IsJoyConnected(JOY_L) && prevKeyP && !currKeyP)) {
			 Packet packet;
			 packet << MINER_MOVE_S2_PACKET << 0;
			 server->Send(packet);
			 }

			 if (IO::JoyButtonTrig(JOY_L, 9)
			 || (!IO::IsJoyConnected(JOY_L) && !prevKeySemi
			 && currKeySemi)) {
			 Packet packet;
			 packet << MINER_MOVE_S2_PACKET << -1;
			 server->Send(packet);
			 } else if (IO::JoyButtonUntrig(JOY_L, 9)
			 || (!IO::IsJoyConnected(JOY_L) && prevKeySemi
			 && !currKeySemi)) {
			 Packet packet;
			 packet << MINER_MOVE_S2_PACKET << 0;
			 server->Send(packet);
			 }*/

			/*if (IO::JoyButtonTrig(JOY_L, 5)
			 || (!IO::IsJoyConnected(JOY_L) && !prevKeyO && currKeyO)) {
			 Packet packet;
			 packet << MINER_MOVE_S1_PACKET << 1;
			 server->Send(packet);
			 } else if (IO::JoyButtonUntrig(JOY_L, 5)
			 || (!IO::IsJoyConnected(JOY_L) && prevKeyO && !currKeyO)) {
			 Packet packet;
			 packet << MINER_MOVE_S1_PACKET << 0;
			 server->Send(packet);
			 }

			 if (IO::JoyButtonTrig(JOY_L, 6)
			 || (!IO::IsJoyConnected(JOY_L) && !prevKeyL && currKeyL)) {
			 Packet packet;
			 packet << MINER_MOVE_S1_PACKET << -1;
			 server->Send(packet);
			 } else if (IO::JoyButtonUntrig(JOY_L, 6)
			 || (!IO::IsJoyConnected(JOY_L) && prevKeyL && !currKeyL)) {
			 Packet packet;
			 packet << MINER_MOVE_S1_PACKET << 0;
			 server->Send(packet);
			 }*/

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

			if (IO::OIButtonTrig(CM_LEVELCM) || (prevKeyT && !currKeyT)) {
				sf::Lock lock(mut_Transmit);
				transmit = !transmit;
				Packet packet;
				packet << CONVEYOR_PACKET + 1 << transmit;
				server->Send(packet);
			}

			/*if (prevKeyT && !currKeyT) {
			 liftMod += 0.05;
			 cout << "Lift mod now " << liftMod << endl;
			 Packet packet;
			 packet << CONVEYOR_PACKET + 1 << liftMod;
			 server->Send(packet);
			 }
			 if (prevKeyG && !currKeyG) {
			 liftMod -= 0.05;
			 cout << "Lift mod now " << liftMod << endl;
			 Packet packet;
			 packet << CONVEYOR_PACKET + 1 << liftMod;
			 server->Send(packet);
			 }*/
		}
	}

	return NULL;
}

int main() {
	Logger::SetLoggingLevel(Logger::LEVEL_INFO_FINE);

	IO::StartOI();

	Server server(25565);
	server.SetMessageCallback(PacketReceived);

	pthread_t windowThread;
	pthread_create(&windowThread, NULL, WindowThread, (void *) &server);

	pthread_join(windowThread, NULL);

	server.Disconnect();

	IO::StopOI();

	return 0;
}
