// $Id$

#ifndef __MSXEXCEPTION_HH__
#define __MSXEXCEPTION_HH__

#include <string>

using namespace std;

class MSXException
{
	public:
		MSXException(const string &message_)
			: message(message_)
		{
		}
		
		virtual ~MSXException()
		{
		}
		
		const string &getMessage() const {
			return message;
		}
		
	private:
		const string message;
};

#endif
