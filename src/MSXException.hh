// $Id$

#ifndef __MSXEXCEPTION_HH__
#define __MSXEXCEPTION_HH__

#include <string>

namespace openmsx {

class MSXException
{
public:
	MSXException(const std::string& message_)
		: message(message_) { }
	virtual ~MSXException() { }

	const std::string& getMessage() const {
		return message;
	}

private:
	const std::string message;
};

class FatalError
{
public:
	FatalError(const std::string& message_)
		: message(message_) { }
	virtual ~FatalError() { }

	const std::string& getMessage() const {
		return message;
	}

private:
	const std::string message;
};

} // namespace openmsx

#endif // __MSXEXCEPTION_HH__
