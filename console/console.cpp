#include <algorithm>
#include <cstring>		//memset
#include <netinet/in.h>
#include <signal.h>
#include <sstream>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

#include "datatypes.h"
#include "railcontrol.h"
#include "util.h"
#include "console.h"

using std::map;
using std::thread;
using std::string;
using std::stringstream;
using std::vector;

namespace console {

Console::Console(Manager& manager, const unsigned short port) :
	ManagerInterface(MANAGER_ID_CONSOLE),
	port(port),
	serverSocket(0),
	clientSocket(-1),
	run(false),
	manager(manager) {

	run = true;
	struct sockaddr_in6 server_addr;

	xlog("Starting console on port %i", port);

	// create server socket
	serverSocket = socket(AF_INET6, SOCK_STREAM, 0);
	if (serverSocket < 0) {
		xlog("Unable to create socket for console. Unable to serve clients.");
		run = false;
		return;
	}

	// bind socket to an address (in6addr_any)
	memset((char *) &server_addr, 0, sizeof(server_addr));
	server_addr.sin6_family = AF_INET6;
	server_addr.sin6_addr = in6addr_any;
	server_addr.sin6_port = htons(port);

	int on = 1;
	if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (const void*)&on, sizeof(on)) < 0) {
		xlog("Unable to set console socket option SO_REUSEADDR.");
	}

	if (bind(serverSocket, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
		xlog("Unable to bind socket for console to port %i. Unable to serve clients.", port);
		close(serverSocket);
		run = false;
		return;
	}

	// listen on the socket
	if (listen(serverSocket, 5) != 0) {
		xlog("Unable to listen on socket for console server on port %i. Unable to serve clients.", port);
		close(serverSocket);
		run = false;
		return;
	}

	// create seperate thread that handles the client requests
	serverThread = thread([this] { worker(); });
}

Console::~Console() {
	if (run) {
		xlog("Stopping console");
		run = false;

		// join server thread
		serverThread.join();
	}
}


// worker is a seperate thread listening on the server socket
void Console::worker() {
	fd_set set;
	struct timeval tv;
	struct sockaddr_in6 client_addr;
	socklen_t client_addr_len = sizeof(client_addr);
	while (run) {
		// wait for connection and abort on shutdown
		int ret;
		do {
			FD_ZERO(&set);
			FD_SET(serverSocket, &set);
			tv.tv_sec = 1;
			tv.tv_usec = 0;
			ret = TEMP_FAILURE_RETRY(select(FD_SETSIZE, &set, NULL, NULL, &tv));
		} while (ret == 0 && run);
		if (ret > 0 && run) {
			// accept connection
			clientSocket = accept(serverSocket, (struct sockaddr *) &client_addr, &client_addr_len);
			if (clientSocket < 0) {
				xlog("Unable to accept client connection for console: %i, %i", clientSocket, errno);
			}
			else {
				// handle client and fill into vector
				handleClient();
			}
		}
	}
}

void Console::handleClient() {
	string unused;
	string status("Welcome to railcontrol console!\nType h for help\n");
	addUpdate(unused, status);
	char buffer_in[1024];
	memset(buffer_in, 0, sizeof(buffer_in));

	while(run) {
		size_t pos = 0;
		string s;
		while(run && pos < sizeof(buffer_in) - 1 && (s.find("\n") == string::npos || s.find("\r") == string::npos)) {
			pos += recv_timeout(clientSocket, buffer_in + pos, sizeof(buffer_in) - 1 - pos, 0);
			s = string(buffer_in);
		}


		switch (s[0])
		{
			case 'f': // feedback
			{
				int i = 1;
				unsigned char input;
				// read possible blanks
				while (true) {
					input = s[i];
					if (input != ' ') {
						break;
					}
					++i;
				}
				// read pin number
				feedbackPin_t feedbackNumber = 0;
				while (true) {
					input = s[i];
					if ( input < '0' || input > '9') {
						break;
					}
					feedbackNumber *= 10;
					feedbackNumber += input - '0';
					++i;
				};
				// read possible blanks
				while (true) {
					input = s[i];
					if (input != ' ') {
						break;
					}
					++i;
				}
				// read state
				input = s[i];
				feedbackState_t state = FEEDBACK_STATE_FREE;
				if (input == 'X' || input == 'x') {
					state = FEEDBACK_STATE_OCCUPIED;
				}
				manager.feedback(MANAGER_ID_CONSOLE, feedbackNumber, state);
				break;
			}
			case 'h': // help
			{
				string status("Available commands:\nh Help\nf pin# [X]\nq Quit\n");
				addUpdate(unused, status);
				break;
			}
			case 'q': // quit
			{
				string status("Quit railcontrol console\n");
				addUpdate(unused, status);
				close(clientSocket);
				return;
			}
			default: {
				string status("Unknown command\n");
				addUpdate(unused, status);
			}
		}
	}
}

void Console::addUpdate(const string& command, const string& status) {
	if (clientSocket < 0) {
		return;
	}
	send_timeout(clientSocket, status.c_str(), status.length(), 0);
}

void Console::booster(const managerID_t managerID, const boosterStatus_t status) {
	if (status) {
		addUpdate("boosteron", "Booster is on");
	}
	else {
		addUpdate("boosteroff", "Booster is off");
	}
}

void Console::locoSpeed(const managerID_t managerID, const locoID_t locoID, const speed_t speed) {
	std::stringstream command;
	std::stringstream status;
	command << "locospeed;loco=" << locoID << ";speed=" << speed;
	status << manager.getLocoName(locoID) << " speed is " << speed;
	addUpdate(command.str(), status.str());
}

void Console::locoDirection(const managerID_t managerID, const locoID_t locoID, const direction_t direction) {
	std::stringstream command;
	std::stringstream status;
	const char* directionText = (direction ? "forward" : "reverse");
	command << "locodirection;loco=" << locoID << ";direction=" << directionText;
	status << manager.getLocoName(locoID) << " direction is " << directionText;
	addUpdate(command.str(), status.str());
}

void Console::locoFunction(const managerID_t managerID, const locoID_t locoID, const function_t function, const bool state) {
	std::stringstream command;
	std::stringstream status;
	command << "locofunction;loco=" << locoID << ";function=" << (unsigned int)function << ";on=" << (state ? "on" : "off");
	status << manager.getLocoName(locoID) << " f" << (unsigned int)function << " is " << (state ? "on" : "off");
	addUpdate(command.str(), status.str());
}

void Console::accessory(const managerID_t managerID, const accessoryID_t accessoryID, const accessoryState_t state) {
	std::stringstream command;
	std::stringstream status;
	unsigned char color;
	unsigned char on;
	char* colorText;
	char* stateText;
	datamodel::Accessory::getAccessoryTexts(state, color, on, colorText, stateText);
	command << "accessory;accessory=" << accessoryID << ";color=" << colorText << ";on=" << stateText;
	status << manager.getAccessoryName(accessoryID) << " " << colorText << " is " << stateText;
	addUpdate(command.str(), status.str());
}

void Console::feedback(const managerID_t managerID, const feedbackPin_t pin, const feedbackState_t state) {
	std::stringstream command;
	std::stringstream status;
	command << "feedback;pin=" << pin << ";state=" << (state ? "on" : "off");
	status << "Feedback " << pin << " is " << (state ? "on" : "off");
	addUpdate(command.str(), status.str());
}

void Console::block(const managerID_t managerID, const blockID_t blockID, const blockState_t state) {
	std::stringstream command;
	std::stringstream status;
	char* stateText;
	datamodel::Block::getTexts(state, stateText);
	command << "block;block=" << blockID << ";state=" << stateText;
	status << manager.getBlockName(blockID) << " is " << stateText;
	addUpdate(command.str(), status.str());
}

void Console::handleSwitch(const managerID_t managerID, const switchID_t switchID, const switchState_t state) {
	std::stringstream command;
	std::stringstream status;
	char* stateText;
	datamodel::Switch::getTexts(state, stateText);
	command << "switch;switch=" << switchID << ";state=" << stateText;
	status << manager.getSwitchName(switchID) << " is " << stateText;
	addUpdate(command.str(), status.str());
}

}; // namespace webserver