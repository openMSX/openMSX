// $Id:$

#ifndef STRINGPOOL_HH
#define STRINGPOOL_HH

#include <noncopyable.hh>
#include <string>
#include <map>

class StringPool : private noncopyable
{
public:
	typedef std::map<std::string, unsigned> Pool;
	typedef Pool::iterator Ref;

	static StringPool& instance();
	Ref add(const std::string& str);
	void release(Ref ref);

private:
	StringPool();
	~StringPool();

	Pool pool;
};


class StringRef
{
public:
	StringRef();
	explicit StringRef(const std::string& str);
	StringRef(const StringRef& other);
	~StringRef();

	StringRef& operator=(StringRef other);
	StringRef& operator=(const std::string& str);

	operator const std::string&();
	const std::string& str() const;

	void swap(StringRef& other);

	bool empty() const;
	void clear();

private:
	StringPool::Ref ref;

	friend bool operator==(const StringRef&, const StringRef&);
};

bool operator==(const StringRef& ref1, const StringRef& ref2);
bool operator==(const StringRef& ref1, const std::string& str2);
bool operator==(const std::string& str1, const StringRef& ref2);
bool operator!=(const StringRef& ref1, const StringRef& ref2);
bool operator!=(const StringRef& ref1, const std::string& str2);
bool operator!=(const std::string& str1, const StringRef& ref2);
std::string operator+(const StringRef& ref1, const StringRef& ref2);
std::string operator+(const StringRef& ref1, const std::string& str2);
std::string operator+(const std::string& str1, const StringRef& ref2);
std::string operator+(const StringRef& ref1, const char* str2);
std::string operator+(const char* str1, const StringRef& ref2);
std::string operator+(const StringRef& ref1, char char2);
std::string operator+(char char1, const StringRef& ref2);

#endif
