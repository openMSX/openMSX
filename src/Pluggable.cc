// $Id$

#include "Pluggable.hh"


const string &Pluggable::getName() const
{
	static const string name("--empty--");
	return name;
}
