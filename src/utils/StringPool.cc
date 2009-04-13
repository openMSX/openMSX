// $Id$

#include "StringPool.hh"
#include <cassert>


// class StringPool

StringPool::StringPool()
{
}

StringPool::~StringPool()
{
}

StringPool& StringPool::instance()
{
	static StringPool oneInstance;
	return oneInstance;
}

StringPool::Ref StringPool::add(const std::string& str)
{
	Ref ref = pool.find(str);
	if (ref == pool.end()) {
		std::pair<Ref, bool> p = pool.insert(std::make_pair(str, 0));
		ref = p.first;
	}
	++(ref->second);
	return ref;
}

void StringPool::release(Ref ref)
{
	assert(ref->second);
	--(ref->second);
	if (ref->second == 0) {
		pool.erase(ref);
	}
}


// class StringRef

StringRef::StringRef()
	: ref(StringPool::instance().add(std::string()))
{
}

StringRef::StringRef(const std::string& str)
	: ref(StringPool::instance().add(str))
{
}

StringRef::StringRef(const StringRef& other)
	: ref(other.ref)
{
	++(ref->second);
}

StringRef::~StringRef()
{
	StringPool::instance().release(ref);
}

StringRef& StringRef::operator=(StringRef other)
{
	swap(other);
	return *this;
}

StringRef& StringRef::operator=(const std::string& str)
{
	StringRef other(str);
	swap(other);
	return *this;
}

StringRef::operator const std::string&()
{
	return ref->first;
}

const std::string& StringRef::str() const
{
	return ref->first;
}

void StringRef::swap(StringRef& other)
{
	std::swap(ref, other.ref);
}

bool StringRef::empty() const
{
	return ref->first.empty();
}

void StringRef::clear()
{
	StringRef empty;
	swap(empty);
}


bool operator==(const StringRef& ref1, const StringRef& ref2)
{
	return ref1.ref == ref2.ref;
}
bool operator==(const StringRef& ref1, const std::string& str2)
{
	return ref1.str() == str2;
}
bool operator==(const std::string& str1, const StringRef& ref2)
{
	return str1 == ref2.str();
}

bool operator!=(const StringRef& ref1, const StringRef& ref2)
{
	return !(ref1 == ref2);
}
bool operator!=(const StringRef& ref1, const std::string& str2)
{
	return ref1.str() != str2;
}
bool operator!=(const std::string& str1, const StringRef& ref2)
{
	return str1 != ref2.str();
}

std::string operator+(const StringRef& ref1, const StringRef& ref2)
{
	return ref1.str() + ref2.str();
}
std::string operator+(const StringRef& ref1, const std::string& str2)
{
	return ref1.str() + str2;
}
std::string operator+(const std::string& str1, const StringRef& ref2)
{
	return str1 + ref2.str();
}
std::string operator+(const StringRef& ref1, const char* str2)
{
	return ref1.str() + str2;
}
std::string operator+(const char* str1, const StringRef& ref2)
{
	return str1 + ref2.str();
}
std::string operator+(const StringRef& ref1, char char2)
{
	return ref1.str() + char2;
}
std::string operator+(char char1, const StringRef& ref2)
{
	return char1 + ref2.str();
}
