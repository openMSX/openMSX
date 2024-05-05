#ifndef CLICONNECTION_HH
#define CLICONNECTION_HH

#include "CliListener.hh"
#include "EventListener.hh"
#include "Socket.hh"
#include "CliComm.hh"
#include "AdhocCliCommParser.hh"
#include "Poller.hh"

#include "stl.hh"

#include <array>
#include <mutex>
#include <string>
#include <thread>

namespace openmsx {

class CommandController;
class EventDistributor;

class CliConnection : public CliListener, private EventListener
{
public:
	void setUpdateEnable(CliComm::UpdateType type, bool value) {
		updateEnabled[type] = value;
	}
	[[nodiscard]] bool getUpdateEnable(CliComm::UpdateType type) const {
		return updateEnabled[type];
	}

	/** Starts the helper thread.
	  * Called when this CliConnection is added to GlobalCliComm (and
	  * after it's allowed to respond to external commands).
	  * Subclasses should themself send the opening tag (startOutput()).
	  */
	void start();

protected:
	CliConnection(CommandController& commandController,
	              EventDistributor& eventDistributor);
	~CliConnection() override;

	virtual void output(std::string_view message) = 0;

	/** End this connection by sending the closing tag
	  * and then closing the stream.
	  * Subclasses should call this method at the start of their destructor.
	  */
	void end();

	/** Close the connection. After this method is called, calls to
	  * output() should be ignored.
	  */
	virtual void close() = 0;

	/** Send opening XML tag, should be called exactly once by a subclass
	  * shortly after opening a connection. Cannot be implemented in the
	  * base class because some subclasses (want to send data before this
	  * tag).
	  */
	void startOutput();

	AdhocCliCommParser parser;
	Poller poller;

private:
	virtual void run() = 0;

	void execute(const std::string& command);

	// CliListener
	void log(CliComm::LogLevel level, std::string_view message, float fraction) noexcept override;
	void update(CliComm::UpdateType type, std::string_view machine,
	            std::string_view name, std::string_view value) noexcept override;

	// EventListener
	bool signalEvent(const Event& event) override;

	CommandController& commandController;
	EventDistributor& eventDistributor;

	std::thread thread;

	array_with_enum_index<CliComm::UpdateType, bool> updateEnabled;
};

class StdioConnection final : public CliConnection
{
public:
	StdioConnection(CommandController& commandController,
	                EventDistributor& eventDistributor);
	~StdioConnection() override;

	void output(std::string_view message) override;

private:
	void close() override;
	void run() override;
};

#ifdef _WIN32
class PipeConnection final : public CliConnection
{
public:
	PipeConnection(CommandController& commandController,
	               EventDistributor& eventDistributor,
	               std::string_view name);
	~PipeConnection() override;

	void output(std::string_view message) override;

private:
	void close() override;
	void run() override;

	HANDLE pipeHandle;
	HANDLE shutdownEvent;
};
#endif

class SocketConnection final : public CliConnection
{
public:
	SocketConnection(CommandController& commandController,
	                 EventDistributor& eventDistributor,
	                 SOCKET sd);
	~SocketConnection() override;

	void output(std::string_view message) override;

private:
	void close() override;
	void run() override;
	void closeSocket();

	std::mutex sdMutex;
	SOCKET sd;
	bool established = false;
};

} // namespace openmsx

#endif
