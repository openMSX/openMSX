// $Id$

#ifndef STRING_REF_HH
#define STRING_REF_HH

#include <string>
#include <iosfwd>
#include <cassert>
#include "string.h"

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
	typedef const char* const_iterator;

	static const unsigned npos = unsigned(-1);

	// construct/copy/assign
	string_ref()
		: dat(NULL), siz(0) {}
	string_ref(const string_ref& str)
		: dat(str.dat), siz(str.siz) {}
	string_ref(const char* str)
		: dat(str), siz(str ? strlen(str) : 0) {}
	string_ref(const char* str, unsigned len)
		: dat(str), siz(len) { if (dat == NULL) assert(siz == 0); }
	string_ref(const char* begin, const char* end)
		: dat(begin), siz(end - begin) { if (dat == NULL) assert(siz == 0); }
	string_ref(const std::string& str)
		: dat(str.data()), siz(str.size()) {}

	string_ref& operator=(const string_ref& rhs) {
		dat = rhs.data();
		siz = rhs.size();
		return *this;
	}

	// iterators
	const_iterator begin() const { return dat; }
	const_iterator end()   const { return dat + siz; }
	//const_iterator rbegin() const;
	//const_iterator rend() const;

	// capacity
	unsigned size()  const { return siz; }
	bool     empty() const { return siz == 0; }
	//unsigned max_size() const;
	//unsigned length() const;

	// element access
	char operator[](unsigned i) const {
		assert(i < siz);
		return dat[i];
	}
	//const char& at(unsigned i) const;
	char front() const { return *dat; }
	char back() const { return *(dat + siz - 1); }
	const char* data() const { return dat; }

	// Outgoing conversion operators
	//explicit operator std::string() const; // c++11
	std::string str() const;

	// mutators
	void clear() { siz = 0; } // no need to change 'dat'
	void remove_prefix(unsigned n);
	void remove_suffix(unsigned n);
	void pop_back()  { remove_suffix(1); }
	void pop_front() { remove_prefix(1); }

	// string operations with the same semantics as std::string
	int compare(string_ref x) const;
	string_ref substr(unsigned pos, unsigned n = npos) const;
	//unsigned copy(char* buf) const;
	unsigned find(string_ref s) const;
	unsigned find(char c) const;
	//unsigned rfind(string_ref s) const;
	//unsigned rfind(char c) const;
	//unsigned find_first_of(string_ref s) const;
	//unsigned find_first_of(char c) const;
	//unsigned find_first_not_of(string_ref s) const;
	//unsigned find_first_not_of(char c) const;
	//unsigned find_last_of(string_ref s) const;
	//unsigned find_last_of(char c) const;
	//unsigned find_last_not_of(string_ref s) const;
	//unsigned find_last_not_of(char c) const;

	// new string operations (not part of std::string)
	bool starts_with(string_ref x) const;
	bool ends_with(string_ref x) const;

private:
	const char* dat;
	unsigned siz;
};


// Comparison operators
bool operator==(string_ref x, string_ref y);
bool operator< (string_ref x, string_ref y);
inline bool operator!=(string_ref x, string_ref y) { return !(x == y); }
inline bool operator> (string_ref x, string_ref y) { return  (y <  x); }
inline bool operator<=(string_ref x, string_ref y) { return !(y <  x); }
inline bool operator>=(string_ref x, string_ref y) { return !(x <  y); }

// numeric conversions
int                stoi  (string_ref str, unsigned* idx = NULL, int base = 0);
//long               stol  (string_ref str, unsigned* idx = NULL, int base = 0);
//unsigned long      stoul (string_ref str, unsigned* idx = NULL, int base = 0);
long long          stoll (string_ref str, unsigned* idx = NULL, int base = 0);
//unsigned long long stoull(string_ref str, unsigned* idx = NULL, int base = 0);
//float              stof  (string_ref str, unsigned* idx = NULL);
//double             stod  (string_ref str, unsigned* idx = NULL);
//long double        stold (string_ref str, unsigned* idx = NULL);

// concatenation (this is not part of the std::string_ref proposal)
std::string operator+(string_ref x, string_ref y);
std::string operator+(char x, string_ref y);
std::string operator+(string_ref x, char y);

std::ostream& operator<<(std::ostream& os, string_ref str);

#endif
