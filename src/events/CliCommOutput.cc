// $Id$

#include <iostream>
#include <cassert>
#include "xmlx.hh"
#include "CliCommOutput.hh"
#include "CommandController.hh"

using std::cout;
using std::endl;

namespace openmsx {

const char* const updateStr[CliCommOutput::NUM_UPDATES] = {
	"led", "break", "setting", "plug", "unplug", "media"
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
}

CliCommOutput::~CliCommOutput()
{
	if (xmlOutput) {
		cout << "</openmsx-output>" << endl;
	}
	commandController.unregisterCommand(&updateCmd, "update");
}

void CliCommOutput::log(LogLevel level, const string& message)
{
	const char* const levelStr[2] = {
		"info",
		"warning"
	};
	
	if (xmlOutput) {
		cout << "<log level=\"" << levelStr[level] << "\">"
		     << XMLEscape(message)
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
	     << XMLEscape(message)
	     << "</reply>" << endl;
}

void CliCommOutput::update(UpdateType type, const string& name, const string& value)
{
	assert(type < NUM_UPDATES);
	if (!updateEnabled[type]) {
		return;
	}
	if (xmlOutput) {
		cout << "<update type=\"" << updateStr[type] << '\"';
		if (!name.empty()) {
			cout << " name=\"" << name << '\"';
		}
		cout << '>' << XMLEscape(value) << "</update>" << endl;
	} else {
		cout << updateStr[type] << ": ";
		if (!name.empty()) {
			cout << name << " = ";
		}
		cout << value << endl;
	}
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

string CliCommOutput::UpdateCmd::help(const vector<string>& tokens) const
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
