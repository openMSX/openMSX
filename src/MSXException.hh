// $Id$

#ifndef __MSXEXCEPTION_HH__
#define __MSXEXCEPTION_HH__

#include <string>


class MSXException
{
	public:
		MSXException(const std::string &message_)
			: message(message_)
		{
		}
		
		virtual ~MSXException()
		{
		}
		
		const std::string &getMessage() const {
			return message;
		}
		
	private:
		const std::string message;
};

#endif
