// $Id$

#ifndef __CLICOMMOUTPUT_HH__
#define __CLICOMMOUTPUT_HH__

#include <string>

using std::string;

namespace openmsx {

class CliCommOutput
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
	};
	
	static CliCommOutput& instance();
	void enableXMLOutput();
	
	void log(LogLevel level, const string& message);
	void reply(ReplyStatus status, const string& message);
	void update(UpdateType type, const string& name, const string& value);

	// convient methods
	void printInfo(const string& message) {
		log(INFO, message);
	}
	void printWarning(const string& message) {
		log(WARNING, message);
	}
	
private:
	CliCommOutput();
	~CliCommOutput();

	bool xmlOutput;
};

} // namespace openmsx

#endif
