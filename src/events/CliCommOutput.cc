// $Id$

#include <iostream>
#include <cassert>
#include <map>
#include "xmlx.hh"
#include "CliCommOutput.hh"
#include "CommandController.hh"
#include "CommandException.hh"
#include "EventDistributor.hh"
#include "LedEvent.hh"

using std::cout;
using std::endl;
using std::map;
using std::set;
using std::string;
using std::vector;

namespace openmsx {

const char* const updateStr[CliCommOutput::NUM_UPDATES] = {
	"led", "break", "setting", "plug", "unplug", "media", "status"
};

CliCommOutput& CliCommOutput::instance()
{
	static CliCommOutput oneInstance;
	return oneInstance;
}

void CliCommOutput::enableXMLOutput()
{
	xmlOutput = true;
	cout << "<openmsx-output>" << endl;
}

CliCommOutput::CliCommOutput()
	: updateCmd(*this), xmlOutput(false),
	  commandController(CommandController::instance())
{
	for (int i = 0; i < NUM_UPDATES; ++i) {
		updateEnabled[i] = false;
	}
	commandController.registerCommand(&updateCmd, "update");
	EventDistributor::instance().registerEventListener(LED_EVENT, *this,
	                                           EventDistributor::NATIVE);
}

CliCommOutput::~CliCommOutput()
{
	EventDistributor::instance().unregisterEventListener(LED_EVENT, *this,
	                                             EventDistributor::NATIVE);
	commandController.unregisterCommand(&updateCmd, "update");
	if (xmlOutput) {
		cout << "</openmsx-output>" << endl;
	}
}

void CliCommOutput::log(LogLevel level, const string& message)
{
	const char* const levelStr[2] = {
		"info",
		"warning"
	};
	
	if (xmlOutput) {
		cout << "<log level=\"" << levelStr[level] << "\">"
		     << XMLElement::XMLEscape(message)
		     << "</log>" << endl;
	} else {
		cout << levelStr[level] << ": " << message << endl;
	}
}

void CliCommOutput::reply(ReplyStatus status, const string& message)
{
	const char* const replyStr[2] = {
		"ok",
		"nok"
	};

	assert(xmlOutput);
	cout << "<reply result=\"" << replyStr[status] << "\">"
	     << XMLElement::XMLEscape(message)
	     << "</reply>" << endl;
}

void CliCommOutput::update(UpdateType type, const string& name, const string& value)
{
	assert(type < NUM_UPDATES);
	if (!updateEnabled[type]) {
		return;
	}
	map<string, string>::iterator it = prevValues[type].find(name);
	if (it != prevValues[type].end()) {
		if (it->second == value) {
			return;
		}
		it->second = value;
	} else {
		prevValues[type][name] = value;
	}
	if (xmlOutput) {
		cout << "<update type=\"" << updateStr[type] << '\"';
		if (!name.empty()) {
			cout << " name=\"" << name << '\"';
		}
		cout << '>' << XMLElement::XMLEscape(value) << "</update>" << endl;
	} else {
		cout << updateStr[type] << ": ";
		if (!name.empty()) {
			cout << name << " = ";
		}
		cout << value << endl;
	}
}

bool CliCommOutput::signalEvent(const Event& event)
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
	
	assert(event.getType() == LED_EVENT);
	const LedEvent& ledEvent = static_cast<const LedEvent&>(event);
	update(LED, ledName[ledEvent.getLed()],
	       ledEvent.getStatus() ? ON : OFF);
	return true;
}

// class UpdateCmd

CliCommOutput::UpdateCmd::UpdateCmd(CliCommOutput& parent_)
	: parent(parent_)
{
}

static unsigned getType(const string& name)
{
	for (unsigned i = 0; i < CliCommOutput::NUM_UPDATES; ++i) {
		if (updateStr[i] == name) {
			return i;
		}
	}
	throw CommandException("No such update type: " + name);
}

string CliCommOutput::UpdateCmd::execute(const vector<string>& tokens)
{
	if (tokens.size() != 3) {
		throw SyntaxError();
	}
	if (tokens[1] == "enable") {
		parent.updateEnabled[getType(tokens[2])] = true;
	} else if (tokens[1] == "disable") {
		parent.updateEnabled[getType(tokens[2])] = false;
	} else {
		throw SyntaxError();
	}
	return "";
}

string CliCommOutput::UpdateCmd::help(const vector<string>& /*tokens*/) const
{
	static const string helpText = "TODO";
	return helpText;
}

void CliCommOutput::UpdateCmd::tabCompletion(vector<string>& tokens) const
{
	switch (tokens.size()) {
		case 2: {
			set<string> ops;
			ops.insert("enable");
			ops.insert("disable");
			CommandController::completeString(tokens, ops);
		}
		case 3: {
			set<string> types(updateStr, updateStr + NUM_UPDATES);
			CommandController::completeString(tokens, types);
		}
	}
}

} // namespace openmsx
