#include <iostream>
#include <SFML/Network.hpp>
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <pthread.h>

#include "Server.h"
#include "Logger.h"
#include "DrawingUtil.h"
#include "IO.h"
#include "NetworkingConstants.h"

#define JOY_NUM 0

#define JOY_MIN_DELTA 0.01

#define COL1 128
#define COL2 300

#define ROW1 8
#define ROW2 64
#define ROW3 128

using namespace std;
using namespace trickfire;

double prevJoyX, prevJoyY;

void PacketReceived(Packet& packet) {
	while (!packet.endOfPacket()) {
		int i;

		packet >> i;

		cout << "Received: " << i << endl;
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
}

void * WindowThread(void * serv) {
	Server* server = (Server*) serv;

	Font wlmCarton;
	if (!wlmCarton.loadFromFile("wlm_carton.ttf")) {
		cerr << "Error loading font" << endl;
	}

	RenderWindow window(VideoMode(500, 768), "TrickFire Robotics - Server");

	while (window.isOpen()) {

		Event event;
		while (window.pollEvent(event)) {
			if (event.type == sf::Event::Closed) {
				window.close();
			}
		}

		IO::UpdateButtonStates();

		UpdateGUI(wlmCarton, server, window);

		window.display();

		if (server->IsConnected()) {
			double joyDX = IO::JoyX(JOY_NUM) - prevJoyX;
			double joyDY = IO::JoyY(JOY_NUM) - prevJoyY;

			if (sqrt(joyDX * joyDX + joyDY * joyDY) >= JOY_MIN_DELTA) {
				prevJoyX = IO::JoyX(JOY_NUM);
				prevJoyY = IO::JoyY(JOY_NUM);

				Packet packet;
				packet << DRIVE_PACKET;
				packet << IO::JoyY(JOY_NUM);
				packet << IO::JoyX(JOY_NUM);
				server->Send(packet);
			}

			if (IO::JoyButtonTrig(JOY_NUM, 0)) {
				Packet packet;
				packet << AUTO_PACKET_1;
				//server->Send(packet);
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
