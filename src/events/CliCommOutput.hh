// $Id$

#ifndef __CLICOMMOUTPUT_HH__
#define __CLICOMMOUTPUT_HH__

#include <string>

using std::string;

namespace openmsx {

class CliCommOutput
{
public:
	static CliCommOutput& instance();
	void enableXMLOutput();
	
	void printInfo(const string& message);
	void printWarning(const string& message);
	void printUpdate(const string& message);

private:
	CliCommOutput();
	~CliCommOutput();

	bool xmlOutput;
};

} // namespace openmsx

#endif
