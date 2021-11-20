#ifndef ZSTRING_VIEW_HH
#define ZSTRING_VIEW_HH

#include <cassert>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>
#include <type_traits>

/**
 * Like std::string_view, but with the extra guarantee that it refers to a
 * zero-terminated string. In other words it has this additional invariant:
 *    data()[size()] == '\0'
 *
 * Current version only implements a subset of the std::string_view API. That
 * is because:
 * - Some functions cannot be implemented (those that trim the tail).
 * - Most were not yet required. We can implement those on demand.
 */
class zstring_view
{
public:
	using size_type = size_t;
	using const_iterator = const char*;

	static constexpr auto npos = std::string_view::npos;

	/*constexpr*/ zstring_view(const char* s)
		: dat(s), siz(size_type(strlen(s))) {}
	constexpr zstring_view(const char* s, size_t n)
		: dat(s), siz(n) { assert(s[n] == '\0'); }
	/*constexpr*/ zstring_view(const std::string& s)
		: dat(s.c_str()), siz(s.size()) {}

	[[nodiscard]] constexpr auto begin() const { return dat; }
	[[nodiscard]] constexpr auto end()   const { return dat + siz; }

	[[nodiscard]] constexpr auto size()  const { return siz; }
	[[nodiscard]] constexpr auto empty() const { return siz == 0; }

	[[nodiscard]] constexpr char operator[](size_type i) const {
		assert(i < siz);
		return dat[i];
	}
	[[nodiscard]] constexpr char front() const { return *dat; }
	[[nodiscard]] constexpr char back() const { return *(dat + siz - 1); }
	[[nodiscard]] constexpr const char* data()  const { return dat; }
	[[nodiscard]] constexpr const char* c_str() const { return dat; }

	[[nodiscard]] constexpr auto find(char c, size_type pos = 0) const {
		return view().find(c, pos);
	}

	[[nodiscard]] constexpr zstring_view substr(size_type pos) const {
		assert(pos <= siz);
		return zstring_view(dat + pos, siz - pos);
	}

	[[nodiscard]] explicit operator std::string() const {
		return {dat, siz};
	}
	[[nodiscard]] constexpr std::string_view view() const {
		return {dat, siz};
	}
	[[nodiscard]] constexpr /*implicit*/ operator std::string_view() const {
		return view();
	}

private:
	const char* dat;
	size_type siz;
};

static_assert(std::is_trivially_destructible_v<zstring_view>);
static_assert(std::is_trivially_copyable_v<zstring_view>);
static_assert(std::is_trivially_copy_constructible_v<zstring_view>);
static_assert(std::is_trivially_move_constructible_v<zstring_view>);
static_assert(std::is_trivially_assignable_v<zstring_view, zstring_view>);
static_assert(std::is_trivially_copy_assignable_v<zstring_view>);
static_assert(std::is_trivially_move_assignable_v<zstring_view>);

[[nodiscard]] constexpr auto begin(const zstring_view& x) { return x.begin(); }
[[nodiscard]] constexpr auto end  (const zstring_view& x) { return x.end();   }

[[nodiscard]] constexpr bool operator==(const zstring_view& x, const zstring_view& y) {
	return std::string_view(x) == std::string_view(y);
}
[[nodiscard]] inline bool operator==(const zstring_view& x, const std::string& y) {
	return std::string_view(x) == std::string_view(y);
}
[[nodiscard]] inline bool operator==(const std::string& x, const zstring_view& y) {
	return std::string_view(x) == std::string_view(y);
}
[[nodiscard]] constexpr bool operator==(const zstring_view& x, const std::string_view& y) {
	return std::string_view(x) == y;
}
[[nodiscard]] constexpr bool operator==(const std::string_view& x, const zstring_view& y) {
	return x == std::string_view(y);
}
[[nodiscard]] constexpr bool operator==(const zstring_view& x, const char* y) {
	return std::string_view(x) == std::string_view(y);
}
[[nodiscard]] constexpr bool operator==(const char* x, const zstring_view& y) {
	return std::string_view(x) == std::string_view(y);
}
[[nodiscard]] constexpr bool operator!=(const zstring_view& x, const zstring_view& y) {
	return std::string_view(x) != std::string_view(y);
}
[[nodiscard]] inline bool operator!=(const zstring_view& x, const std::string& y) {
	return std::string_view(x) != std::string_view(y);
}
[[nodiscard]] inline bool operator!=(const std::string& x, const zstring_view& y) {
	return std::string_view(x) != std::string_view(y);
}
[[nodiscard]] constexpr bool operator!=(const zstring_view& x, const std::string_view& y) {
	return std::string_view(x) != y;
}
[[nodiscard]] constexpr bool operator!=(const std::string_view& x, const zstring_view& y) {
	return x != std::string_view(y);
}
[[nodiscard]] constexpr bool operator!=(const zstring_view& x, const char* y) {
	return std::string_view(x) != std::string_view(y);
}
[[nodiscard]] constexpr bool operator!=(const char* x, const zstring_view& y) {
	return std::string_view(x) != std::string_view(y);
}

inline std::ostream& operator<<(std::ostream& os, const zstring_view& str)
{
	os << std::string_view(str);
	return os;
}
#endif
