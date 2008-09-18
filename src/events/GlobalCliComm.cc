// $Id$

#include "GlobalCliComm.hh"
#include "CommandException.hh"
#include "XMLElement.hh"
#include "EventDistributor.hh"
#include "CliConnection.hh"
#include "Socket.hh"
#include "GlobalCommandController.hh"
#include "Command.hh"
#include "StringOp.hh"
#include <map>
#include <iostream>
#include <cassert>

using std::cout;
using std::cerr;
using std::endl;
using std::map;
using std::set;
using std::string;
using std::vector;
using std::auto_ptr;

namespace openmsx {

class UpdateCmd : public SimpleCommand
{
public:
	UpdateCmd(CommandController& commandController,
	          GlobalCliComm& cliComm);
	virtual string execute(const vector<string>& tokens);
	virtual string help(const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	CliConnection& getConnection();
	GlobalCliComm& cliComm;
};


GlobalCliComm::GlobalCliComm(GlobalCommandController& commandController_,
                 EventDistributor& eventDistributor_)
	: updateCmd(new UpdateCmd(commandController_, *this))
	, commandController(commandController_)
	, eventDistributor(eventDistributor_)
	, sem(1)
	, xmlOutput(false)
{
	commandController.setCliComm(this);
}

GlobalCliComm::~GlobalCliComm()
{
	ScopedLock lock(sem);
	for (Connections::const_iterator it = connections.begin();
	     it != connections.end(); ++it) {
		delete *it;
	}

	commandController.setCliComm(0);
}

void GlobalCliComm::startInput(const string& option)
{
	string type_name, arguments;
	StringOp::splitOnFirst(option, ":", type_name, arguments);

	auto_ptr<CliConnection> connection;
	if (type_name == "stdio") {
		connection.reset(new StdioConnection(
			commandController, eventDistributor));
	}
#ifdef _WIN32
	else if (type_name == "pipe") {
		OSVERSIONINFO info;
		info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
		GetVersionExA(&info);
		if (info.dwPlatformId == VER_PLATFORM_WIN32_NT) {
			connection.reset(new PipeConnection(
				commandController, eventDistributor, arguments));
		} else {
			throw FatalError("Pipes are not supported on this "
			                 "version of Windows");
		}
	}
#endif
	else {
		throw FatalError("Unknown control type: '"  + type_name + "'");
	}

	xmlOutput = true;
	addConnection(connection);
}

void GlobalCliComm::addConnection(auto_ptr<CliConnection> connection)
{
	ScopedLock lock(sem);
	connections.push_back(connection.release());
}

const char* const updateStr[CliComm::NUM_UPDATES] = {
	"led", "setting", "setting-info", "hardware", "plug", "unplug",
	"media", "status", "extension", "sounddevice", "connector"
};
void GlobalCliComm::log(LogLevel level, const string& message)
{
	const char* const levelStr[2] = {
		"info", "warning"
	};

	ScopedLock lock(sem);
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
		((level == INFO) ? cout : cerr) << levelStr[level] << ": " << message << endl;
	}
}

void GlobalCliComm::update(UpdateType type, const string& name,
                           const string& value)
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
	updateHelper(type, "", name, value);
}

void GlobalCliComm::updateHelper(UpdateType type, const string& machine,
                                 const string& name, const string& value)
{
	ScopedLock lock(sem);
	if (!connections.empty()) {
		string str = string("<update type=\"") + updateStr[type] + '\"';
		if (!machine.empty()) {
			str += " machine=\"" + machine + '\"';
		}
		if (!name.empty()) {
			str += " name=\"" + XMLElement::XMLEscape(name) + '\"';
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


// class UpdateCmd

UpdateCmd::UpdateCmd(CommandController& commandController,
                     GlobalCliComm& cliComm_)
	: SimpleCommand(commandController, "update")
	, cliComm(cliComm_)
{
}

static GlobalCliComm::UpdateType getType(const string& name)
{
	for (unsigned i = 0; i < CliComm::NUM_UPDATES; ++i) {
		if (updateStr[i] == name) {
			return static_cast<CliComm::UpdateType>(i);
		}
	}
	throw CommandException("No such update type: " + name);
}

CliConnection& UpdateCmd::getConnection()
{
	CliConnection* connection = getCommandController().getConnection();
	if (!connection) {
		throw CommandException("This command only makes sense when "
		                    "it's used from an external application.");
	}
	return *connection;
}

string UpdateCmd::execute(const vector<string>& tokens)
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

string UpdateCmd::help(const vector<string>& /*tokens*/) const
{
	static const string helpText = "Enable or disable update events for external applications. See doc/openmsx-control-xml.txt.";
	return helpText;
}

void UpdateCmd::tabCompletion(vector<string>& tokens) const
{
	switch (tokens.size()) {
	case 2: {
		set<string> ops;
		ops.insert("enable");
		ops.insert("disable");
		completeString(tokens, ops);
		break;
	}
	case 3: {
		set<string> types(updateStr, updateStr + CliComm::NUM_UPDATES);
		completeString(tokens, types);
	}
	}
}

} // namespace openmsx
