// $Id$

#include "Pluggable.hh"


const std::string &Pluggable::getName()
{
	static const std::string name("--empty--");
	return name;
}
