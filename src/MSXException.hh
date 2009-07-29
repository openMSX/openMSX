// $Id$

#ifndef MSXEXCEPTION_HH
#define MSXEXCEPTION_HH

#include <string>

namespace openmsx {

class MSXException
{
public:
	explicit MSXException(const std::string& message);
	explicit MSXException(const char*        message);
	~MSXException();

	const std::string& getMessage() const {
		return message;
	}

private:
	const std::string message;
};

class FatalError
{
public:
	explicit FatalError(const std::string& message);
	explicit FatalError(const char*        message);
	~FatalError();

	const std::string& getMessage() const {
		return message;
	}

private:
	const std::string message;
};

} // namespace openmsx

#endif
