// $Id$

#ifndef __PLUGGABLE__
#define __PLUGGABLE__

#include <string>

using std::string;

class EmuTime;
class Connector;

class Pluggable
{
public:
	/**
	 * A pluggable has a name
	 */
	virtual const string &getName() const;

	/**
	 * A pluggable belongs to a certain class. A pluggable only fits in
	 * connectors of the same class.
	 */
	virtual const string &getClass() const = 0;

	/**
	 * This method is called when this pluggable is inserted in a
	 * connector. The default implementation does nothing.
	 */
	virtual void plug(Connector* connector, const EmuTime &time) = 0;

	/**
	 * This method is called when this pluggable is removed from a 
	 * conector. The default implementation does nothing.
	 */
	virtual void unplug(const EmuTime &time) = 0;
};

#endif //__PLUGGABLE__
