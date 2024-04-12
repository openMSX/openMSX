/**
 * Example implementation for bidirectional communication with openMSX.
 *
 *   requires: libxml2
 *   compile: g++ `xml2-config --cflags` `xml2-config --libs` openmsx-control-stdio.cc
 */

#include <array>
#include <cstring>
#include <string>
#include <list>
#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/select.h>
#include <libxml/parser.h>


class OpenMSXComm
{
public:
	// main loop
	void start();

	// send a command to openmsx
	void sendCommand(const std::string& command);

private:
	// these methods get called when openMSX has send the corresponding tag
	void openmsx_cmd_ok(const std::string& msg);
	void openmsx_cmd_nok(const std::string& msg);
	void openmsx_cmd_info(const std::string& msg) const;
	void openmsx_cmd_warning(const std::string& msg) const;
	void openmsx_cmd_update(const std::string& name, const std::string& value) const;

	// XML parsing call-back functions
	static void cb_start_element(OpenMSXComm* comm, const xmlChar* name,
	                             const xmlChar** attrs);
	static void cb_end_element(OpenMSXComm* comm, const xmlChar* name);
	static void cb_text(OpenMSXComm* comm, const xmlChar* chars, int len);

	void parseReply(const char** attrs);
	void parseLog(const char** attrs);
	void parseUpdate(const char** attrs);

	void doReply();
	void doLog();
	void doUpdate();

	void deprecated() const;

	// commands being executed
	std::list<std::string> commandStack;

	// XML parsing
	enum State {
		START,
		TAG_OPENMSX,
		TAG_REPLY,
		TAG_LOG,
		TAG_UPDATE,
		TAG_OK,
		TAG_NOK,
		TAG_INFO,
		TAG_WARNING,
	} state;
	unsigned unknownLevel;
	std::string content;
	xmlSAXHandler sax_handler;
	xmlParserCtxt* parser_context;

	enum ReplyStatus {
		REPLY_UNKNOWN,
		REPLY_OK,
		REPLY_NOK
	} replyStatus;

	enum LogLevel {
		LOG_UNKNOWN,
		LOG_INFO,
		LOG_WARNING
	} logLevel;

	enum UpdateType {
		UPDATE_UNKNOWN,
		UPDATE_LED
	} updateType;
	std::string updateName;

	// communication with openmsx process
	int fd_out;
};


void OpenMSXComm::openmsx_cmd_ok(const std::string& msg)
{
	const std::string& command = commandStack.front();
	std::cout << "CMD: '" << command << "' executed\n";
	if (!msg.empty()) {
		std::cout << msg << '\n';
	}
	std::cout << std::flush;
	commandStack.pop_front();
}

void OpenMSXComm::openmsx_cmd_nok(const std::string& msg)
{
	const std::string& command = commandStack.front();
	std::cout << "CMD: '" << command << "' failed\n";
	if (!msg.empty()) {
		std::cout << msg << '\n';
	}
	std::cout << std::flush;
	commandStack.pop_front();
}

void OpenMSXComm::openmsx_cmd_info(const std::string& msg) const
{
	std::cout << "INFO: " << msg << '\n' << std::flush;
}

void OpenMSXComm::openmsx_cmd_warning(const std::string& msg) const
{
	std::cout << "WARNING: " << msg << '\n' << std::flush;
}

void OpenMSXComm::openmsx_cmd_update(const std::string& name, const std::string& value) const
{
	std::cout << "UPDATE: " << name << ' ' << value << '\n' << std::flush;
}

void OpenMSXComm::deprecated() const
{
	static bool alreadyPrinted = false;
	if (!alreadyPrinted) {
		alreadyPrinted = true;
		std::cout << "The openMSX you're running still uses the old communication protocol.\n"
		             "Because of this some stuff will not work (LED status for example).\n"
		             "Please upgrade to openMSX 0.4.0 or higher.\n";
	}
}

void OpenMSXComm::cb_start_element(OpenMSXComm* comm, const xmlChar* name,
                                   const xmlChar** attrs)
{
	if (comm->unknownLevel) {
		++(comm->unknownLevel);
		return;
	}
	switch (comm->state) {
		case START:
			if (strcmp((const char*)name, "openmsx-output") == 0) {
				comm->state = TAG_OPENMSX;
			} else {
				++(comm->unknownLevel);
			}
			break;
		case TAG_OPENMSX:
			if (strcmp((const char*)name, "reply") == 0) {
				comm->state = TAG_REPLY;
				comm->parseReply((const char**)attrs);
			} else if (strcmp((const char*)name, "log") == 0) {
				comm->state = TAG_LOG;
				comm->parseLog((const char**)attrs);
			} else if (strcmp((const char*)name, "update") == 0) {
				comm->state = TAG_UPDATE;
				comm->parseUpdate((const char**)attrs);
			// backwards compatiblity stuff
			} else if (strcmp((const char*)name, "ok") == 0) {
				comm->state = TAG_OK;
				comm->replyStatus = REPLY_OK;
				comm->deprecated();
			} else if (strcmp((const char*)name, "nok") == 0) {
				comm->state = TAG_NOK;
				comm->replyStatus = REPLY_NOK;
				comm->deprecated();
			} else if (strcmp((const char*)name, "info") == 0) {
				comm->state = TAG_INFO;
				comm->logLevel = LOG_INFO;
				comm->deprecated();
			} else if (strcmp((const char*)name, "warning") == 0) {
				comm->state = TAG_WARNING;
				comm->logLevel = LOG_WARNING;
				comm->deprecated();
			} else {
				++(comm->unknownLevel);
			}
			break;
		default:
			++(comm->unknownLevel);
			break;
	}
	comm->content.clear();
}

void OpenMSXComm::parseReply(const char** attrs)
{
	replyStatus = REPLY_UNKNOWN;
	if (attrs) {
		for ( ; *attrs; attrs += 2) {
			if (strcmp(attrs[0], "result") == 0) {
				if (strcmp(attrs[1], "ok") == 0) {
					replyStatus = REPLY_OK;
				} else if (strcmp(attrs[1], "nok") == 0) {
					replyStatus = REPLY_NOK;
				}
			}
		}
	}
}

void OpenMSXComm::parseLog(const char** attrs)
{
	logLevel = LOG_UNKNOWN;
	if (attrs) {
		for ( ; *attrs; attrs += 2) {
			if (strcmp(attrs[0], "level") == 0) {
				if (strcmp(attrs[1], "info") == 0) {
					logLevel = LOG_INFO;
				} else if (strcmp(attrs[1], "warning") == 0) {
					logLevel = LOG_WARNING;
				}
			}
		}
	}
}

void OpenMSXComm::parseUpdate(const char** attrs)
{
	updateType = UPDATE_UNKNOWN;
	if (attrs) {
		for ( ; *attrs; attrs += 2) {
			if (strcmp(attrs[0], "type") == 0) {
				if (strcmp(attrs[1], "led") == 0) {
					updateType = UPDATE_LED;
				}
			} else if (strcmp(attrs[0], "name") == 0) {
				updateName = attrs[1];
			}
		}
	}
}

void OpenMSXComm::cb_end_element(OpenMSXComm* comm, const xmlChar* /*name*/)
{
	if (comm->unknownLevel) {
		--(comm->unknownLevel);
		return;
	}
	switch (comm->state) {
		case TAG_OPENMSX:
			comm->state = START;
			break;
		case TAG_REPLY:
			comm->doReply();
			comm->state = TAG_OPENMSX;
			break;
		case TAG_LOG:
			comm->doLog();
			comm->state = TAG_OPENMSX;
			break;
		case TAG_UPDATE:
			comm->doUpdate();
			comm->state = TAG_OPENMSX;
			break;
		// backwards compatiblity stuff
		case TAG_OK:
			comm->openmsx_cmd_ok(comm->content);
			comm->state = TAG_OPENMSX;
			break;
		case TAG_NOK:
			comm->openmsx_cmd_nok(comm->content);
			comm->state = TAG_OPENMSX;
			break;
		case TAG_INFO:
			comm->openmsx_cmd_info(comm->content);
			comm->state = TAG_OPENMSX;
			break;
		case TAG_WARNING:
			comm->openmsx_cmd_warning(comm->content);
			comm->state = TAG_OPENMSX;
			break;
		default:
			break;
	}
}

void OpenMSXComm::doReply()
{
	switch (replyStatus) {
		case REPLY_OK:
			openmsx_cmd_ok(content);
			break;
		case REPLY_NOK:
			openmsx_cmd_nok(content);
			break;
		case REPLY_UNKNOWN:
			// ignore
			break;
	}
}

void OpenMSXComm::doLog()
{
	switch (logLevel) {
		case LOG_INFO:
			openmsx_cmd_info(content);
			break;
		case LOG_WARNING:
			openmsx_cmd_warning(content);
			break;
		case LOG_UNKNOWN:
			// ignore
			break;
	}
}

void OpenMSXComm::doUpdate()
{
	switch (updateType) {
		case UPDATE_LED:
			openmsx_cmd_update(updateName, content);
			break;
		case UPDATE_UNKNOWN:
			// ignore
			break;
	}
}

void OpenMSXComm::cb_text(OpenMSXComm* comm, const xmlChar* chars, int len)
{
	switch (comm->state) {
		case TAG_REPLY:
		case TAG_LOG:
		case TAG_UPDATE:
		case TAG_OK:
		case TAG_NOK:
		case TAG_INFO:
		case TAG_WARNING:
			comm->content.append((const char*)chars, len);
			break;
		default:
			break;
	}
}


void OpenMSXComm::sendCommand(const std::string& command)
{
	write(fd_out, "<command>", 9);
	write(fd_out, command.c_str(), command.length());
	write(fd_out, "</command>", 10);
	commandStack.push_back(command);
}


void OpenMSXComm::start()
{
	// init XML parser
	state = START;
	unknownLevel = 0;
	memset(&sax_handler, 0, sizeof(sax_handler));
	sax_handler.startElement = (startElementSAXFunc)cb_start_element;
	sax_handler.endElement   = (endElementSAXFunc)  cb_end_element;
	sax_handler.characters   = (charactersSAXFunc)  cb_text;
	parser_context = xmlCreatePushParserCtxt(&sax_handler, this, nullptr, 0, nullptr);

	// create pipes
	std::array<int, 2> pipe_to_child;
	std::array<int, 2> pipe_from_child;
	pipe(pipe_to_child.data());
	pipe(pipe_from_child.data());
	fd_out = pipe_to_child[1];

	// start openmsx sub-process
	pid_t pid = fork();
	if (pid == 0) {
		dup2(pipe_to_child[0], STDIN_FILENO);
		dup2(pipe_from_child[1], STDOUT_FILENO);
		close(pipe_to_child[0]);
		close(pipe_to_child[1]);
		close(pipe_from_child[0]);
		close(pipe_from_child[1]);
		execlp("openmsx", "openmsx", "-control", "stdio:", nullptr);
		exit(0);
	}
	close(pipe_to_child[0]);
	close(pipe_from_child[1]);

	// send initial commands
	write(pipe_to_child[1], "<openmsx-control>", 17);
	sendCommand("set power on");
	sendCommand("restoredefault renderer");

	// event loop
	std::string command; // (partial) input from STDIN
	while (true) {
		std::array<char, 4096> buf;
		fd_set rdFs;
		FD_ZERO(&rdFs);
		FD_SET(pipe_from_child[0], &rdFs);
		FD_SET(STDIN_FILENO, &rdFs);
		select(pipe_from_child[0] + 1, &rdFs, nullptr, nullptr, nullptr);
		if (FD_ISSET(pipe_from_child[0], &rdFs)) {
			// data available from openMSX
			ssize_t size = read(pipe_from_child[0], buf.data(), buf.size());
			if (size == 0) {
				// openmsx process died
				break;
			}
			xmlParseChunk(parser_context, buf.data(), size, 0);
		}
		if (FD_ISSET(STDIN_FILENO, &rdFs)) {
			// data available from STDIN
			ssize_t size = read(STDIN_FILENO, buf.data(), buf.size());
			char* oldPos = buf.data();
			while (true) {
				if (auto* pos = static_cast<char*>(memchr(oldPos, '\n', size))) {
					auto num = pos - oldPos;
					command.append(oldPos, num);
					sendCommand(command);
					command.clear();
					oldPos = pos + 1;
					size -= num + 1;
				} else {
					command.append(oldPos, size);
					break;
				}
			}
		}
	}

	// cleanup
	xmlFreeParserCtxt(parser_context);
}


int main()
{
	OpenMSXComm comm;
	comm.start();
	return 0;
}
