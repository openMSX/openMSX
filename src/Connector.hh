// $Id$

#ifndef __CONNECTOR__
#define __CONNECTOR__

#include <string>

//forward declarations
class Pluggable;
class EmuTime;


class Connector {
public:
	Connector();

	/**
	 * A connector has a name
	 */
	virtual const std::string &getName() const = 0;

	/**
	 * A connector belong to a certain class. All pluggables of this
	 * class can be plugged in this connector
	 */
	virtual const std::string &getClass() const = 0;

	/**
	 * This plugs a pluggable in this connector. The default
	 * implementation is ok.
	 */
	virtual void plug(Pluggable *device, const EmuTime &time);

	/**
	 * This unplugs a device (if inserted) from this connector. 
	 * The easiest implementation for handling unplugged connectors
	 * is to insert a dummy pluggable. If a subclass wants this behavior
	 * it must redefine this method (the new implementation must call 
	 * this original method)
	 */
	virtual void unplug(const EmuTime &time);

	/**
	 * Returns the Pluggable currently plugged in
	 */
	Pluggable* getPlug() const;
	
protected:
	Pluggable *pluggable;
};

#endif //__CONNECTOR__
