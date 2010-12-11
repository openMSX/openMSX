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
		WARNING,
		LOGLEVEL_ERROR, // ERROR may give preprocessor name clashes
		PROGRESS,
		NUM_LEVELS // must be last
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
	void printError(const std::string& message);
	void printProgress(const std::string& message);

	// string representations of the LogLevel and UpdateType enums
	static const char* const* getLevelStrings()  { return levelStr;  }
	static const char* const* getUpdateStrings() { return updateStr; }

protected:
	CliComm();
	virtual ~CliComm();

private:
	static const char* const levelStr [NUM_LEVELS];
	static const char* const updateStr[NUM_UPDATES];
};

} // namespace openmsx

#endif
