#ifndef CONNECTOR_HH
#define CONNECTOR_HH

#include "EmuTime.hh"
#include "serialize_meta.hh"
#include <memory>
#include <string_view>

namespace openmsx {

class Pluggable;
class PluggingController;

/**
 * Represents something you can plug devices into.
 * Examples are a joystick port, a printer port, a MIDI port etc.
 * When there is not an actual Pluggable plugged in, a dummy Pluggable
 * is used.
 */
class Connector
{
public:
	Connector(const Connector&) = delete;
	Connector(Connector&&) = delete;
	Connector& operator=(const Connector&) = delete;
	Connector& operator=(Connector&&) = delete;

	/**
	 * Name that identifies this connector.
	 */
	[[nodiscard]] const std::string& getName() const { return name; }

	/**
	 * Get a description for this connector
	 */
	[[nodiscard]] virtual std::string_view getDescription() const = 0;

	/**
	 * A Connector belong to a certain class.
	 * Only Pluggables of this class can be plugged in this Connector.
	 */
	[[nodiscard]] virtual std::string_view getClass() const = 0;

	/**
	 * This plugs a Pluggable in this Connector.
	 * The default implementation is ok.
	 * @throw PlugException
	 */
	virtual void plug(Pluggable& device, EmuTime::param time);

	/**
	 * This unplugs the currently inserted Pluggable from this Connector.
	 * It is replaced by the dummy Pluggable provided by the concrete
	 * Connector subclass.
	 */
	virtual void unplug(EmuTime::param time);

	/**
	 * Returns the Pluggable currently plugged in.
	 */
	[[nodiscard]] Pluggable& getPlugged() const { return *plugged; }

	[[nodiscard]] PluggingController& getPluggingController() const {
		return pluggingController;
	}

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

protected:
	/**
	 * Creates a new Connector.
	 * @param pluggingController PluggingController.
	 * @param name Name that identifies this connector.
	 * @param dummy Dummy Pluggable whose class matches this Connector.
	 */
	Connector(PluggingController& pluggingController,
	          std::string name, std::unique_ptr<Pluggable> dummy);

	~Connector();

private:
	PluggingController& pluggingController;
	const std::string name;
	const std::unique_ptr<Pluggable> dummy;
	Pluggable* plugged;
};

REGISTER_BASE_CLASS(Connector, "Connector");

} // namespace openmsx

#endif
