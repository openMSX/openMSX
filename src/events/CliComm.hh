#ifndef CLICOMM_HH
#define CLICOMM_HH

#include "array_ref.hh"
#include "strCat.hh"
#include "string_view.hh"

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

	virtual void log(LogLevel level, string_view message) = 0;
	virtual void update(UpdateType type, string_view name,
	                    string_view value) = 0;

	// convenience methods (shortcuts for log())
	void printInfo    (string_view message);
	void printWarning (string_view message);
	void printError   (string_view message);
	void printProgress(string_view message);

	// These overloads are (only) needed for efficiency, because otherwise
	// the templated overload below is a better match than the 'string_view'
	// overload above (and we don't want to construct a temp string).
	void printInfo(const char* message) {
		printInfo(string_view(message));
	}
	void printWarning(const char* message) {
		printWarning(string_view(message));
	}
	void printError(const char* message) {
		printError(string_view(message));
	}
	void printProgress(const char* message) {
		printProgress(string_view(message));
	}

	template<typename... Args>
	void printInfo(Args&& ...args) {
		printInfo(string_view(strCat(std::forward<Args>(args)...)));
	}
	template<typename... Args>
	void printWarning(Args&& ...args) {
		printWarning(string_view(strCat(std::forward<Args>(args)...)));
	}
	template<typename... Args>
	void printError(Args&& ...args) {
		printError(string_view(strCat(std::forward<Args>(args)...)));
	}
	template<typename... Args>
	void printProgress(Args&& ...args) {
		printProgress(string_view(strCat(std::forward<Args>(args)...)));
	}

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
