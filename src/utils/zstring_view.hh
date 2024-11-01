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

	constexpr zstring_view()
		: dat(nullptr), siz(0) {}
	constexpr zstring_view(const char* s)
		: dat(s), siz(std::char_traits<char>::length(s)) {}
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
		return {dat + pos, siz - pos};
	}
	[[nodiscard]] constexpr std::string_view substr(size_type pos, size_type count) const {
		assert(pos <= siz);
		return view().substr(pos, count);
	}

	[[nodiscard]] constexpr bool starts_with(std::string_view sv) const {
		return view().starts_with(sv);
	}
	[[nodiscard]] constexpr bool starts_with(char c) const {
		return view().starts_with(c);
	}
	[[nodiscard]] constexpr bool starts_with(const char* s) const {
		return view().starts_with(s);
	}
	[[nodiscard]] constexpr bool ends_with(std::string_view sv) const {
		return view().ends_with(sv);
	}
	[[nodiscard]] constexpr bool ends_with(char c) const {
		return view().ends_with(c);
	}
	[[nodiscard]] constexpr bool ends_with(const char* s) const {
		return view().ends_with(s);
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

	[[nodiscard]] constexpr friend bool operator==(const zstring_view& x, const zstring_view& y) {
		return std::string_view(x) == std::string_view(y);
	}
	[[nodiscard]] inline friend bool operator==(const zstring_view& x, const std::string& y) {
		return std::string_view(x) == std::string_view(y);
	}
	[[nodiscard]] constexpr friend bool operator==(const zstring_view& x, const std::string_view& y) {
		return std::string_view(x) == y;
	}
	[[nodiscard]] constexpr friend bool operator==(const zstring_view& x, const char* y) {
		return std::string_view(x) == std::string_view(y);
	}

	friend std::ostream& operator<<(std::ostream& os, const zstring_view& str)
	{
		os << std::string_view(str);
		return os;
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

// !!! Workaround clang-15, libc++ bug !!! (fixed in clang-16)
// These should be 4x operator<=> instead of 4x operator<, <=, >=, >
[[nodiscard]] constexpr auto operator<(const zstring_view& x, const zstring_view& y) {
	return std::string_view(x) < std::string_view(y);
}
[[nodiscard]] inline auto operator<(const zstring_view& x, const std::string& y) {
	return std::string_view(x) < std::string_view(y);
}
[[nodiscard]] constexpr auto operator<(const zstring_view& x, const std::string_view& y) {
	return std::string_view(x) < y;
}
[[nodiscard]] constexpr auto operator<(const zstring_view& x, const char* y) {
	return std::string_view(x) < std::string_view(y);
}
[[nodiscard]] constexpr auto operator<=(const zstring_view& x, const zstring_view& y) {
	return std::string_view(x) <= std::string_view(y);
}
[[nodiscard]] inline auto operator<=(const zstring_view& x, const std::string& y) {
	return std::string_view(x) <= std::string_view(y);
}
[[nodiscard]] constexpr auto operator<=(const zstring_view& x, const std::string_view& y) {
	return std::string_view(x) <= y;
}
[[nodiscard]] constexpr auto operator<=(const zstring_view& x, const char* y) {
	return std::string_view(x) <= std::string_view(y);
}
[[nodiscard]] constexpr auto operator>(const zstring_view& x, const zstring_view& y) {
	return std::string_view(x) > std::string_view(y);
}
[[nodiscard]] inline auto operator>(const zstring_view& x, const std::string& y) {
	return std::string_view(x) > std::string_view(y);
}
[[nodiscard]] constexpr auto operator>(const zstring_view& x, const std::string_view& y) {
	return std::string_view(x) > y;
}
[[nodiscard]] constexpr auto operator>(const zstring_view& x, const char* y) {
	return std::string_view(x) > std::string_view(y);
}
[[nodiscard]] constexpr auto operator>=(const zstring_view& x, const zstring_view& y) {
	return std::string_view(x) >= std::string_view(y);
}
[[nodiscard]] inline auto operator>=(const zstring_view& x, const std::string& y) {
	return std::string_view(x) >= std::string_view(y);
}
[[nodiscard]] constexpr auto operator>=(const zstring_view& x, const std::string_view& y) {
	return std::string_view(x) >= y;
}
[[nodiscard]] constexpr auto operator>=(const zstring_view& x, const char* y) {
	return std::string_view(x) >= std::string_view(y);
}

#endif
