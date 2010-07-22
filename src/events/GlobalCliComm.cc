// $Id$

#include "GlobalCliComm.hh"
#include "XMLElement.hh"
#include "CliConnection.hh"
#include "StringOp.hh"
#include <iostream>
#include <cassert>

using std::cout;
using std::cerr;
using std::endl;
using std::map;
using std::string;

namespace openmsx {

GlobalCliComm::GlobalCliComm()
	: sem(1)
	, xmlOutput(false)
{
}

GlobalCliComm::~GlobalCliComm()
{
	ScopedLock lock(sem);
	for (Connections::const_iterator it = connections.begin();
	     it != connections.end(); ++it) {
		delete *it;
	}
}

void GlobalCliComm::setXMLOutput()
{
	xmlOutput = true;
}

void GlobalCliComm::addConnection(std::auto_ptr<CliConnection> connection)
{
	ScopedLock lock(sem);
	connections.push_back(connection.release());
}

const char* const updateStr[CliComm::NUM_UPDATES] = {
	"led", "setting", "setting-info", "hardware", "plug", "unplug",
	"media", "status", "extension", "sounddevice", "connector"
};
const char* const* GlobalCliComm::getUpdateStrs()
{
	return updateStr;
}

void GlobalCliComm::log(LogLevel level, const string& message)
{
	const char* const levelStr[2] = {
		"info", "warning"
	};

	ScopedLock lock(sem);
	if (!connections.empty()) {
		string str = StringOp::Builder() <<
			"<log level=\"" << levelStr[level] << "\">" <<
			XMLElement::XMLEscape(message) << "</log>\n";
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
		StringOp::Builder tmp;
		tmp << "<update type=\"" << updateStr[type] << '\"';
		if (!machine.empty()) {
			tmp << " machine=\"" << machine << '\"';
		}
		if (!name.empty()) {
			tmp << " name=\"" << XMLElement::XMLEscape(name) << '\"';
		}
		tmp << '>' << XMLElement::XMLEscape(value) << "</update>\n";
		string str = tmp;
		for (Connections::const_iterator it = connections.begin();
		     it != connections.end(); ++it) {
			CliConnection& connection = **it;
			if (connection.getUpdateEnable(type)) {
				connection.output(str);
			}
		}
	}
}

} // namespace openmsx
