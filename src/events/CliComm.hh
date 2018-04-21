#ifndef CLICOMM_HH
#define CLICOMM_HH

#include "array_ref.hh"
#include "string_ref.hh"

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
		MEDIA,
		STATUS,
		EXTENSION,
		SOUNDDEVICE,
		CONNECTOR,
		NUM_UPDATES // must be last
	};

	virtual void log(LogLevel level, string_ref message) = 0;
	virtual void update(UpdateType type, string_ref name,
	                    string_ref value) = 0;

	// convenience methods (shortcuts for log())
	void printInfo    (string_ref message);
	void printWarning (string_ref message);
	void printError   (string_ref message);
	void printProgress(string_ref message);

	// string representations of the LogLevel and UpdateType enums
	static array_ref<const char*> getLevelStrings()  {
		return make_array_ref(levelStr);
	}
	static array_ref<const char*> getUpdateStrings() {
		return make_array_ref(updateStr);
	}

protected:
	CliComm() = default;
	~CliComm() = default;

private:
	static const char* const levelStr [NUM_LEVELS];
	static const char* const updateStr[NUM_UPDATES];
};

} // namespace openmsx

#endif
