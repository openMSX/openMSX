#ifndef CLICOMM_HH
#define CLICOMM_HH

#include "strCat.hh"
#include <array>
#include <span>
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
		SETTING_INFO,
		HARDWARE,
		PLUG,
		MEDIA,
		STATUS,
		EXTENSION,
		SOUND_DEVICE,
		CONNECTOR,
		DEBUG_UPDT,
		NUM_UPDATES // must be last
	};

	/** Log a message with a certain priority level.
	  * The 'fraction' parameter is only meaningful for 'level=PROGRESS'.
	  * See printProgress() for details.
	  */
	virtual void log(LogLevel level, std::string_view message, float fraction = 0.0f) = 0;
	virtual void update(UpdateType type, std::string_view name,
	                    std::string_view value) = 0;
	/** Same as update(), but checks that the value for type-name is the
	    same as in the previous call. If so do nothing. */
	virtual void updateFiltered(UpdateType type, std::string_view name,
	                            std::string_view value) = 0;

	// convenience methods (shortcuts for log())
	void printInfo    (std::string_view message);
	void printWarning (std::string_view message);
	void printError   (std::string_view message);
	// 'fraction' should be between 0.0 and 1.0, with these exceptions:
	//   a negative value (currently -1.0 is used) means an unknown progress fraction
	//   any value > 1.0 is clipped to 1.0
	// Progress messages should be send periodically (aim for a few per second).
	// The last message in such a sequence MUST always have 'fraction >= // 1.0',
	// because this signals the action is done and e.g. removes the progress bar.
	void printProgress(std::string_view message, float fraction);

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
	void printProgress(const char* message, float fraction) {
		printProgress(std::string_view(message), fraction);
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

	// string representations of the LogLevel and UpdateType enums
	[[nodiscard]] static std::span<const std::string_view, NUM_LEVELS> getLevelStrings() {
		static constexpr std::array<std::string_view, NUM_LEVELS> levelStr = {
			"info", "warning", "error", "progress"
		};
		return levelStr;
	}
	[[nodiscard]] static std::span<const std::string_view, NUM_UPDATES> getUpdateStrings() {
		static constexpr std::array<std::string_view, NUM_UPDATES> updateStr = {
			"led", "setting", "setting-info", "hardware", "plug",
			"media", "status", "extension", "sounddevice", "connector",
			"debug"
		};
		return updateStr;
	}

protected:
	CliComm() = default;
	~CliComm() = default;
};

} // namespace openmsx

#endif
