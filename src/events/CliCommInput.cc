// $Id$

#ifdef __WIN32__
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <iostream>
#include <cstdio>
#include <unistd.h>
#include "CliCommInput.hh"
#include "CliCommOutput.hh"
#include "CommandController.hh"
#include "Scheduler.hh"

using std::cout;


namespace openmsx {

static xmlSAXHandler sax_handler;
static xmlParserCtxt* parser_context;
CliCommInput::ParseState CliCommInput::user_data;


CliCommInput::CliCommInput(CommandLineParser::ControlType type, const string& arguments)
	: lock(1), thread(this), ioType(type), ioArguments(arguments)
{
	Scheduler::instance(); // make sure it is instantiated in main thread
	thread.start();
}

CliCommInput::~CliCommInput()
{
}

void CliCommInput::cb_start_element(ParseState* user_data,
                     const xmlChar* name, const xmlChar** attrs)
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
	user_data->content = ""; // clear() doesn't compile on gcc-2.95
}

void CliCommInput::cb_end_element(ParseState* user_data, const xmlChar* name)
{
	if (user_data->unknownLevel) {
		--(user_data->unknownLevel);
		return;
	}
	switch (user_data->state) {
		case TAG_OPENMSX:
			user_data->state = START;
			break;
		case TAG_COMMAND:
			user_data->object->execute(user_data->content);
			user_data->state = TAG_OPENMSX;
			break;
		default:
			break;
	}
}

void CliCommInput::cb_text(ParseState* user_data, const xmlChar* chars, int len)
{
	if (user_data->state == TAG_COMMAND) {
		user_data->content.append((const char*)chars, len);
	}
}

void CliCommInput::run() throw()
{
#ifdef __WIN32__
	bool useNamedPipes = false;
	if (ioType == CommandLineParser::IO_PIPE) {
		OSVERSIONINFO info;
		info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
		GetVersionExA(&info);
		if (info.dwPlatformId == VER_PLATFORM_WIN32_NT) {
			useNamedPipes = true;
		}
	}
	
	HANDLE pipeHandle = INVALID_HANDLE_VALUE;
		
	if (useNamedPipes) {
		static const char namebase[] = {"\\\\.\\pipe\\"};
		char* pipeName = new char[strlen(namebase) + ioArguments.size() + 1];
		strcpy(pipeName, namebase);
		strcat(pipeName, ioArguments.c_str());
		pipeHandle = CreateFileA(pipeName,GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
		if (pipeHandle == INVALID_HANDLE_VALUE) {
			char msg[256];
			snprintf(msg, 255, "Error reopening pipefile '%s': error %u",pipeName, (unsigned int)GetLastError());
			MessageBoxA(NULL, msg, "Error", MB_OK);
		}
	}
#endif
	user_data.state = START;
	user_data.unknownLevel = 0;
	user_data.object = this;
	memset(&sax_handler, 0, sizeof(sax_handler));
	sax_handler.startElement = (startElementSAXFunc)cb_start_element;
	sax_handler.endElement   = (endElementSAXFunc)  cb_end_element;
	sax_handler.characters   = (charactersSAXFunc)  cb_text;
	
	parser_context = xmlCreatePushParserCtxt(&sax_handler, &user_data, 0, 0, 0);

	char buf[4096];
	while (true) {
		ssize_t n = -1;
#ifdef __WIN32__
		if (useNamedPipes) {
			unsigned long bytesRead;
			if (ReadFile(pipeHandle, buf, 4096, &bytesRead, NULL)) {
				n = (ssize_t)bytesRead;
			} else {
				CloseHandle(pipeHandle); // read error so close it
			}
		} else {
			n = read(STDIN_FILENO, buf, 4096); // just use stdio in win 9x or if the user wants to
		}
#else
		n = read(STDIN_FILENO, buf, 4096); // and in non-windows systems
#endif
		if (n <= 0) {
			break;
		}
		xmlParseChunk(parser_context, buf, n, 0);
	}
	xmlFreeParserCtxt(parser_context);
}

void CliCommInput::execute(const string& command)
{
	lock.down();
	cmds.push_back(command);
	lock.up();
	Scheduler::instance().setAsyncPoint(this);
}

void CliCommInput::executeUntil(const EmuTime& time, int userData) throw()
{
	CommandController& controller = CommandController::instance();
	lock.down();
	while (!cmds.empty()) {
		try {
			string result = controller.executeCommand(cmds.front());
			CliCommOutput::instance().reply(CliCommOutput::OK, result);
		} catch (CommandException &e) {
			string result = e.getMessage() + '\n';
			CliCommOutput::instance().reply(CliCommOutput::NOK, result);
		}
		cmds.pop_front();
	}
	lock.up();
}

const string& CliCommInput::schedName() const
{
	static const string NAME("CliCommInput");
	return NAME;
}

CliCommInput::ParseState::ParseState()
{
}

} // namespace openmsx
