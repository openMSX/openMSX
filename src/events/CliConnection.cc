// $Id$

// TODO:
// - To avoid any possible conflicts, anything called from "run" should be
//   locked.
// - Maybe document for each method whether it is called from the listener
//   thread or from the main thread?
// - Unsubscribe at CliComm after stream is closed.

#include "CliConnection.hh"
#include "EventDistributor.hh"
#include "Event.hh"
#include "CommandController.hh"
#include "CommandException.hh"
#include "XMLElement.hh"
#include "checked_cast.hh"
#include "cstdiop.hh"
#include "unistdp.hh"
#include <cassert>
#include <iostream>

#ifdef _WIN32
#include "SocketStreamWrapper.hh"
#include "SspiNegotiateServer.hh"
#endif

using std::string;

namespace openmsx {

// class CliCommandEvent

class CliCommandEvent : public Event
{
public:
	CliCommandEvent(const string& command_, const CliConnection* id_)
		: Event(OPENMSX_CLICOMMAND_EVENT)
		, command(command_), id(id_)
	{
	}
	const string& getCommand() const
	{
		return command;
	}
	const CliConnection* getId() const
	{
		return id;
	}
private:
	const string command;
	const CliConnection* id;
};


// class CliConnection

CliConnection::CliConnection(CommandController& commandController_,
                             EventDistributor& eventDistributor_)
	: thread(this)
	, commandController(commandController_)
	, eventDistributor(eventDistributor_)
{
	user_data.state = START;
	user_data.unknownLevel = 0;
	user_data.object = this;
	memset(&sax_handler, 0, sizeof(sax_handler));
	sax_handler.startElement = cb_start_element;
	sax_handler.endElement   = cb_end_element;
	sax_handler.characters   = cb_text;

	parser_context = xmlCreatePushParserCtxt(&sax_handler, &user_data, 0, 0, 0);

	for (int i = 0; i < CliComm::NUM_UPDATES; ++i) {
		updateEnabled[i] = false;
	}

	eventDistributor.registerEventListener(OPENMSX_CLICOMMAND_EVENT, *this);
}

CliConnection::~CliConnection()
{
	eventDistributor.unregisterEventListener(OPENMSX_CLICOMMAND_EVENT, *this);
	xmlFreeParserCtxt(parser_context);
}

void CliConnection::setUpdateEnable(CliComm::UpdateType type, bool value)
{
	updateEnabled[type] = value;
}

bool CliConnection::getUpdateEnable(CliComm::UpdateType type) const
{
	return updateEnabled[type];
}

void CliConnection::start()
{
	thread.start();
}

void CliConnection::end()
{
	output("</openmsx-output>\n");
	close();
}

void CliConnection::execute(const string& command)
{
	eventDistributor.distributeEvent(new CliCommandEvent(command, this));
}

static string reply(const string& message, bool status)
{
	return string("<reply result=\"") + (status ? "ok" : "nok") + "\">" +
	       XMLElement::XMLEscape(message) + "</reply>\n";
}

bool CliConnection::signalEvent(shared_ptr<const Event> event)
{
	const CliCommandEvent& commandEvent =
		checked_cast<const CliCommandEvent&>(*event);
	if (commandEvent.getId() == this) {
		try {
			string result = commandController.executeCommand(
				commandEvent.getCommand(), this);
			output(reply(result, true));
		} catch (CommandException& e) {
			string result = e.getMessage() + '\n';
			output(reply(result, false));
		}
	}
	return true;
}

void CliConnection::cb_start_element(void* user_data,
                     const xmlChar* name, const xmlChar** /*attrs*/)
{
	ParseState* parseState = static_cast<ParseState*>(user_data);
	if (parseState->unknownLevel) {
		++(parseState->unknownLevel);
		return;
	}
	switch (parseState->state) {
		case START:
			if (strcmp(reinterpret_cast<const char*>(name), "openmsx-control") == 0) {
				parseState->state = TAG_OPENMSX;
			} else {
				++(parseState->unknownLevel);
			}
			break;
		case TAG_OPENMSX:
			if (strcmp(reinterpret_cast<const char*>(name), "command") == 0) {
				parseState->state = TAG_COMMAND;
			} else {
				++(parseState->unknownLevel);
			}
			break;
		default:
			++(parseState->unknownLevel);
			break;
	}
	parseState->content.clear();
}

void CliConnection::cb_end_element(void* user_data, const xmlChar* /*name*/)
{
	ParseState* parseState = static_cast<ParseState*>(user_data);
	if (parseState->unknownLevel) {
		--(parseState->unknownLevel);
		return;
	}
	switch (parseState->state) {
		case TAG_OPENMSX:
			parseState->object->end();
			parseState->state = END;
			break;
		case TAG_COMMAND:
			parseState->object->execute(parseState->content);
			parseState->state = TAG_OPENMSX;
			break;
		default:
			break;
	}
}

void CliConnection::cb_text(void* user_data, const xmlChar* chars, int len)
{
	ParseState* parseState = static_cast<ParseState*>(user_data);
	if (parseState->state == TAG_COMMAND) {
		parseState->content.append(reinterpret_cast<const char*>(chars), len);
	}
}


// class StdioConnection

static const int BUF_SIZE = 4096;
StdioConnection::StdioConnection(CommandController& commandController,
                                 EventDistributor& eventDistributor)
	: CliConnection(commandController, eventDistributor)
	, ok(true)
{
	start();
}

StdioConnection::~StdioConnection()
{
	end();
}

void StdioConnection::run()
{
	while (ok) {
		char buf[BUF_SIZE];
		int n = read(STDIN_FILENO, buf, sizeof(buf));
		if (n > 0) {
			xmlParseChunk(parser_context, buf, n, 0);
		} else if (n < 0) {
			close();
			break;
		}
	}
}

void StdioConnection::output(const string& message)
{
	if (ok) {
		std::cout << message << std::flush;
	}
}

void StdioConnection::close()
{
	// don't close stdin/out/err
	ok = false;
}


#ifdef _WIN32
// class PipeConnection

// INVALID_HANDLE_VALUE is #defined as  (HANDLE)(-1)
// but that gives a old-style-cast warning
static const HANDLE OPENMSX_INVALID_HANDLE_VALUE = reinterpret_cast<HANDLE>(-1);

PipeConnection::PipeConnection(CommandController& commandController,
                               EventDistributor& eventDistributor,
                               const string& name)
	: CliConnection(commandController, eventDistributor)
{
	string pipeName = "\\\\.\\pipe\\" + name;
	pipeHandle = CreateFileA(pipeName.c_str(), GENERIC_READ, 0, NULL,
	                         OPEN_EXISTING, 0, NULL);
	if (pipeHandle == OPENMSX_INVALID_HANDLE_VALUE) {
		char msg[256];
		snprintf(msg, 255, "Error reopening pipefile '%s': error %u",
		         pipeName.c_str(), unsigned(GetLastError()));
		throw FatalError(msg);
	}

	start();
}

PipeConnection::~PipeConnection()
{
	end();
}

void PipeConnection::run()
{
	while (pipeHandle != OPENMSX_INVALID_HANDLE_VALUE) {
		char buf[BUF_SIZE];
		unsigned long bytesRead;
		if (!ReadFile(pipeHandle, buf, BUF_SIZE, &bytesRead, NULL)) {
			close();
			break;
		}
		xmlParseChunk(parser_context, buf, bytesRead, 0);
	}
}

void PipeConnection::output(const std::string& message)
{
	if (pipeHandle != OPENMSX_INVALID_HANDLE_VALUE) {
		std::cout << message << std::flush;
	}
}

void PipeConnection::close()
{
	if (pipeHandle != OPENMSX_INVALID_HANDLE_VALUE) {
		// TODO: Proper locking
		HANDLE _pipeHandle = pipeHandle;
		pipeHandle = OPENMSX_INVALID_HANDLE_VALUE;
		CloseHandle(_pipeHandle);
	}
}
#endif // _WIN32


// class SocketConnection

SocketConnection::SocketConnection(CommandController& commandController,
                                   EventDistributor& eventDistributor,
                                   SOCKET sd_)
	: CliConnection(commandController, eventDistributor)
	, sem(1), sd(sd_)
{
	start();
}

SocketConnection::~SocketConnection()
{
	end();
}

void SocketConnection::run()
{
	// TODO is locking correct?
	// No need to lock in this thread because we don't write to 'sd'
	// and 'sd' only gets written to in this thread.

#ifdef _WIN32
	{ // Scope to release resources before connection ends
	SocketStreamWrapper stream(sd);
	SspiNegotiateServer server(stream);
	if (!server.Authenticate() || !server.Authorize()) {
		closesocket(sd);
		return;
	}
	}
#endif
	output("<openmsx-output>\n");

	while (true) {
		if (sd == OPENMSX_INVALID_SOCKET) return;
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

void SocketConnection::output(const std::string& message)
{
	const char* data = message.data();
	unsigned pos = 0;
	size_t bytesLeft = message.size();
	while (bytesLeft) {
		int bytesSend;
		{
			ScopedLock lock(sem);
			if (sd == OPENMSX_INVALID_SOCKET) return;
			bytesSend = sock_send(sd, &data[pos], bytesLeft);
		}
		if (bytesSend > 0) {
			bytesLeft -= bytesSend;
			pos += bytesSend;
		} else {
			close();
			break;
		}
	}
}

void SocketConnection::close()
{
	ScopedLock lock(sem);
	if (sd != OPENMSX_INVALID_SOCKET) {
		SOCKET _sd = sd;
		sd = OPENMSX_INVALID_SOCKET;
		sock_close(_sd);
	}
}

} // namespace openmsx
