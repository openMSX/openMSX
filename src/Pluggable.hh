// $Id$

#ifndef __PLUGGABLE__
#define __PLUGGABLE__

#include "MSXException.hh"
#include <string>

using std::string;

namespace openmsx {

class EmuTime;
class Connector;

/** Thrown when a plug action fails.
  */
class PlugException: public MSXException
{
public:
	PlugException(const string& message)
		: MSXException(message) {}
};

class Pluggable
{
public:
	virtual ~Pluggable();

	/**
	 * Name used to identify this pluggable.
	 */
	virtual const string& getName() const;

	/**
	 * A pluggable belongs to a certain class. A pluggable only fits in
	 * connectors of the same class.
	 */
	virtual const string& getClass() const = 0;

	/**
	 * Description for this pluggable.
	 */
	virtual const string& getDescription() const = 0;
	
	/**
	 * This method is called when this pluggable is inserted in a
	 * connector. The default implementation does nothing.
	 */
	virtual void plug(Connector* connector, const EmuTime& time)
		throw(PlugException) = 0;

	/**
	 * This method is called when this pluggable is removed from a
	 * conector. The default implementation does nothing.
	 */
	virtual void unplug(const EmuTime& time) = 0;
};

} // namespace openmsx

#endif //__PLUGGABLE__
