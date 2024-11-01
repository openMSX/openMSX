#ifndef TEMPORARYSTRING_HH
#define TEMPORARYSTRING_HH

#include "StringStorage.hh"
#include "zstring_view.hh"

#include <array>
#include <concepts>
#include <iostream>
#include <string_view>


/** TemporaryString
 *
 * This a type meant to store temporary strings. Typically it stores the string
 * in the object itself. Except when the string is too large, then it falls
 * back to heap-allocation. So it's more or less like a std::string but with a
 * relatively large (127 character) "small-string-buffer". (Typical std::string
 * implementations only have a buffer for 15 or 23 characters).
 *
 * Because of this internal buffer, TemporaryString objects are quite large.
 * The intention is that they are only used as local variable (so stored on the
 * stack). Not as member variables.
 *
 * The API of this type is still very minimal. It can be extended when needed.
 */
class TemporaryString {
public:
	static constexpr size_t BUFSIZE = 127;

	explicit TemporaryString()
		: n(0)
	{
		ptr = buffer.data();
		buffer[0] = '\0';
	}

	explicit TemporaryString(const char* s)
		: n(std::char_traits<char>::length(s)), ptr(s) {}
	explicit TemporaryString(zstring_view s)
		: n(s.size()), ptr(s.data()) {}

	explicit TemporaryString(size_t n_, std::invocable<char*> auto fillOp)
		: n(n_)
	{
		char* p = [&] {
			if (n <= BUFSIZE) {
				return buffer.data();
			} else {
				owner = allocate_string_storage(n + 1);
				return owner.get();
			}
		}();
		ptr = p;
		fillOp(p);
		p[n] = '\0';
	}
	TemporaryString(const TemporaryString&) = delete;
	TemporaryString(TemporaryString&&) = delete;
	TemporaryString& operator=(const TemporaryString&) = delete;
	TemporaryString& operator=(TemporaryString&&) = delete;
	~TemporaryString() = default;

	[[nodiscard]] auto size()  const { return n; }
	[[nodiscard]] auto empty() const { return n == 0; }

	[[nodiscard]] const char* data()  const { return ptr; }
	[[nodiscard]] const char* c_str() const { return ptr; }

	[[nodiscard]] std::string_view view()     const { return {ptr, n}; }
	[[nodiscard]] operator std::string_view() const { return {ptr, n}; }
	[[nodiscard]] operator     zstring_view() const { return {ptr, n}; }

	[[nodiscard]] bool starts_with(std::string_view s) const { return view().starts_with(s); }
	[[nodiscard]] bool starts_with(char c)             const { return view().starts_with(c); }
	[[nodiscard]] bool starts_with(const char* s)      const { return view().starts_with(s); }
	[[nodiscard]] bool ends_with(std::string_view s) const { return view().ends_with(s); }
	[[nodiscard]] bool ends_with(char c)             const { return view().ends_with(c); }
	[[nodiscard]] bool ends_with(const char* s)      const { return view().ends_with(s); }

	[[nodiscard]] friend bool operator==(const TemporaryString& x, const TemporaryString& y) {
		return std::string_view(x) == std::string_view(y);
	}
	[[nodiscard]] friend bool operator==(const TemporaryString& x, const zstring_view& y) {
		return std::string_view(x) == std::string_view(y);
	}
	[[nodiscard]] friend bool operator==(const TemporaryString& x, const std::string& y) {
		return std::string_view(x) == std::string_view(y);
	}
	[[nodiscard]] friend bool operator==(const TemporaryString& x, const std::string_view& y) {
		return std::string_view(x) == y;
	}
	[[nodiscard]] friend bool operator==(const TemporaryString& x, const char* y) {
		return std::string_view(x) == std::string_view(y);
	}

	friend std::ostream& operator<<(std::ostream& os, const TemporaryString& str)
	{
		os << std::string_view(str);
		return os;
	}

private:
	size_t n;
	const char* ptr;
	StringStorage owner;
	std::array<char, BUFSIZE + 1> buffer;
};

#endif
