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
	Pluggable();
	virtual ~Pluggable();

	/** Name used to identify this pluggable.
	  */
	virtual const string& getName() const;

	/** A pluggable belongs to a certain class. A pluggable only fits in
	  * connectors of the same class.
	  */
	virtual const string& getClass() const = 0;

	/** Description for this pluggable.
	  */
	virtual const string& getDescription() const = 0;

	/** This method is called when this pluggable is inserted in a
	 * connector.
	 */
	void plug(Connector* connector, const EmuTime& time)
		throw(PlugException);

	/** This method is called when this pluggable is removed from a
	  * conector.
	  */
	void unplug(const EmuTime& time);

	/** Get the connector this Pluggable is plugged into. Returns a NULL
	  * pointer if this Pluggable is not plugged.
	  */
	Connector* getConnector() const;
	
protected:
	virtual void plugHelper(Connector* newConnector, const EmuTime& time)
		throw(PlugException) = 0;

	virtual void unplugHelper(const EmuTime& time) = 0;

	Connector* connector;
};

} // namespace openmsx

#endif //__PLUGGABLE__
