#ifndef CLICOMM_HH
#define CLICOMM_HH

#include "span.hh"
#include "strCat.hh"
#include <string_view>

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

	virtual void log(LogLevel level, std::string_view message) = 0;
	virtual void update(UpdateType type, std::string_view name,
	                    std::string_view value) = 0;

	// convenience methods (shortcuts for log())
	void printInfo    (std::string_view message);
	void printWarning (std::string_view message);
	void printError   (std::string_view message);
	void printProgress(std::string_view message);

	// These overloads are (only) needed for efficiency, because otherwise
	// the templated overload below is a better match than the 'string_view'
	// overload above (and we don't want to construct a temp string).
	void printInfo(const char* message) {
		printInfo(std::string_view(message));
	}
	void printWarning(const char* message) {
		printWarning(std::string_view(message));
	}
	void printError(const char* message) {
		printError(std::string_view(message));
	}
	void printProgress(const char* message) {
		printProgress(std::string_view(message));
	}

	template<typename... Args>
	void printInfo(Args&& ...args) {
		auto tmp = tmpStrCat(std::forward<Args>(args)...);
		printInfo(std::string_view(tmp));
	}
	template<typename... Args>
	void printWarning(Args&& ...args) {
		auto tmp = tmpStrCat(std::forward<Args>(args)...);
		printWarning(std::string_view(tmp));
	}
	template<typename... Args>
	void printError(Args&& ...args) {
		auto tmp = tmpStrCat(std::forward<Args>(args)...);
		printError(std::string_view(tmp));
	}
	template<typename... Args>
	void printProgress(Args&& ...args) {
		auto tmp = tmpStrCat(std::forward<Args>(args)...);
		printProgress(std::string_view(tmp));
	}

	// string representations of the LogLevel and UpdateType enums
	[[nodiscard]] static span<const char* const> getLevelStrings()  {
		static constexpr const char* const levelStr [NUM_LEVELS] = {
			"info", "warning", "error", "progress"
		};
		return levelStr;
	}
	[[nodiscard]] static span<const char* const> getUpdateStrings() {
		static constexpr const char* const updateStr[NUM_UPDATES] = {
			"led", "setting", "setting-info", "hardware", "plug",
			"media", "status", "extension", "sounddevice", "connector"
		};
		return updateStr;
	}

protected:
	CliComm() = default;
	~CliComm() = default;
};

} // namespace openmsx

#endif
