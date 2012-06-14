// $Id$

#include "string_ref.hh"
#include "cstdlibp.hh"
#include <algorithm>
#include <iostream>

using std::string;

// Outgoing conversion operators

string string_ref::str() const
{
	return siz ? string(dat, siz)
	           : string();
}


// mutators

void string_ref::remove_prefix(size_type n)
{
	if (n <= siz) {
		dat += n;
		siz -= n;
	} else {
		clear();
	}
}

void string_ref::remove_suffix(size_type n)
{
	if (n <= siz) {
		siz -= n;
	} else {
		clear();
	}
}


// string operations with the same semantics as std::string

int string_ref::compare(string_ref rhs) const
{
	// Check prefix.
	if (int r = memcmp(dat, rhs.dat, std::min(siz, rhs.siz))) {
		return r;
	}
	// Prefixes match, check length.
	return siz - rhs.siz; // Note: this overflows for very large strings.
}


string_ref string_ref::substr(size_type pos, size_type n) const
{
	if (pos >= siz) return string_ref();
	return string_ref(dat + pos, std::min(n, siz - pos));
}

string_ref::size_type string_ref::find(string_ref s) const
{
	// Simple string search algorithm O(size() * s.size()). An algorithm
	// like Boyer–Moore has better time complexity and will run a lot
	// faster on large strings. Though when the strings are relatively
	// short (the typically case?) this very simple algorithm may run
	// faster (because it has no setup-time). The implementation of
	// std::string::find() in gcc uses a similar simple algorithm.
	if (s.empty()) return 0;
	if (s.size() <= siz) {
		size_type m = siz - s.size();
		for (size_type pos = 0; pos <= m; ++pos) {
			if ((dat[pos] == s[0]) &&
			    std::equal(s.begin() + 1, s.end(), dat + pos + 1)) {
				return pos;
			}
		}
	}
	return npos;
}

string_ref::size_type string_ref::find(char c) const
{
	const_iterator it = std::find(begin(), end(), c);
	return (it == end()) ? npos : it - begin();
}

string_ref::size_type string_ref::rfind(string_ref s) const
{
	// see comment in find()
	if (s.empty()) return siz;
	if (s.size() <= siz) {
		size_type m = siz - s.size();
		for (size_type pos = m; pos != size_type(-1); --pos) {
			if ((dat[pos] == s[0]) &&
			    std::equal(s.begin() + 1, s.end(), dat + pos + 1)) {
				return pos;
			}
		}
	}
	return npos;
}

string_ref::size_type string_ref::rfind(char c) const
{
	const_reverse_iterator it = std::find(rbegin(), rend(), c);
	return (it == rend()) ? npos : (it.base() - begin() - 1);
}

string_ref::size_type string_ref::find_first_of(string_ref s) const
{
	const_iterator it = std::find_first_of(begin(), end(), s.begin(), s.end());
	return (it == end()) ? npos : it - begin();
}

string_ref::size_type string_ref::find_first_of(char c) const
{
	return find(c);
}

//string_ref::size_type string_ref::find_first_not_of(string_ref s) const;
//string_ref::size_type string_ref::find_first_not_of(char c) const;

string_ref::size_type string_ref::find_last_of(string_ref s) const
{
	const_reverse_iterator it = std::find_first_of(
		rbegin(), rend(), s.begin(), s.end());
	return (it == rend()) ? npos : (it.base() - begin() - 1);
}

string_ref::size_type string_ref::find_last_of(char c) const
{
	return rfind(c);
}

//string_ref::size_type string_ref::find_last_not_of(string_ref s) const;
//string_ref::size_type string_ref::find_last_not_of(char c) const;

// new string operations (not part of std::string)
bool string_ref::starts_with(string_ref x) const
{
	return (siz >= x.size()) &&
	       (memcmp(dat, x.data(), x.size()) == 0);
}

bool string_ref::ends_with(string_ref x) const
{
	return (siz >= x.size()) &&
	       (memcmp(dat + siz - x.size(), x.data(), x.size()) == 0);
}


// Comparison operators
bool operator==(string_ref x, string_ref y)
{
	return (x.size() == y.size()) &&
	       (memcmp(x.data(), y.data(), x.size()) == 0);
}

bool operator< (string_ref x, string_ref y)
{
	return x.compare(y) < 0;
}


// numeric conversions
//  TODO could be implemented more efficient (don't make a copy)
int stoi(string_ref str, string_ref::size_type* idx, int base)
{
	string s = str.str();
	const char* begin = s.c_str();
	char* end;
	int result = strtol(begin, &end, base);
	if (idx) *idx = end - begin;
	return result;
}
unsigned long stoul (string_ref str, string_ref::size_type* idx, int base)
{
	string s = str.str();
	const char* begin = s.c_str();
	char* end;
	int result = strtoul(begin, &end, base);
	if (idx) *idx = end - begin;
	return result;
}
long long stoll(string_ref str, string_ref::size_type* idx, int base)
{
	string s = str.str();
	const char* begin = s.c_str();
	char* end;
	int result = strtoll(begin, &end, base);
	if (idx) *idx = end - begin;
	return result;
}


// concatenation
// TODO make s1 + s2 + s3 also efficient
string operator+(string_ref x, string_ref y)
{
	string result;
	result.reserve(x.size() + y.size());
	result.append(x.data(), x.size());
	result.append(y.data(), y.size());
	return result;
}
std::string operator+(char x, string_ref y)
{
	string result;
	result.reserve(1 + y.size());
	result.append(&x, 1);
	result.append(y.data(), y.size());
	return result;
}
std::string operator+(string_ref x, char y)
{
	string result;
	result.reserve(x.size() + 1);
	result.append(x.data(), x.size());
	result.append(&y, 1);
	return result;
}


std::ostream& operator<<(std::ostream& os, string_ref str)
{
	os.write(str.data(), str.size());
	return os;
}
