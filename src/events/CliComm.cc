// $Id$

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "CliComm.hh"
#include "CommandController.hh"
#include "CommandException.hh"
#include "Scheduler.hh"
#include "XMLElement.hh"
#include "EventDistributor.hh"
#include "LedEvent.hh"
#include "Socket.hh"
#include <map>
#include <iostream>
#include <cstdio>
#include <cassert>
#include <unistd.h>

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


// class Connection

CliComm::Connection::Connection()
	: lock(1)
{
	user_data.state = START;
	user_data.unknownLevel = 0;
	user_data.object = this;
	memset(&sax_handler, 0, sizeof(sax_handler));
	sax_handler.startElement = (startElementSAXFunc)cb_start_element;
	sax_handler.endElement   = (endElementSAXFunc)  cb_end_element;
	sax_handler.characters   = (charactersSAXFunc)  cb_text;
	
	parser_context = xmlCreatePushParserCtxt(&sax_handler, &user_data, 0, 0, 0);
}

CliComm::Connection::~Connection()
{
	xmlFreeParserCtxt(parser_context);
}

void CliComm::Connection::execute(const string& command)
{
	lock.down();
	cmds.push_back(command);
	lock.up();
	Scheduler::instance().setSyncPoint(Scheduler::ASAP, this);
}

static string reply(const string& message, bool status)
{
	return string("<reply result=\"") + (status ? "ok" : "nok") + "\">" +
	       XMLElement::XMLEscape(message) + "</reply>\n";
}

void CliComm::Connection::executeUntil(const EmuTime& /*time*/, int /*userData*/)
{
	CommandController& controller = CommandController::instance();
	lock.down();
	while (!cmds.empty()) {
		try {
			string result = controller.executeCommand(cmds.front());
			output(reply(result, true));
		} catch (CommandException &e) {
			string result = e.getMessage() + '\n';
			output(reply(result, false));
		}
		cmds.pop_front();
	}
	lock.up();
}

const string& CliComm::Connection::schedName() const
{
	static const string NAME("CliComm");
	return NAME;
}

void CliComm::Connection::cb_start_element(ParseState* user_data,
                     const xmlChar* name, const xmlChar** /*attrs*/)
{
	if (user_data->unknownLevel) {
		++(user_data->unknownLevel);
		return;
	}
	switch (user_data->state) {
		case START:
			if (strcmp((const char*)name, "openmsx-control") == 0) {
				user_data->state = TAG_OPENMSX;
			} else {
				++(user_data->unknownLevel);
			}
			break;
		case TAG_OPENMSX:
			if (strcmp((const char*)name, "command") == 0) {
				user_data->state = TAG_COMMAND;
			} else {
				++(user_data->unknownLevel);
			}
			break;
		default:
			++(user_data->unknownLevel);
			break;
	}
	user_data->content.clear();
}

void CliComm::Connection::cb_end_element(ParseState* user_data, const xmlChar* /*name*/)
{
	if (user_data->unknownLevel) {
		--(user_data->unknownLevel);
		return;
	}
	switch (user_data->state) {
		case TAG_OPENMSX:
			user_data->object->output("</openmsx-output>\n");
			user_data->object->close();
			user_data->state = END;
			break;
		case TAG_COMMAND:
			user_data->object->execute(user_data->content);
			user_data->state = TAG_OPENMSX;
			break;
		default:
			break;
	}
}

void CliComm::Connection::cb_text(ParseState* user_data, const xmlChar* chars, int len)
{
	if (user_data->state == TAG_COMMAND) {
		user_data->content.append((const char*)chars, len);
	}
}


// class StdioConnection

const int BUF_SIZE = 4096;
CliComm::StdioConnection::StdioConnection()
	: thread(this), ok(true)
{
	output("<openmsx-output>\n");
	thread.start();
}

CliComm::StdioConnection::~StdioConnection()
{
	output("</openmsx-output>\n");
}

void CliComm::StdioConnection::run()
{
	while (ok) {
		char buf[BUF_SIZE];
		int n = read(STDIN_FILENO, buf, 4096);
		if (n > 0) {
			xmlParseChunk(parser_context, buf, n, 0);
		} else if (n < 0) {
			close();
			break;
		}
	}
}

void CliComm::StdioConnection::output(const string& message)
{
	if (ok) {
		cout << message << std::flush;
	}
}

void CliComm::StdioConnection::close()
{
	ok = false;
}


#ifdef _WIN32
// class PipeConnection

CliComm::PipeConnection::PipeConnection(const string& name)
	: thread(this)
{
	string pipeName = "\\\\.\\pipe\\" + name;
	pipeHandle = CreateFileA(pipeName.c_str(), GENERIC_READ, 0, NULL,
	                         OPEN_EXISTING, 0, NULL);
	if (pipeHandle == INVALID_HANDLE_VALUE) {
		char msg[256];
		snprintf(msg, 255, "Error reopening pipefile '%s': error %u",
		         pipeName, (unsigned int)GetLastError());
		throw FatalError(msg);
	}

	output("<openmsx-output>\n");
	thread.start();
}

CliComm::PipeConnection::~PipeConnection()
{
	output("</openmsx-output>\n");
}

void CliComm::PipeConnection::run()
{
	while (pipeHandle != -1) {
		char buf[BUF_SIZE];
		unsigned long bytesRead;
		if (!ReadFile(pipeHandle, buf, BUF_SIZE, &bytesRead, NULL)) {
			close();
			break;
		}
		xmlParseChunk(parser_context, buf, bytesRead, 0);
	}
}

void CliComm::PipeConnection::output(const std::string& message)
{
	if (pipeHandle != -1) {
		cout << message << std::flush;
	}
}

void CliComm::PipeConnection::close()
{
	CloseHandle(pipeHandle);
	pipeHandle = -1;
}
#endif // _WIN32


// class SocketConnection

CliComm::SocketConnection::SocketConnection(SOCKET sd_)
	: thread(this), sd(sd_)
{
	output("<openmsx-output>\n");
	thread.start();
}

CliComm::SocketConnection::~SocketConnection()
{
	output("</openmsx-output>\n");
}

void CliComm::SocketConnection::run()
{
	while (sd != -1) {
		char buf[BUF_SIZE];
		int n = sock_recv(sd, buf, BUF_SIZE);
		if (n > 0) {
			xmlParseChunk(parser_context, buf, n, 0);
		} else if (n < 0) {
			close();
			break;
		}
	}
}

void CliComm::SocketConnection::output(const std::string& message)
{
	if (sd == -1) return;

	const char* data = message.data();
	unsigned pos = 0;
	unsigned num = message.size();
	while (num) {
		int n = sock_send(sd, &data[pos], num);
		if (n > 0) {
			num -= n;
		} else {
			close();
			break;
		}
	}
}

void CliComm::SocketConnection::close()
{
	sock_close(sd);
	sd = -1;
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
	server_address.sin_addr.s_addr = htonl(INADDR_ANY);
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
