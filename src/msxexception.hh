// $Id$

#ifndef __MSXEXCEPTION_HH__
#define __MSXEXCEPTION_HH__

class MSXException
{
public:
	MSXException(const string &descs="", int errnos=0):desc(descs),errorcode(errnos) {}
	virtual ~MSXException() {}
	const string desc;
	const int    errorcode;
};

#endif
