// $Id$

#include "Pluggable.hh"


const std::string &Pluggable::getName() const
{
	static const std::string name("--empty--");
	return name;
}
