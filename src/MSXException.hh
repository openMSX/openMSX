// $Id$

#ifndef __MSXEXCEPTION_HH__
#define __MSXEXCEPTION_HH__

#include <string>

class MSXException
{
	public:
		MSXException(const std::string &desc) : desc(desc) {}
		virtual ~MSXException() {}

		const std::string desc;
};

#endif
