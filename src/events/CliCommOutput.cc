// $Id$

#include <iostream>
#include <cassert>
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
	if (xmlOutput) {
		switch (level) {
		case INFO:
			cout << "<info>" << message << "</info>" << endl;
			break;
		case WARNING:
			cout << "<warning>" << message << "</warning>" << endl;
			break;
		default:
			assert(false);
		}
	} else {
		switch (level) {
		case INFO:
			cout << message << endl;
			break;
		case WARNING:
			cout << "Warning: " << message << endl;
			break;
		default:
			assert(false);
		}
	}
}

void CliCommOutput::reply(ReplyStatus status, const string& message)
{
	assert(xmlOutput);
	switch (status) {
	case OK:
		cout << "<ok>" << message << "</ok>" << endl;
		break;
	case NOK:
		cout << "<nok>" << message << "</nok>" << endl;
		break;
	default:
		assert(false);
	}

}

void CliCommOutput::update(UpdateType type, const string& message)
{
	if (xmlOutput) {
		switch (type) {
		case LED:
			cout << "<update>" << message << "</update>" << endl;
			break;
		case BREAK:
			cout << "<update>Break at " << message << "</update>" << endl;
			break;
		default:
			assert(false);
		}
	} else {
		switch (type) {
		case LED:
			cout << message << endl;
			break;
		case BREAK:
			cout << "Break at " << message << endl;
			break;
		default:
			assert(false);
		}
	}
}

} // namespace openmsx
