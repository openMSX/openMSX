// $Id$

#include "CliComm.hh"
#include "CommandException.hh"
#include "XMLElement.hh"
#include "EventDistributor.hh"
#include "LedEvent.hh"
#include "CliConnection.hh"
#include "Socket.hh"
#include "CommandController.hh"
#include "StringOp.hh"
#include <map>
#include <iostream>
#include <cassert>
#include <iostream>

using std::cout;
using std::endl;
using std::map;
using std::set;
using std::string;
using std::vector;

namespace openmsx {

CliComm::CliComm(Scheduler& scheduler_, CommandController& commandController_,
                 EventDistributor& eventDistributor_)
	: updateCmd(commandController_, *this)
	, scheduler(scheduler_)
	, commandController(commandController_)
	, eventDistributor(eventDistributor_)
	, xmlOutput(false)
{
	commandController.setCliComm(this);
	
	eventDistributor.registerEventListener(
		OPENMSX_LED_EVENT, *this, EventDistributor::DETACHED);
}

CliComm::~CliComm()
{
	for (Connections::const_iterator it = connections.begin();
	     it != connections.end(); ++it) {
		delete *it;
	}

	eventDistributor.unregisterEventListener(
		OPENMSX_LED_EVENT, *this, EventDistributor::DETACHED);
	
	commandController.setCliComm(0);
}

void CliComm::startInput(CommandLineParser::ControlType type, const string& arguments)
{
	xmlOutput = true;
	CliConnection* connection;
	switch (type) {
	case CommandLineParser::IO_PIPE: {
#ifdef _WIN32
		OSVERSIONINFO info;
		info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
		GetVersionExA(&info);
		if (info.dwPlatformId == VER_PLATFORM_WIN32_NT) {
			connection = new PipeConnection(
				scheduler, commandController, arguments);
		} else {
			throw FatalError(
				"Pipes are not supported on this version of Windows");
		}
#else
		assert(false);
		if (&arguments); // avoid warning
#endif
		break;
	}
	case CommandLineParser::IO_STD:
		connection = new StdioConnection(scheduler, commandController);
		break;
	default:
		assert(false);
	}
	connections.push_back(connection);
}

const char* const updateStr[CliComm::NUM_UPDATES] = {
	"led", "break", "resume", "setting", "plug", "unplug", "media", "status"
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
			CliConnection& connection = **it;
			if (connection.getUpdateEnable(type)) {
				connection.output(str);
			}
		}
	}
}

void CliComm::signalEvent(const Event& event)
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

	assert(event.getType() == OPENMSX_LED_EVENT);
	const LedEvent& ledEvent = static_cast<const LedEvent&>(event);
	update(LED, ledName[ledEvent.getLed()],
	       ledEvent.getStatus() ? ON : OFF);
}


// class UpdateCmd

CliComm::UpdateCmd::UpdateCmd(CommandController& commandController,
                              CliComm& cliComm_)
	: SimpleCommand(commandController, "update")
	, cliComm(cliComm_)
{
}

static CliComm::UpdateType getType(const string& name)
{
	for (unsigned i = 0; i < CliComm::NUM_UPDATES; ++i) {
		if (updateStr[i] == name) {
			return static_cast<CliComm::UpdateType>(i);
		}
	}
	throw CommandException("No such update type: " + name);
}

CliConnection& CliComm::UpdateCmd::getConnection()
{
	CliConnection* connection = getCommandController().getConnection();
	if (!connection) {
		throw CommandException("This command only makes sense when "
		                    "it's used from an external connection.");
	}
	return *connection;
}

string CliComm::UpdateCmd::execute(const vector<string>& tokens)
{
	if (tokens.size() != 3) {
		throw SyntaxError();
	}
	if (tokens[1] == "enable") {
		getConnection().setUpdateEnable(getType(tokens[2]), true);
	} else if (tokens[1] == "disable") {
		getConnection().setUpdateEnable(getType(tokens[2]), false);
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
			completeString(tokens, ops);
		}
		case 3: {
			set<string> types(updateStr, updateStr + NUM_UPDATES);
			completeString(tokens, types);
		}
	}
}

} // namespace openmsx
