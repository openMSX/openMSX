// $Id$

#ifndef STRING_REF_HH
#define STRING_REF_HH

#include <string>
#include <iterator>
#include <iosfwd>
#include <cassert>
#include <cstring>

/** This class implements a subset of the proposal for std::string_ref
  * (proposed for the next c++ standard (c++1y)).
  *    http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2012/n3334.html#classstd_1_1basic__string__ref_1ab23a4885309a116e8e67349fe0950290
  *
  * It has an interface that is close to std::string, but it does not own
  * the memory for the string. Basically it's just a wrapper around:
  *   const char*  +  length.
  */
class string_ref
{
public:
	typedef unsigned size_type; // only 32-bit on x86_64
	typedef int difference_type;
	typedef const char* const_iterator;
	typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

	static const size_type npos = size_type(-1);

	// construct/copy/assign
	string_ref()
		: dat(NULL), siz(0) {}
	string_ref(const string_ref& str)
		: dat(str.dat), siz(str.siz) {}
	string_ref(const char* str)
		: dat(str), siz(str ? size_type(strlen(str)) : 0) {}
	string_ref(const char* str, size_type len)
		: dat(str), siz(len) { if (dat == NULL) assert(siz == 0); }
	string_ref(const char* begin, const char* end)
		: dat(begin), siz(end - begin) { if (dat == NULL) assert(siz == 0); }
	string_ref(const std::string& str)
		: dat(str.data()), siz(size_type(str.size())) {}

	string_ref& operator=(const string_ref& rhs) {
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
	void remove_prefix(size_type n);
	void remove_suffix(size_type n);
	void pop_back()  { remove_suffix(1); }
	void pop_front() { remove_prefix(1); }

	// string operations with the same semantics as std::string
	int compare(string_ref x) const;
	string_ref substr(size_type pos, size_type n = npos) const;
	//size_type copy(char* buf) const;
	size_type find(string_ref s) const;
	size_type find(char c) const;
	size_type rfind(string_ref s) const;
	size_type rfind(char c) const;
	size_type find_first_of(string_ref s) const;
	size_type find_first_of(char c) const;
	//size_type find_first_not_of(string_ref s) const;
	//size_type find_first_not_of(char c) const;
	size_type find_last_of(string_ref s) const;
	size_type find_last_of(char c) const;
	//size_type find_last_not_of(string_ref s) const;
	//size_type find_last_not_of(char c) const;

	// new string operations (not part of std::string)
	bool starts_with(string_ref x) const;
	bool ends_with(string_ref x) const;

private:
	const char* dat;
	size_type siz;
};


// Comparison operators
bool operator==(string_ref x, string_ref y);
bool operator< (string_ref x, string_ref y);
inline bool operator!=(string_ref x, string_ref y) { return !(x == y); }
inline bool operator> (string_ref x, string_ref y) { return  (y <  x); }
inline bool operator<=(string_ref x, string_ref y) { return !(y <  x); }
inline bool operator>=(string_ref x, string_ref y) { return !(x <  y); }

// numeric conversions
int                stoi  (string_ref str, string_ref::size_type* idx = NULL, int base = 0);
//long               stol  (string_ref str, string_ref::size_type* idx = NULL, int base = 0);
unsigned long      stoul (string_ref str, string_ref::size_type* idx = NULL, int base = 0);
long long          stoll (string_ref str, string_ref::size_type* idx = NULL, int base = 0);
//unsigned long long stoull(string_ref str, string_ref::size_type* idx = NULL, int base = 0);
//float              stof  (string_ref str, string_ref::size_type* idx = NULL);
//double             stod  (string_ref str, string_ref::size_type* idx = NULL);
//long double        stold (string_ref str, string_ref::size_type* idx = NULL);

// concatenation (this is not part of the std::string_ref proposal)
std::string operator+(string_ref x, string_ref y);
std::string operator+(char x, string_ref y);
std::string operator+(string_ref x, char y);

std::ostream& operator<<(std::ostream& os, string_ref str);

#endif
