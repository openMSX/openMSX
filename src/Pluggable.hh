// $Id$

#ifndef __PLUGGABLE__
#define __PLUGGABLE__

#include <string>


class Pluggable {
public:
	/**
	 * A pluggable has a name
	 */
	virtual const std::string &getName() = 0;

	/**
	 * A pluggable belongs to a certain class. A pluggable only fits in
	 * connectors of the same class.
	 */
	virtual const std::string &getClass() = 0;

	/**
	 * This method is called just before this pluggable is inserted in a
	 * connector. The default implementation does nothing.
	 */
	virtual void plug() {}

	/**
	 * This method is called when this pluggable is removed from a 
	 * conector. The default implementation does nothing.
	 */
	virtual void unplug() {}
};

#endif //__PLUGGABLE__
