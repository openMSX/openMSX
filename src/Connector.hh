// $Id$

#ifndef CONNECTOR
#define CONNECTOR

#include <string>
#include <memory>

namespace openmsx {

class EmuTime;
class Pluggable;

/**
 * Represents something you can plug devices into.
 * Examples are a joystick port, a printer port, a MIDI port etc.
 * When there is not an actual Pluggable plugged in, a dummy Pluggable
 * is used.
 */
class Connector
{
public:
	/**
	 * Name that identifies this connector.
	 */
	const std::string& getName() const;

	/**
	 * Get a description for this connector
	 */
	virtual const std::string& getDescription() const = 0;

	/**
	 * A Connector belong to a certain class.
	 * Only Pluggables of this class can be plugged in this Connector.
	 */
	virtual const std::string& getClass() const = 0;

	/**
	 * This plugs a Pluggable in this Connector.
	 * The default implementation is ok.
	 * @throw PlugException
	 */
	virtual void plug(Pluggable* device, const EmuTime& time);

	/**
	 * This unplugs the currently inserted Pluggable from this Connector.
	 * It is replaced by the dummy Pluggable provided by the concrete
	 * Connector subclass.
	 */
	virtual void unplug(const EmuTime& time);

	/**
	 * Returns the Pluggable currently plugged in.
	 */
	virtual Pluggable& getPlugged() const = 0;

protected:
	/**
	 * Creates a new Connector.
	 * @param name Name that identifies this connector.
	 * @param dummy Dummy Pluggable whose class matches this Connector.
	 */
	Connector(const std::string& name, std::auto_ptr<Pluggable> dummy);

	virtual ~Connector();

	Pluggable* plugged;

private:
	const std::string name;
	const std::auto_ptr<Pluggable> dummy;
};

} // namespace openmsx

#endif
