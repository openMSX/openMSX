// $Id$

#ifndef PLUGGABLE_HH
#define PLUGGABLE_HH

#include "EmuTime.hh"
#include "noncopyable.hh"
#include <string>

namespace openmsx {

class Connector;

class Pluggable : private noncopyable
{
public:
	Pluggable();
	virtual ~Pluggable();

	/** Name used to identify this pluggable.
	  */
	virtual const std::string& getName() const;

	/** A pluggable belongs to a certain class. A pluggable only fits in
	  * connectors of the same class.
	  */
	virtual const std::string& getClass() const = 0;

	/** Description for this pluggable.
	  */
	virtual const std::string& getDescription() const = 0;

	/** This method is called when this pluggable is inserted in a
	 * connector.
	 * @throws PlugException
	 */
	void plug(Connector& connector, EmuTime::param time);

	/** This method is called when this pluggable is removed from a
	  * conector.
	  */
	void unplug(EmuTime::param time);

	/** Get the connector this Pluggable is plugged into. Returns a NULL
	  * pointer if this Pluggable is not plugged.
	  */
	Connector* getConnector() const;

	/** Returns true if this pluggable is currently plugged into a connector.
	 * The method getConnector() can also be used, but this is more
	 * descriptive.
	 */
	bool isPluggedIn() const { return getConnector() != 0; }

protected:
	virtual void plugHelper(Connector& newConnector, EmuTime::param time) = 0;
	/* implementations of unplugHelper() may not throw exceptions. */
	virtual void unplugHelper(EmuTime::param time) = 0;

	friend class Connector; // for de-serialization
	void setConnector(Connector* conn);

private:
	Connector* connector;
};

} // namespace openmsx

#endif
