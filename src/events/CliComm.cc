// $Id$

#include "CliComm.hh"
#include "CommandController.hh"
#include "CommandException.hh"
#include "Scheduler.hh"
#include "XMLElement.hh"
#include "EventDistributor.hh"
#include "LedEvent.hh"
#include "CliConnection.hh"
#include "Socket.hh"
#include <map>
#include <iostream>
#include <cassert>

using std::cout;
using std::endl;
using std::map;
using std::set;
using std::string;
using std::vector;

namespace openmsx {

CliComm::CliComm()
	: updateCmd(*this)
	, commandController(CommandController::instance())
	, xmlOutput(false)
{
	Scheduler::instance(); // make sure it is instantiated in main thread
	for (int i = 0; i < NUM_UPDATES; ++i) {
		updateEnabled[i] = false;
	}
	commandController.registerCommand(&updateCmd, "update");
	EventDistributor::instance().registerEventListener(LED_EVENT, *this,
	                                           EventDistributor::NATIVE);

	serverSocket.start();
}

CliComm::~CliComm()
{
	for (Connections::const_iterator it = connections.begin();
	     it != connections.end(); ++it) {
		delete *it;
	}

	EventDistributor::instance().unregisterEventListener(LED_EVENT, *this,
	                                             EventDistributor::NATIVE);
	commandController.unregisterCommand(&updateCmd, "update");
}

CliComm& CliComm::instance()
{
	static CliComm oneInstance;
	return oneInstance;
}

void CliComm::startInput(CommandLineParser::ControlType type, const string& arguments)
{
	xmlOutput = true;

#ifdef _WIN32
	if (type == CommandLineParser::IO_PIPE) {
		OSVERSIONINFO info;
		info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
		GetVersionExA(&info);
		if (info.dwPlatformId == VER_PLATFORM_WIN32_NT) {
			connections.push_back(new PipeConnection(arguments));
		} else {
			connections.push_back(new StdioConnection());
		}
	}
#else
	type = type;            // avoid warning
	if (arguments.empty()); // avoid warning

	connections.push_back(new StdioConnection());
#endif
}

const char* const updateStr[CliComm::NUM_UPDATES] = {
	"led", "break", "setting", "plug", "unplug", "media", "status"
};
void CliComm::log(LogLevel level, const string& message)
{
	const char* const levelStr[2] = {
		"info", "warning"
	};

	if (!connections.empty()) {
		string str = string("<log level=\"") + levelStr[level] + "\">" +
		             XMLElement::XMLEscape(message) +
		             "</log>\n";
		for (Connections::const_iterator it = connections.begin();
		     it != connections.end(); ++it) {
			(*it)->output(str);
		}
	}
	if (!xmlOutput) {
		cout << levelStr[level] << ": " << message << endl;
	}
}

void CliComm::update(UpdateType type, const string& name, const string& value)
{
	assert(type < NUM_UPDATES);
	if (!updateEnabled[type]) {
		return;
	}
	map<string, string>::iterator it = prevValues[type].find(name);
	if (it != prevValues[type].end()) {
		if (it->second == value) {
			return;
		}
		it->second = value;
	} else {
		prevValues[type][name] = value;
	}
	if (!connections.empty()) {
		string str = string("<update type=\"") + updateStr[type] + '\"';
		if (!name.empty()) {
			str += " name=\"" + name + '\"';
		}
		str += '>' + XMLElement::XMLEscape(value) + "</update>\n";
		for (Connections::const_iterator it = connections.begin();
		     it != connections.end(); ++it) {
			(*it)->output(str);
		}
	}
	if (!xmlOutput) {
		cout << updateStr[type] << ": ";
		if (!name.empty()) {
			cout << name << " = ";
		}
		cout << value << endl;
	}
}

bool CliComm::signalEvent(const Event& event)
{
	static const string ON = "on";
	static const string OFF = "off";
	static map<LedEvent::Led, string> ledName;
	static bool init = false;
	if (!init) {
		init = true;
		ledName[LedEvent::POWER] = "power";
		ledName[LedEvent::CAPS]  = "caps";
		ledName[LedEvent::KANA]  = "kana";
		ledName[LedEvent::PAUSE] = "pause";
		ledName[LedEvent::TURBO] = "turbo";
		ledName[LedEvent::FDD]   = "FDD";
	}
	
	assert(event.getType() == LED_EVENT);
	const LedEvent& ledEvent = static_cast<const LedEvent&>(event);
	update(LED, ledName[ledEvent.getLed()],
	       ledEvent.getStatus() ? ON : OFF);
	return true;
}


// class ServerSocket

CliComm::ServerSocket::ServerSocket()
	: thread(this)
{
	sock_startup();
}

CliComm::ServerSocket::~ServerSocket()
{
	sock_cleanup();
}

void CliComm::ServerSocket::start()
{
	thread.start();
}

void CliComm::ServerSocket::run()
{
	try {
		mainLoop();
	} catch (MSXException& e) {
		cout << e.getMessage() << endl;
	}
}

void CliComm::ServerSocket::mainLoop()
{
	// setup listening socket
	SOCKET listenSock = socket(AF_INET, SOCK_STREAM, 0);
	if (listenSock == INVALID_SOCKET) {
		throw FatalError(sock_error());
	}
	sock_reuseAddr(listenSock);

	sockaddr_in server_address;
	memset((char*)&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	server_address.sin_port = htons(9938); // TODO port number
	if (bind(listenSock, (sockaddr*)&server_address,
	         sizeof(server_address)) == SOCKET_ERROR) {
		sock_close(listenSock);
		throw FatalError(sock_error());
	}
	listen(listenSock, SOMAXCONN);

	// main loop
	while (true) {
		// We have a new connection coming in!
		SOCKET sd = accept(listenSock, NULL, NULL);
		if (sd == INVALID_SOCKET) {
			throw FatalError(sock_error());
		}
		CliComm::instance().connections.push_back(new SocketConnection(sd));
	}
}
                

// class UpdateCmd

CliComm::UpdateCmd::UpdateCmd(CliComm& parent_)
	: parent(parent_)
{
}

static unsigned getType(const string& name)
{
	for (unsigned i = 0; i < CliComm::NUM_UPDATES; ++i) {
		if (updateStr[i] == name) {
			return i;
		}
	}
	throw CommandException("No such update type: " + name);
}

string CliComm::UpdateCmd::execute(const vector<string>& tokens)
{
	if (tokens.size() != 3) {
		throw SyntaxError();
	}
	if (tokens[1] == "enable") {
		parent.updateEnabled[getType(tokens[2])] = true;
	} else if (tokens[1] == "disable") {
		parent.updateEnabled[getType(tokens[2])] = false;
	} else {
		throw SyntaxError();
	}
	return "";
}

string CliComm::UpdateCmd::help(const vector<string>& /*tokens*/) const
{
	static const string helpText = "TODO";
	return helpText;
}

void CliComm::UpdateCmd::tabCompletion(vector<string>& tokens) const
{
	switch (tokens.size()) {
		case 2: {
			set<string> ops;
			ops.insert("enable");
			ops.insert("disable");
			CommandController::completeString(tokens, ops);
		}
		case 3: {
			set<string> types(updateStr, updateStr + NUM_UPDATES);
			CommandController::completeString(tokens, types);
		}
	}
}

} // namespace openmsx
