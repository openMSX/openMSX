// $Id$

#ifndef CLICOMM_HH
#define CLICOMM_HH

#include <string>

namespace openmsx {

class CliComm
{
public:
	enum LogLevel {
		INFO,
		WARNING
	};
	enum UpdateType {
		LED,
		SETTING,
		SETTINGINFO,
		HARDWARE,
		PLUG,
		UNPLUG,
		MEDIA,
		STATUS,
		EXTENSION,
		SOUNDDEVICE,
		CONNECTOR,
		NUM_UPDATES // must be last
	};

	virtual void log(LogLevel level, const std::string& message) = 0;
	virtual void update(UpdateType type, const std::string& name,
	                    const std::string& value) = 0;

	// convenience methods (shortcuts for log())
	void printInfo(const std::string& message);
	void printWarning(const std::string& message);

protected:
	CliComm();
	virtual ~CliComm();
};

} // namespace openmsx

#endif
