// $Id$

#ifndef CLICOMM_HH
#define CLICOMM_HH

#ifdef	_WIN32
	typedef void* HANDLE;
	typedef unsigned int SOCKET; // dirty but it works
#else
	typedef int SOCKET
#endif

#include "Thread.hh"
#include "Schedulable.hh"
#include "Semaphore.hh"
#include "CommandLineParser.hh"
#include "Command.hh"
#include "EventListener.hh"
#include <libxml/parser.h>
#include <map>
#include <deque>
#include <string>

namespace openmsx {

class CommandController;

class CliComm : private EventListener
{
public:
	enum LogLevel {
		INFO,
		WARNING
	};
	enum ReplyStatus {
		OK,
		NOK
	};
	enum UpdateType {
		LED,
		BREAK,
		SETTING,
		PLUG,
		UNPLUG,
		MEDIA,
		STATUS,
		NUM_UPDATES // must be last
	};
	
	static CliComm& instance();
	void startInput(CommandLineParser::ControlType type,
	                const std::string& arguments);
	
	void log(LogLevel level, const std::string& message);
	void update(UpdateType type, const std::string& name, const std::string& value);

	// convient methods
	void printInfo(const std::string& message) {
		log(INFO, message);
	}
	void printWarning(const std::string& message) {
		log(WARNING, message);
	}

private:
	CliComm();
	~CliComm();
	
	// EventListener
	virtual bool signalEvent(const Event& event);

	class Connection : private Schedulable {
	public:
		Connection();
		~Connection();
		virtual void output(const std::string& message) = 0;
		virtual void close() = 0;

	protected:
		xmlParserCtxt* parser_context;

	private:
		void execute(const std::string& command);
		virtual void executeUntil(const EmuTime& time, int userData);
		virtual const std::string& schedName() const;
		
		enum State {
			START, TAG_OPENMSX, TAG_COMMAND, END
		};
		struct ParseState {
			State state;
			unsigned unknownLevel;
			std::string content;
			Connection* object;
		};

		static void cb_start_element(ParseState* user_data, const xmlChar* name,
		                             const xmlChar** attrs);
		static void cb_end_element(ParseState* user_data, const xmlChar* name);
		static void cb_text(ParseState* user_data, const xmlChar* chars, int len);
	
		xmlSAXHandler sax_handler;
		ParseState user_data;
		Semaphore lock;
		std::deque<std::string> cmds;
	};

	class StdioConnection : public Connection, private Runnable {
	public:
		StdioConnection();
		~StdioConnection();
		virtual void output(const std::string& message);
		virtual void close();
	private:
		virtual void run();
		Thread thread;
		bool ok;
	};

#ifdef _WIN32
	class PipeConnection : public Connection, private Runnable {
	public:
		PipeConnection(const std::string& name);
		~PipeConnection();
		virtual void output(const std::string& message);
		virtual void close();
	private:
		virtual void run();
		Thread thread;
		HANDLE pipeHandle;
	};
#endif

	class SocketConnection : public Connection, private Runnable {
	public:
		SocketConnection(SOCKET sd);
		~SocketConnection();
		virtual void output(const std::string& message);
		virtual void close();
	private:
		virtual void run();
		Thread thread;
		int sd;
	};

	class ServerSocket : private Runnable {
	public:
		ServerSocket();
		~ServerSocket();
		void start();
	private:
		virtual void run();
		void mainLoop();
		Thread thread;
	};
	
	class UpdateCmd : public SimpleCommand {
	public:
		UpdateCmd(CliComm& parent);
		virtual std::string execute(const std::vector<std::string>& tokens);
		virtual std::string help(const std::vector<std::string>& tokens) const;
		virtual void tabCompletion(std::vector<std::string>& tokens) const;
	private:
		CliComm& parent;
	} updateCmd;

	bool updateEnabled[NUM_UPDATES];
	std::map<std::string, std::string> prevValues[NUM_UPDATES];
	CommandController& commandController;

	bool xmlOutput;
	typedef std::vector<Connection*> Connections;
	Connections connections;
	ServerSocket serverSocket;
};

} // namespace openmsx

#endif
