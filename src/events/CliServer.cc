#include "CliServer.hh"
#include "GlobalCliComm.hh"
#include "CliConnection.hh"
#include "FileOperations.hh"
#include "MSXException.hh"
#include "one_of.hh"
#include "random.hh"
#include "xrange.hh"
#include <memory>
#include <string>

#ifdef _WIN32
#include <fstream>
#include <ctime>
#else
#include <pwd.h>
#include <unistd.h>
#include <fcntl.h>
#endif

namespace openmsx {

[[nodiscard]] static std::string getUserName()
{
#if defined(_WIN32)
	return "default";
#else
	struct passwd* pw = getpwuid(getuid());
	return pw->pw_name ? pw->pw_name : std::string{};
#endif
}

[[nodiscard]] static bool checkSocketDir(zstring_view dir)
{
	auto st = FileOperations::getStat(dir);
	if (!st) {
		// error during stat()
		return false;
	}
	if (!FileOperations::isDirectory(*st)) {
		// not a directory
		return false;
	}
#ifndef _WIN32
	// only do permission and owner checks on *nix
	if ((st->st_mode & 0777) != 0700) {
		// wrong permissions
		return false;
	}
	if (st->st_uid != getuid()) {
		// wrong uid
		return false;
	}
#endif
	return true;
}

[[nodiscard]] static bool checkSocket(zstring_view socket)
{
	std::string_view name = FileOperations::getFilename(socket);
	if (!name.starts_with("socket.")) {
		return false; // wrong name
	}

	auto st = FileOperations::getStat(socket);
	if (!st) {
		// error during stat()
		return false;
	}
#ifdef _WIN32
	if (!FileOperations::isRegularFile(*st)) {
		// not a regular file
		return false;
	}
#else
	if (!S_ISSOCK(st->st_mode)) {
		// not a socket
		return false;
	}
#endif
#ifndef _WIN32
	// only do permission and owner checks on *nix
	if ((st->st_mode & 0777) != 0600) {
		// check will be different on win32 (!= 777) thus actually useless
		// wrong permissions
		return false;
	}
	if (st->st_uid != getuid()) {
		// does this work on win32? is this check meaningful?
		// wrong uid
		return false;
	}
#endif
	return true;
}

#ifdef _WIN32
[[nodiscard]] int CliServer::openPort(SOCKET listenSock)
{
	const int RANGE = 64;

	setCurrentSocketPortNumber(0);

	const int configuredPort = globalSettings.getSocketSettingsManager().getConfiguredSocketPortNumber();
	const bool isFixedPort   = globalSettings.getSocketSettingsManager().getSocketSelectionMode() == SOCKSEL_FIXED;

	const int portOffset     = isFixedPort ? 0 : random_int(0, RANGE - 1); // [0, RANGE)

	for (auto n : xrange(RANGE)) {
		int port = configuredPort + ((portOffset + n) % RANGE);
		sockaddr_in server_address;
		memset(&server_address, 0, sizeof(server_address));
		server_address.sin_family = AF_INET;
		server_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		server_address.sin_port = htons(port);
		if (bind(listenSock, reinterpret_cast<sockaddr*>(&server_address),
		         sizeof(server_address)) != -1) {
			setCurrentSocketPortNumber(port);
			return port;
		}
	}
	throw MSXException("Couldn't open socket.");
}

void CliServer::update(const SocketSettingsManager& socketSettingsManager) noexcept
{
	if (listenSock != OPENMSX_INVALID_SOCKET) {
		sock_close(listenSock);
		listenSock = OPENMSX_INVALID_SOCKET;
		thread.join();
	}

	deleteSocketFile(socketName);

	try {
		listenSock = createSocket();
		thread = std::thread([this]() { mainLoop(); });
	}
	catch (MSXException& e) {
		cliComm.printWarning(e.getMessage());
	}
}
#endif

SOCKET CliServer::createSocket()
{
	auto dir = tmpStrCat(FileOperations::getTempDir(), "/openmsx-", getUserName());
	FileOperations::mkdir(dir, 0700);
	if (!checkSocketDir(dir)) {
		throw MSXException("Couldn't create socket directory.");
	}
	socketName = strCat(dir, "/socket.", int(getpid()));

#ifdef _WIN32
	SOCKET sd = socket(AF_INET, SOCK_STREAM, 0);
	if (sd == OPENMSX_INVALID_SOCKET) {
		throw MSXException(sock_error());
	}
	int portNumber = openPort(sd);

	// write port number to file
	FileOperations::unlink(socketName); // ignore error
	std::ofstream out;
	FileOperations::openOfStream(out, socketName);
	out << portNumber << '\n';
	if (!out.good()) {
		sock_close(sd);
		throw MSXException("Couldn't write socket port file.");
	}

#else
	SOCKET sd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sd == OPENMSX_INVALID_SOCKET) {
		throw MSXException(sock_error());
	}

	FileOperations::unlink(socketName); // ignore error

	sockaddr_un addr;
	strncpy(addr.sun_path, socketName.c_str(), sizeof(addr.sun_path) - 1);
	addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';
	addr.sun_family = AF_UNIX;

	if (bind(sd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1) {
		sock_close(sd);
		throw MSXException("Couldn't bind socket.");
	}
	if (chmod(socketName.c_str(), 0600) == -1) {
		sock_close(sd);
		throw MSXException("Couldn't set socket permissions.");
	}

#endif
	if (!checkSocket(socketName)) {
		sock_close(sd);
		throw MSXException("Opened socket fails sanity check.");
	}
	if (listen(sd, SOMAXCONN) == SOCKET_ERROR) {
		sock_close(sd);
		throw MSXException("Couldn't listen to socket: ", sock_error());
	}
	return sd;
}

void CliServer::exitAcceptLoop()
{
	sock_close(listenSock);
	poller.abort();
}

void CliServer::deleteSocketFile(const std::string& socketFileName)
{
	FileOperations::unlink(socketFileName); // ignore errors
	auto dir = socketFileName.substr(0, socketFileName.find_last_of('/'));
	FileOperations::rmdir(dir); // ignore errors
}


CliServer::CliServer(CommandController& commandController_,
					EventDistributor& eventDistributor_,
					GlobalCliComm& cliComm_,
					GlobalSettings& globalSettings_)
	: globalSettings(globalSettings_)
	, commandController(commandController_)
	, eventDistributor(eventDistributor_)
	, cliComm(cliComm_)
	, listenSock(OPENMSX_INVALID_SOCKET)
{
#ifdef _WIN32
	globalSettings.getSocketSettingsManager().attach(*this);
#endif

	sock_startup();
	try {
		listenSock = createSocket();
		thread = std::thread([this]() { mainLoop(); });
	} catch (MSXException& e) {
		cliComm.printWarning(e.getMessage());
	}
}

CliServer::~CliServer()
{
#ifdef _WIN32
	globalSettings.getSocketSettingsManager().detach(*this);
#endif

	if (listenSock != OPENMSX_INVALID_SOCKET) {
		exitAcceptLoop();
		thread.join();
	}

	deleteSocketFile(socketName);
	sock_cleanup();
}

void CliServer::mainLoop()
{
#ifndef _WIN32
	// Set socket to non-blocking to make sure accept() doesn't hang when
	// a connection attempt is dropped between poll() and accept().
	fcntl(listenSock, F_SETFL, O_NONBLOCK);
#endif
	while (true) {
		// wait for incoming connection
		// Note: On Windows, closing the socket is sufficient to exit the
		//       accept() call.
#ifndef _WIN32
		if (poller.poll(listenSock)) {
			break;
		}
#endif
		SOCKET sd = accept(listenSock, nullptr, nullptr);
		if (poller.aborted()) {
			if (sd != OPENMSX_INVALID_SOCKET) {
				sock_close(sd);
			}
			break;
		}
		if (sd == OPENMSX_INVALID_SOCKET) {
			if (errno == one_of(EAGAIN, EWOULDBLOCK)) {
				continue;
			} else {
				break;
			}
		}
#ifndef _WIN32
		// The BSD/OSX sockets implementation inherits O_NONBLOCK, while Linux
		// does not. To be on the safe side, we explicitly reset file flags.
		fcntl(sd, F_SETFL, 0);
#endif
		cliComm.addListener(std::make_unique<SocketConnection>(
			commandController, eventDistributor, sd, globalSettings));
	}
}

} // namespace openmsx
