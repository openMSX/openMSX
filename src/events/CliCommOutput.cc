// $Id$

#include <iostream>
#include <cassert>
#include "xmlx.hh"
#include "CliCommOutput.hh"

using std::cout;
using std::endl;

namespace openmsx {

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
	: xmlOutput(false)
{
}

CliCommOutput::~CliCommOutput()
{
	if (xmlOutput) {
		cout << "</openmsx-output>" << endl;
	}
}

void CliCommOutput::log(LogLevel level, const string& message)
{
	const char* levelStr[2] = {
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
	const char* replyStr[2] = {
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
	const char* updateStr[2] = {
		"led", "break"
	};

	if (xmlOutput) {
		cout << "<update type=\"" << updateStr[type] << "\" "
		                "name=\"" << name << "\">"
		     << XMLEscape(value)
		     << "</update>" << endl;
	} else {
		cout << updateStr[type] << ": " << name << " = " << value << endl;
	}
}

} // namespace openmsx
