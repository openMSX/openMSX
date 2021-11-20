/**
 * Example implementation for bidirectional communication with openMSX.
 *
 *  requires: libxml2
 *  compile:
 *    *nix:  g++ `xml2-config --cflags` `xml2-config --libs` openmsx-control-socket.cc
 *    win32: g++ `xml2-config --cflags` `xml2-config --libs` openmsx-control-socket.cc -lwsock32
 */

#include <string>
#include <deque>
#include <vector>
#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <libxml/parser.h>
#include <unistd.h>
#include <dirent.h>

#ifdef _WIN32
#include <fstream>
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <pwd.h>
#endif

using std::cout;
using std::endl;
using std::deque;
using std::string;
using std::vector;


class ReadDir
{
public:
	ReadDir(const std::string& directory) {
		dir = opendir(directory.c_str());
	}
	~ReadDir() {
		if (dir) {
			closedir(dir);
		}
	}

	dirent* getEntry() {
		if (!dir) {
			return 0;
		}
		return readdir(dir);
	}

private:
	DIR* dir;
};

static string getTempDir()
{
	const char* result = NULL;
	if (!result) result = getenv("TMPDIR");
	if (!result) result = getenv("TMP");
	if (!result) result = getenv("TEMP");
	if (!result) {
#ifdef _WIN32
		result = "C:/WINDOWS/TEMP";
#else
		result = "/tmp";
#endif
	}
	return result;
}

static string getUserName()
{
#ifdef _WIN32
	return "default";
#else
	struct passwd* pw = getpwuid(getuid());
	return pw->pw_name ? pw->pw_name : "";
#endif
}


class OpenMSXComm
{
public:
	// main loop
	void start(int sd);

	// send a command to openmsx
	void sendCommand(const string& command);

private:
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

	// commands being executed
	deque<string> commandStack;

	// XML parsing
	enum State {
		START,
		TAG_OPENMSX,
		TAG_REPLY,
		TAG_LOG,
		TAG_UPDATE,
	} state;
	unsigned unknownLevel;
	string content;
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

	string updateType;
	string updateName;

	// communication with openmsx process
	int sd;
};


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
	updateType = "unknown";
	if (attrs) {
		for ( ; *attrs; attrs += 2) {
			if (strcmp(attrs[0], "type") == 0) {
				updateType = attrs[1];
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
		default:
			break;
	}
}

void OpenMSXComm::doReply()
{
	switch (replyStatus) {
		case REPLY_OK:
			cout << "OK: ";
			break;
		case REPLY_NOK:
			cout << "ERR: ";
			break;
		case REPLY_UNKNOWN:
			// ignore
			break;
	}
	cout << commandStack.front() << endl;
	commandStack.pop_front();
	if (!content.empty()) {
		cout << content << endl;
	}
}

void OpenMSXComm::doLog()
{
	switch (logLevel) {
		case LOG_INFO:
			cout << "INFO: ";
			break;
		case LOG_WARNING:
			cout << "WARNING: ";
			break;
		case LOG_UNKNOWN:
			// ignore
			break;
	}
	cout << content << endl;
}

void OpenMSXComm::doUpdate()
{
	cout << "UPDATE: " << updateType << " " << updateName << " " << content << endl;
}

void OpenMSXComm::cb_text(OpenMSXComm* comm, const xmlChar* chars, int len)
{
	switch (comm->state) {
		case TAG_REPLY:
		case TAG_LOG:
		case TAG_UPDATE:
			comm->content.append((const char*)chars, len);
			break;
		default:
			break;
	}
}


void OpenMSXComm::sendCommand(const string& command)
{
	write(sd, "<command>", 9);
	write(sd, command.c_str(), command.length());
	write(sd, "</command>", 10);
	commandStack.push_back(command);
}

void OpenMSXComm::start(int sd_)
{
	sd = sd_;

	// init XML parser
	state = START;
	unknownLevel = 0;
	memset(&sax_handler, 0, sizeof(sax_handler));
	sax_handler.startElement = (startElementSAXFunc)cb_start_element;
	sax_handler.endElement   = (endElementSAXFunc)  cb_end_element;
	sax_handler.characters   = (charactersSAXFunc)  cb_text;
	parser_context = xmlCreatePushParserCtxt(&sax_handler, this, 0, 0, 0);

	write(sd, "<openmsx-control>", 17);

	// event loop
	string command; // (partial) input from STDIN
	while (true) {
		char buf[4096];
		fd_set rdfs;
		FD_ZERO(&rdfs);
		FD_SET(sd, &rdfs);
		FD_SET(STDIN_FILENO, &rdfs);
		select(sd + 1, &rdfs, NULL, NULL, NULL);
		if (FD_ISSET(sd, &rdfs)) {
			// data available from openMSX
			ssize_t size = read(sd, buf, 4096);
			if (size == 0) {
				// openmsx process died
				break;
			}
			xmlParseChunk(parser_context, buf, size, 0);
		}
		if (FD_ISSET(STDIN_FILENO, &rdfs)) {
			// data available from STDIN
			ssize_t size = read(STDIN_FILENO, buf, 4096);
			char* oldpos = buf;
			while (true) {
				char* pos = (char*)memchr(oldpos, '\n', size);
				if (pos) {
					unsigned num = pos - oldpos;
					command.append(oldpos, num);
					sendCommand(command);
					command.clear();
					oldpos = pos + 1;
					size -= num + 1;
				} else {
					command.append(pos, size);
					break;
				}
			}
		}
	}

	// cleanup
	xmlFreeParserCtxt(parser_context);
}


static bool checkSocketDir(const string& dir)
{
	struct stat st;
	if (stat(dir.c_str(), &st)) {
		// cannot stat
		return false;
	}
	if (!S_ISDIR(st.st_mode)) {
		// not a directory
		return false;
	}
#ifndef _WIN32
	// only do permission and owner checks on *nix
	if ((st.st_mode & 0777) != 0700) {
		// wrong permissions
		return false;
	}
	if (st.st_uid != getuid()) {
		// wrong uid
		return false;
	}
#endif
	return true;
}

static bool checkSocket(const string& socket)
{
	string dir  = socket.substr(0, socket.find_last_of('/'));
	string name = socket.substr(socket.find_last_of('/') + 1);

	if (name.substr(0, 7) != "socket.") {
		// wrong name
		return false;
	}

	struct stat st;
	if (stat(socket.c_str(), &st)) {
		// cannot stat
		return false;
	}
#ifdef _WIN32
	if (!S_ISREG(st.st_mode)) {
		// not a regular file
		return false;
	}
#else
	if (!S_ISSOCK(st.st_mode)) {
		// not a socket
		return false;
	}
#endif
#ifndef _WIN32
	// only do permission and owner checks on *nix
	if ((st.st_mode & 0777) != 0600) {
		// check will be different on win32 (!= 777) thus actually useless
		// wrong permissions
		return false;
	}
	if (st.st_uid != getuid()) {
		// does this work on win32? is this check meaningful?
		// wrong uid
		return false;
	}
#endif
	return true;
}

static void deleteSocket(const string& socket)
{
	unlink(socket.c_str()); // ignore errors
	string dir = socket.substr(0, socket.find_last_of('/'));
	rmdir(dir.c_str()); // ignore errors
}

static int openSocket(const string& socketName)
{
	if (!checkSocket(socketName)) {
		return -1;
	}

#ifdef _WIN32
	int port = -1;
	std::ifstream in(socketName.c_str());
	in >> port;
	if (port == -1) {
		return -1;
	}

	int sd = socket(AF_INET, SOCK_STREAM, 0);
	if (sd == -1) {
		return -1;
	}

	sockaddr_in addr;
	memset((char*)&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = htons(port);

#else
	int sd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sd == -1) {
		return -1;
	}

	sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, socketName.c_str());
#endif

	if (connect(sd, (sockaddr*)&addr, sizeof(addr)) == -1) {
		// It appears to be a socket but we cannot connect to it.
		// Must be a stale socket. Try to clean it up.
		deleteSocket(socketName);
		close(sd);
		return -1;
	}

	return sd;
}

void collectServers(vector<string>& servers)
{
	string dir = getTempDir() + "/openmsx-" + getUserName();
	if (!checkSocketDir(dir)) {
		return;
	}
	ReadDir readDir(dir);
	while (dirent* entry = readDir.getEntry()) {
		string socketName = dir + '/' + entry->d_name;
		int sd = openSocket(socketName);
		if (sd != -1) {
			close(sd);
			servers.push_back(socketName);
		}
	}
}


int main()
{
#ifdef _WIN32
	WSAData wsaData;
	WSAStartup(MAKEWORD(1, 1), &wsaData);
#endif

	vector<string> servers;
	collectServers(servers);
	if (servers.empty()) {
		cout << "No running openmsx found." << endl;
		return 0;
	}

	// TODO let the user pick one if there is more than 1
	int sd = openSocket(servers.front());

	OpenMSXComm comm;
	comm.start(sd);
	return 0;
}
