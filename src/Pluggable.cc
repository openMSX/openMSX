// $Id$

#include "Pluggable.hh"


namespace openmsx {

Pluggable::~Pluggable()
{
}

const string &Pluggable::getName() const
{
	static const string name("--empty--");
	return name;
}

} // namespace openmsx
