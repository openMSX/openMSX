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
#include "TclObject.hh"
#include "TemporaryString.hh"
#include "XMLElement.hh"
#include "cstdiop.hh"
#include "openmsx.hh"
#include "ranges.hh"
#include "unistdp.hh"
#include <cassert>
#include <iostream>

#ifdef _WIN32
#include "SocketStreamWrapper.hh"
#include "SspiNegotiateServer.hh"
#endif

using std::string;

namespace openmsx {

// class CliConnection

CliConnection::CliConnection(CommandController& commandController_,
                             EventDistributor& eventDistributor_)
	: parser([this](const std::string& cmd) { execute(cmd); })
	, commandController(commandController_)
	, eventDistributor(eventDistributor_)
{
	ranges::fill(updateEnabled, false);

	eventDistributor.registerEventListener(EventType::CLICOMMAND, *this);
}

CliConnection::~CliConnection()
{
	eventDistributor.unregisterEventListener(EventType::CLICOMMAND, *this);
}

void CliConnection::log(CliComm::LogLevel level, std::string_view message) noexcept
{
	auto levelStr = CliComm::getLevelStrings();
	output(tmpStrCat("<log level=\"", levelStr[level], "\">",
	                 XMLElement::XMLEscape(message), "</log>\n"));
}

void CliConnection::update(CliComm::UpdateType type, std::string_view machine,
                           std::string_view name, std::string_view value) noexcept
{
	if (!getUpdateEnable(type)) return;

	auto updateStr = CliComm::getUpdateStrings();
	string tmp = strCat("<update type=\"", updateStr[type], '\"');
	if (!machine.empty()) {
		strAppend(tmp, " machine=\"", machine, '\"');
	}
	if (!name.empty()) {
		strAppend(tmp, " name=\"", XMLElement::XMLEscape(name), '\"');
	}
	strAppend(tmp, '>', XMLElement::XMLEscape(value), "</update>\n");

	output(tmp);
}

void CliConnection::startOutput()
{
	output("<openmsx-output>\n");
}

void CliConnection::start()
{
	thread = std::thread([this]() { run(); });
}

void CliConnection::end()
{
	output("</openmsx-output>\n");
	close();

	poller.abort();
	// Thread might not be running if start() was never called.
	if (thread.joinable()) {
		thread.join();
	}
}

void CliConnection::execute(const string& command)
{
	eventDistributor.distributeEvent(
		Event::create<CliCommandEvent>(command, this));
}

static TemporaryString reply(std::string_view message, bool status)
{
	return tmpStrCat("<reply result=\"", (status ? "ok" : "nok"), "\">",
	              XMLElement::XMLEscape(message), "</reply>\n");
}

int CliConnection::signalEvent(const Event& event) noexcept
{
	assert(getType(event) == EventType::CLICOMMAND);
	const auto& commandEvent = get<CliCommandEvent>(event);
	if (commandEvent.getId() == this) {
		try {
			auto result = commandController.executeCommand(
				commandEvent.getCommand(), this).getString();
			output(reply(result, true));
		} catch (CommandException& e) {
			string result = std::move(e).getMessage() + '\n';
			output(reply(result, false));
		}
	}
	return 0;
}


// class StdioConnection

constexpr int BUF_SIZE = 4096;
StdioConnection::StdioConnection(CommandController& commandController_,
                                 EventDistributor& eventDistributor_)
	: CliConnection(commandController_, eventDistributor_)
{
	startOutput();
}

StdioConnection::~StdioConnection()
{
	end();
}

void StdioConnection::run()
{
	// runs in helper thread
	while (true) {
#ifdef _WIN32
		if (poller.aborted()) break;
#else
		if (poller.poll(STDIN_FILENO)) break;
#endif
		char buf[BUF_SIZE];
		int n = read(STDIN_FILENO, buf, sizeof(buf));
		if (n > 0) {
			parser.parse(buf, n);
		} else if (n < 0) {
			break;
		}
	}
}

void StdioConnection::output(std::string_view message)
{
	std::cout << message << std::flush;
}

void StdioConnection::close()
{
	// don't close stdin/out/err
}


#ifdef _WIN32
// class PipeConnection

// INVALID_HANDLE_VALUE is #defined as  (HANDLE)(-1)
// but that gives a old-style-cast warning
static const HANDLE OPENMSX_INVALID_HANDLE_VALUE = reinterpret_cast<HANDLE>(-1);

PipeConnection::PipeConnection(CommandController& commandController_,
                               EventDistributor& eventDistributor_,
                               std::string_view name)
	: CliConnection(commandController_, eventDistributor_)
{
	string pipeName = strCat("\\\\.\\pipe\\", name);
	pipeHandle = CreateFileA(pipeName.c_str(), GENERIC_READ, 0, nullptr,
	                         OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
	if (pipeHandle == OPENMSX_INVALID_HANDLE_VALUE) {
		throw FatalError("Error reopening pipefile '", pipeName, "': error ",
		                 unsigned(GetLastError()));
	}

	shutdownEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
	if (!shutdownEvent) {
		throw FatalError("Error creating shutdown event: ", GetLastError());
	}

	startOutput();
}

PipeConnection::~PipeConnection()
{
	end();

	assert(pipeHandle == OPENMSX_INVALID_HANDLE_VALUE);
	CloseHandle(shutdownEvent);
}

static void InitOverlapped(LPOVERLAPPED overlapped)
{
	ZeroMemory(overlapped, sizeof(*overlapped));
	overlapped->hEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
	if (!overlapped->hEvent) {
		throw FatalError("Error creating overlapped event: ", GetLastError());
	}
}

static void ClearOverlapped(LPOVERLAPPED overlapped)
{
	if (overlapped->hEvent) {
		CloseHandle(overlapped->hEvent);
		overlapped->hEvent = nullptr;
	}
}

void PipeConnection::run()
{
	// runs in helper thread
	OVERLAPPED overlapped;
	InitOverlapped(&overlapped);
	HANDLE waitHandles[2] = { shutdownEvent, overlapped.hEvent };

	while (pipeHandle != OPENMSX_INVALID_HANDLE_VALUE) {
		char buf[BUF_SIZE];
		if (!ReadFile(pipeHandle, buf, BUF_SIZE, nullptr, &overlapped) &&
			GetLastError() != ERROR_IO_PENDING) {
			break; // Pipe broke
		}
		DWORD wait = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
		if (wait == WAIT_OBJECT_0 + 1) {
			DWORD bytesRead;
			if (!GetOverlappedResult(pipeHandle, &overlapped, &bytesRead, TRUE)) {
				break; // Pipe broke
			}
			parser.parse(buf, bytesRead);
		} else if (wait == WAIT_OBJECT_0) {
			break; // Shutdown
		} else {
			throw FatalError(
				"WaitForMultipleObjects returned unexpectedly: ", wait);
		}
	}

	ClearOverlapped(&overlapped);

	// We own the pipe handle, so close it here
	CloseHandle(pipeHandle);
	pipeHandle = OPENMSX_INVALID_HANDLE_VALUE;
}

void PipeConnection::output(std::string_view message)
{
	if (pipeHandle != OPENMSX_INVALID_HANDLE_VALUE) {
		std::cout << message << std::flush;
	}
}

void PipeConnection::close()
{
	SetEvent(shutdownEvent);
}
#endif // _WIN32


// class SocketConnection

SocketConnection::SocketConnection(CommandController& commandController_,
                                   EventDistributor& eventDistributor_,
                                   SOCKET sd_)
	: CliConnection(commandController_, eventDistributor_)
	, sd(sd_), established(false)
{
}

SocketConnection::~SocketConnection()
{
	end();
}

void SocketConnection::run()
{
	// runs in helper thread
#ifdef _WIN32
	bool ok;
	{
		std::lock_guard<std::mutex> lock(sdMutex);
		// Authenticate and authorize the caller
		SocketStreamWrapper stream(sd);
		SspiNegotiateServer server(stream);
		ok = server.Authenticate() && server.Authorize();
	}
	if (!ok) {
		closeSocket();
		return;
	}
#endif
	// Start output element
	established = true; // TODO needs locking?
	startOutput();

	// TODO is locking correct?
	// No need to lock in this thread because we don't write to 'sd'
	// and 'sd' only gets written to in this thread.
	while (true) {
		if (sd == OPENMSX_INVALID_SOCKET) return;
#ifndef _WIN32
		if (poller.poll(sd)) {
			break;
		}
#endif
		char buf[BUF_SIZE];
		int n = sock_recv(sd, buf, BUF_SIZE);
		if (n > 0) {
			parser.parse(buf, n);
		} else if (n < 0) {
			break;
		}
	}
	closeSocket();
}

void SocketConnection::output(std::string_view message)
{
	if (!established) { // TODO needs locking?
		// Connection isn't authorized yet (and opening tag is not
		// yet send). Ignore log and update messages for now.
		return;
	}
	const char* data = message.data();
	unsigned pos = 0;
	size_t bytesLeft = message.size();
	while (bytesLeft) {
		int bytesSend;
		{
			std::lock_guard<std::mutex> lock(sdMutex);
			if (sd == OPENMSX_INVALID_SOCKET) return;
			bytesSend = sock_send(sd, &data[pos], bytesLeft);
		}
		if (bytesSend > 0) {
			bytesLeft -= bytesSend;
			pos += bytesSend;
		} else {
			// Note: On Windows we rely on closing the socket to
			//       wake up the worker thread, on other platforms
			//       we rely on Poller.
			closeSocket();
			poller.abort();
			break;
		}
	}
}

void SocketConnection::closeSocket()
{
	std::lock_guard<std::mutex> lock(sdMutex);
	if (sd != OPENMSX_INVALID_SOCKET) {
		SOCKET _sd = sd;
		sd = OPENMSX_INVALID_SOCKET;
		sock_close(_sd);
	}
}

void SocketConnection::close()
{
	closeSocket();
}

} // namespace openmsx
