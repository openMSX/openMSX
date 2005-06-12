// $Id$

// TODO:
// - To avoid any possible conflicts, anything called from "run" should be
//   locked.
// - Maybe document for each method whether it is called from the listener
//   thread or from the main thread?
// - Unsubscribe at CliComm after stream is closed.

#include "CliConnection.hh"
#include "CommandController.hh"
#include "CommandException.hh"
#include "Scheduler.hh"
#include "XMLElement.hh"
#include <cstdio>
#include <iostream>
#include <unistd.h>

using std::string;

namespace openmsx {

// class CliConnection

CliConnection::CliConnection()
	: thread(this), lock(1)
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

CliConnection::~CliConnection()
{
	xmlFreeParserCtxt(parser_context);
}

void CliConnection::start()
{
	output("<openmsx-output>\n");
	thread.start();
}

void CliConnection::end()
{
	output("</openmsx-output>\n");
	close();
}

void CliConnection::execute(const string& command)
{
	ScopedLock l(lock);
	cmds.push_back(command);
	setSyncPoint(Scheduler::ASAP);
}

static string reply(const string& message, bool status)
{
	return string("<reply result=\"") + (status ? "ok" : "nok") + "\">" +
	       XMLElement::XMLEscape(message) + "</reply>\n";
}

void CliConnection::executeUntil(const EmuTime& /*time*/, int /*userData*/)
{
	CommandController& controller = CommandController::instance();
	ScopedLock l(lock);
	while (!cmds.empty()) {
		try {
			string result = controller.executeCommand(cmds.front());
			output(reply(result, true));
		} catch (CommandException& e) {
			string result = e.getMessage() + '\n';
			output(reply(result, false));
		}
		cmds.pop_front();
	}
}

const string& CliConnection::schedName() const
{
	static const string NAME("CliConnection");
	return NAME;
}

void CliConnection::cb_start_element(ParseState* user_data,
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

void CliConnection::cb_end_element(ParseState* user_data, const xmlChar* /*name*/)
{
	if (user_data->unknownLevel) {
		--(user_data->unknownLevel);
		return;
	}
	switch (user_data->state) {
		case TAG_OPENMSX:
			user_data->object->end();
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

void CliConnection::cb_text(ParseState* user_data, const xmlChar* chars, int len)
{
	if (user_data->state == TAG_COMMAND) {
		user_data->content.append((const char*)chars, len);
	}
}


// class StdioConnection

static const int BUF_SIZE = 4096;
StdioConnection::StdioConnection()
	: ok(true)
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
	if (ok) {
		// TODO: close stdout?
		::close(STDIN_FILENO);
		::close(STDOUT_FILENO);
		::close(STDERR_FILENO);
		ok = false;
	}
}


#ifdef _WIN32
// class PipeConnection

PipeConnection::PipeConnection(const string& name)
{
	string pipeName = "\\\\.\\pipe\\" + name;
	pipeHandle = CreateFileA(pipeName.c_str(), GENERIC_READ, 0, NULL,
	                         OPEN_EXISTING, 0, NULL);
	if (pipeHandle == INVALID_HANDLE_VALUE) {
		char msg[256];
		snprintf(msg, 255, "Error reopening pipefile '%s': error %u",
		         pipeName.c_str(), (unsigned int)GetLastError());
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
	while (pipeHandle != INVALID_HANDLE_VALUE) {
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
	if (pipeHandle != INVALID_HANDLE_VALUE) {
		std::cout << message << std::flush;
	}
}

void PipeConnection::close()
{
	if (pipeHandle != INVALID_HANDLE_VALUE) {
		// TODO: Proper locking
		HANDLE _pipeHandle = pipeHandle;
		pipeHandle = INVALID_HANDLE_VALUE;
		CloseHandle(_pipeHandle);
	}
}
#endif // _WIN32


// class SocketConnection

SocketConnection::SocketConnection(SOCKET sd_)
	: sd(sd_)
{
	start();
}

SocketConnection::~SocketConnection()
{
	end();
}

void SocketConnection::run()
{
	while (sd != INVALID_SOCKET) {
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
	if (sd == INVALID_SOCKET) return;

	const char* data = message.data();
	unsigned pos = 0;
	unsigned bytesLeft = message.size();
	while (bytesLeft) {
		int bytesSend = sock_send(sd, &data[pos], bytesLeft);
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
	if (sd != INVALID_SOCKET) {
		// TODO: Proper locking
		SOCKET _sd = sd;
		sd = INVALID_SOCKET;
		sock_close(_sd);
	}
}

} // namespace openmsx
