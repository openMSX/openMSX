// $Id$

#include <iostream>
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

void CliCommOutput::printInfo(const string& message)
{
	if (xmlOutput) {
		cout << "<info>" << message << "</info>" << endl;
	} else {
		cout << message << endl;
	}
}

void CliCommOutput::printWarning(const string& message)
{
	if (xmlOutput) {
		cout << "<warning>" << message << "</warning>" << endl;
	} else {
		cout << "Warning: " << message << endl;
	}
}

void CliCommOutput::printUpdate(const string& message)
{
	if (xmlOutput) {
		cout << "<update>" << message << "</update>" << endl;
	} else {
		cout << message << endl;
	}
}

} // namespace openmsx
