#ifndef PLUGGABLE_HH
#define PLUGGABLE_HH

#include "EmuTime.hh"

#include "zstring_view.hh"

namespace openmsx {

class Connector;

class Pluggable
{
public:
	virtual ~Pluggable() = default;

	/** Name used to identify this pluggable.
	  */
	[[nodiscard]] virtual zstring_view getName() const;

	/** A pluggable belongs to a certain class. A pluggable only fits in
	  * connectors of the same class.
	  */
	[[nodiscard]] virtual zstring_view getClass() const = 0;

	/** Description for this pluggable.
	  */
	[[nodiscard]] virtual zstring_view getDescription() const = 0;

	/** This method is called when this pluggable is inserted in a
	  * connector.
	  * @throws PlugException
	  */
	void plug(Connector& connector, EmuTime time);

	/** This method is called when this pluggable is removed from a
	  * connector.
	  */
	void unplug(EmuTime time);

	/** Get the connector this Pluggable is plugged into. Returns nullptr
	  * if this Pluggable is not plugged.
	  */
	[[nodiscard]] Connector* getConnector() const { return connector; }

	/** Returns true if this pluggable is currently plugged into a connector.
	  * The method getConnector() can also be used, but this is more
	  * descriptive.
	  */
	[[nodiscard]] bool isPluggedIn() const { return getConnector() != nullptr; }

protected:
	Pluggable();
	virtual void plugHelper(Connector& newConnector, EmuTime time) = 0;
	/* implementations of unplugHelper() may not throw exceptions. */
	virtual void unplugHelper(EmuTime time) = 0;

	friend class Connector; // for de-serialization
	void setConnector(Connector* conn) { connector = conn; }

private:
	Connector* connector;
};

} // namespace openmsx

#endif
