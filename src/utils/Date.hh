#ifndef DATE_HH
#define DATE_HH

#include <ctime>
#include <limits>
#include <span>
#include <string>
#include <string_view>

namespace openmsx::Date {

	constexpr time_t INVALID_TIME_T = std::numeric_limits<time_t>::min();

	// Convert INVALID_TIME_T to a different (close) value.
	constexpr time_t adjustTimeT(time_t time) {
		return (time == Date::INVALID_TIME_T) ? (Date::INVALID_TIME_T + 1)
		                                      : time;
	}

	[[nodiscard]] time_t fromString(std::span<const char, 24> s);

	[[nodiscard]] std::string toString(time_t time);

	// Zero-allocation version: formats into buffer, always returns 24-char view.
	[[nodiscard]] std::string_view toString(time_t time, std::span<char, 24> buffer);

} // namespace openmsx::Date

#endif
