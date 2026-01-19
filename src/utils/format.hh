#ifndef FORMAT_HH
#define FORMAT_HH

#include "zstring_view.hh"

#include <cassert>
#include <format>
#include <span>
#include <string_view>
#include <utility>

// Utility functions related to std::format

template<typename... Args>
std::string_view format_to_buf(
	std::span<char> buf, std::format_string<Args...> fmt, Args&&... args)
{
	auto r = std::format_to_n(buf.begin(), buf.size(), fmt, std::forward<Args>(args)...);
	assert(size_t(r.size) <= buf.size()); // must fit
	return {buf.data(), size_t(r.size)};
}

template<typename... Args>
zstring_view format_to_zbuf(
	std::span<char> buf, std::format_string<Args...> fmt, Args&&... args)
{
	assert(!buf.empty());
	// Reserve 1 byte for the terminating '\0'
	auto r = std::format_to_n(buf.begin(), buf.size() - 1, fmt, std::forward<Args>(args)...);
	assert(size_t(r.size) < buf.size()); // must fit with spare location for '\0'
	buf[r.size] = '\0';
	return {buf.data(), size_t(r.size)};
}

#endif
