// $Id$

#ifndef __MSXEXCEPTION_HH__
#define __MSXEXCEPTION_HH__

#include <string>

class MSXException
{
public:
	MSXException(const std::string &descs="", int errnos=0):desc(descs),errorcode(errnos) {}
	virtual ~MSXException() {}
	const std::string desc;
	const int    errorcode;
};

#endif
