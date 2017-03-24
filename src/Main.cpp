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

#define JOY_NUM 0

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

double prevJoyX, prevJoyY;
bool prevKeyO, currKeyO;
bool prevKeyL, currKeyL;
bool prevKeyI, currKeyI;
bool prevKeyK, currKeyK;

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

	Vector2f joyXLabelSize = DrawingUtil::DrawGenericHeader("Joy X",
			Vector2f(COL1, ROW2), false, font, Color::Green, window);
	DrawingUtil::DrawCenteredAxisBar(IO::JoyX(JOY_NUM),
			Vector2f(COL1 + (joyXLabelSize.x / 2) - 20, ROW3),
			Vector2f(40, 264), Vector2f(4, 4), background, Color::Green,
			window);

	Vector2f joyYLabelSize = DrawingUtil::DrawGenericHeader("Joy Y",
			Vector2f(COL2, ROW2), false, font, Color::Green, window);
	DrawingUtil::DrawCenteredAxisBar(IO::JoyY(JOY_NUM),
			Vector2f(COL2 + (joyYLabelSize.x / 2) - 20, ROW3),
			Vector2f(40, 264), Vector2f(4, 4), background, Color::Green,
			window);

	// Camera feed
	mutex_cameraVars.lock();
	for (int i = 0; i < CAM_COUNT; i++) {
		UpdateCameraFrame(i);
	}

	int targetSize = 320;
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

		prevKeyO = currKeyO;
		prevKeyL = currKeyL;
		prevKeyI = currKeyI;
		prevKeyK = currKeyK;

		currKeyO = Keyboard::isKeyPressed(Keyboard::O);
		currKeyL = Keyboard::isKeyPressed(Keyboard::L);
		currKeyI = Keyboard::isKeyPressed(Keyboard::I);
		currKeyK = Keyboard::isKeyPressed(Keyboard::K);

		window.display();

		if (server->IsConnected()) {
			double joyDX = IO::JoyX(JOY_NUM) - prevJoyX;
			double joyDY = IO::JoyY(JOY_NUM) - prevJoyY;
			double driveScale = 1.0;

			if (sqrt(joyDX * joyDX + joyDY * joyDY) >= JOY_MIN_DELTA) {
				prevJoyX = IO::JoyX(JOY_NUM) * driveScale;
				prevJoyY = IO::JoyY(JOY_NUM) * driveScale;

				Packet packet;
				packet << DRIVE_PACKET;
				packet << IO::JoyY(JOY_NUM) * driveScale;
				packet << IO::JoyX(JOY_NUM) * driveScale;
				server->Send(packet);
			}

			if (IO::JoyButtonTrig(JOY_NUM, 3)
					|| (!IO::IsJoyConnected(JOY_NUM) && !prevKeyO && currKeyO)) {
				Packet packet;
				packet << MINER_MOVE_PACKET << 1;
				server->Send(packet);
			} else if (IO::JoyButtonUntrig(JOY_NUM, 3)
					|| (!IO::IsJoyConnected(JOY_NUM) && prevKeyO && !currKeyO)) {
				Packet packet;
				packet << MINER_MOVE_PACKET << 0;
				server->Send(packet);
			}

			if (IO::JoyButtonTrig(JOY_NUM, 2)
					|| (!IO::IsJoyConnected(JOY_NUM) && !prevKeyL && currKeyL)) {
				Packet packet;
				packet << MINER_MOVE_PACKET << -1;
				server->Send(packet);
			} else if (IO::JoyButtonUntrig(JOY_NUM, 2)
					|| (!IO::IsJoyConnected(JOY_NUM) && prevKeyL && !currKeyL)) {
				Packet packet;
				packet << MINER_MOVE_PACKET << 0;
				server->Send(packet);
			}

			if (IO::JoyButtonTrig(JOY_NUM, 4)
					|| (!IO::IsJoyConnected(JOY_NUM) && !prevKeyI && currKeyI)) {
				Packet packet;
				packet << MINER_SPIN_PACKET << 1;
				server->Send(packet);
			} else if (IO::JoyButtonUntrig(JOY_NUM, 4)
					|| (!IO::IsJoyConnected(JOY_NUM) && prevKeyI && !currKeyI)) {
				Packet packet;
				packet << MINER_SPIN_PACKET << 0;
				server->Send(packet);
			}

			if (IO::JoyButtonTrig(JOY_NUM, 5)
					|| (!IO::IsJoyConnected(JOY_NUM) && !prevKeyK && currKeyK)) {
				Packet packet;
				packet << MINER_SPIN_PACKET << -1;
				server->Send(packet);
			} else if (IO::JoyButtonUntrig(JOY_NUM, 5)
					|| (!IO::IsJoyConnected(JOY_NUM) && prevKeyK && !currKeyK)) {
				Packet packet;
				packet << MINER_SPIN_PACKET << 0;
				server->Send(packet);
			}
		}
	}

	return NULL;
}

int main() {
	Logger::SetLoggingLevel(Logger::LEVEL_INFO_FINE);

	Server server(25565);
	server.SetMessageCallback(PacketReceived);

	pthread_t windowThread;
	pthread_create(&windowThread, NULL, WindowThread, (void *) &server);

	pthread_join(windowThread, NULL);

	server.Disconnect();

	return 0;
}
