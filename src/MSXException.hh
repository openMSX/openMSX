// $Id$

#ifndef __MSXEXCEPTION_HH__
#define __MSXEXCEPTION_HH__

#include <string>

using std::string;

namespace openmsx {

class MSXException
{
public:
	MSXException(const string& message_)
		: message(message_) { }
	~MSXException() { }
	
	const string& getMessage() const {
		return message;
	}
	
private:
	const string message;
};


class FatalError
{
public:
	FatalError(const string& message_)
		: message(message_) { }
	~FatalError() { }

	const string& getMessage() const {
		return message;
	}

private:
	const string message;
};

} // namespace openmsx

#endif
