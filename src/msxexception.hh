// $Id$

#ifndef __MSXEXCEPTION_HH__
#define __MSXEXCEPTION_HH__

class MSXException
{
public:
	MSXException(const string &descs="", int errno=0):desc(descs),errno(errnos) {}
	const string desc;
	const int    errno;
};

#endif
