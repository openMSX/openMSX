#ifndef STRING_VIEW_HH
#define STRING_VIEW_HH

#include <string>
#include <iterator>
#include <iosfwd>
#include <cassert>
#include <cstring>

/** This class implements a (close approximation) of the std::string_view class.
  *    https://en.cppreference.com/w/cpp/string/basic_string_view
  * std::string_view is new in c++17. Once we can count on a c++17 compiler
  * to compile openMSX, we can drop this class.
  */
class string_view
{
public:
	using size_type = size_t;
	using difference_type = std::ptrdiff_t;
	using const_iterator = const char*;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	static const size_type npos = size_type(-1);

	// construct/copy/assign
	string_view()
		: dat(nullptr), siz(0) {}
	string_view(const string_view& s)
		: dat(s.dat), siz(s.siz) {}
	/*implicit*/ string_view(const char* s)
		: dat(s), siz(s ? size_type(strlen(s)) : 0) {}
	string_view(const char* s, size_type len)
		: dat(s), siz(len) { if (!dat) assert(siz == 0); }
	string_view(const char* first, const char* last)
		: dat(first), siz(last - first) { if (!dat) assert(siz == 0); }
	/*implicit*/ string_view(const std::string& s)
		: dat(s.data()), siz(s.size()) {}

	string_view& operator=(const string_view& rhs) {
		dat = rhs.data();
		siz = rhs.size();
		return *this;
	}

	// iterators
	const_iterator begin() const { return dat; }
	const_iterator end()   const { return dat + siz; }
	const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
	const_reverse_iterator rend()   const { return const_reverse_iterator(begin()); }

	// capacity
	size_type size()  const { return siz; }
	bool     empty() const { return siz == 0; }
	//size_type max_size() const;
	//size_type length() const;

	// element access
	char operator[](size_type i) const {
		assert(i < siz);
		return dat[i];
	}
	//const char& at(size_type i) const;
	char front() const { return *dat; }
	char back() const { return *(dat + siz - 1); }
	const char* data() const { return dat; }

	// Outgoing conversion operators
	//explicit operator std::string() const; // c++11
	std::string str() const;

	// mutators
	void clear() { siz = 0; } // no need to change 'dat'
	void remove_prefix(size_type n) {
		assert(n <= siz);
		dat += n;
		siz -= n;
	}
	void remove_suffix(size_type n) {
		assert(n <= siz);
		siz -= n;
	}
	void pop_back()  { remove_suffix(1); }
	void pop_front() { remove_prefix(1); }

	// string operations with the same semantics as std::string
	int compare(string_view x) const;
	string_view substr(size_type pos, size_type n = npos) const;
	//size_type copy(char* buf) const;
	size_type find(string_view s) const;
	size_type find(char c) const;
	size_type rfind(string_view s) const;
	size_type rfind(char c) const;
	size_type find_first_of(string_view s) const;
	size_type find_first_of(char c) const;
	//size_type find_first_not_of(string_view s) const;
	//size_type find_first_not_of(char c) const;
	size_type find_last_of(string_view s) const;
	size_type find_last_of(char c) const;
	//size_type find_last_not_of(string_view s) const;
	//size_type find_last_not_of(char c) const;

	// c++20
	bool starts_with(string_view x) const;
	bool starts_with(char x) const;
	bool ends_with(string_view x) const;
	bool ends_with(char x) const;

private:
	const char* dat;
	size_type siz;
};


// Comparison operators
inline bool operator==(string_view x, string_view y) {
	return (x.size() == y.size()) &&
	       (memcmp(x.data(), y.data(), x.size()) == 0);
}

bool operator< (string_view x, string_view y);
inline bool operator!=(string_view x, string_view y) { return !(x == y); }
inline bool operator> (string_view x, string_view y) { return  (y <  x); }
inline bool operator<=(string_view x, string_view y) { return !(y <  x); }
inline bool operator>=(string_view x, string_view y) { return !(x <  y); }

// numeric conversions
//int                stoi  (string_view s, string_view::size_type* idx = nullptr, int base = 0);
//long               stol  (string_view s, string_view::size_type* idx = nullptr, int base = 0);
//unsigned long      stoul (string_view s, string_view::size_type* idx = nullptr, int base = 0);
//long long          stoll (string_view s, string_view::size_type* idx = nullptr, int base = 0);
//unsigned long long stoull(string_view s, string_view::size_type* idx = nullptr, int base = 0);
//float              stof  (string_view s, string_view::size_type* idx = nullptr);
//double             stod  (string_view s, string_view::size_type* idx = nullptr);
//long double        stold (string_view s, string_view::size_type* idx = nullptr);


// Faster than the above, but less general (not part of the std proposal):
// - Only handles decimal.
// - No leading + or - sign (and thus only positive values).
// - No leading whitespace.
// - No trailing non-digit characters.
// - No out-of-range check (so undetected overflow on e.g. 9999999999).
// - Empty string parses as zero.
// Throws std::invalid_argument if any character is different from [0-9],
// similar to the error reporting in the std::stoi() (and related) functions.
unsigned fast_stou(string_view s);


std::ostream& operator<<(std::ostream& os, string_view s);

// begin, end
inline string_view::const_iterator begin(const string_view& x) { return x.begin(); }
inline string_view::const_iterator end  (const string_view& x) { return x.end();   }

#endif
