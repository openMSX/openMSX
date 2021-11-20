#ifndef DATE_HH
#define DATE_HH

#include <limits>
#include <string>
#include <ctime>

namespace openmsx::Date {

	constexpr time_t INVALID_TIME_T = std::numeric_limits<time_t>::min();

	// Convert INVALID_TIME_T to a different (close) value.
	constexpr time_t adjustTimeT(time_t time) {
		return (time == Date::INVALID_TIME_T) ? (Date::INVALID_TIME_T + 1)
		                                      : time;
	}

	// 'p' must point to a buffer that is at least 24 characters long
	[[nodiscard]] time_t fromString(const char* p);

	[[nodiscard]] std::string toString(time_t time);

} // namespace openmsx::Date

#endif
