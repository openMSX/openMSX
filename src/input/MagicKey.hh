// $Id$

#ifndef MAGICKEY_HH
#define MAGICKEY_HH

#include "JoystickDevice.hh"
#include "serialize_meta.hh"

namespace openmsx {

class MagicKey : public JoystickDevice
{
public:
	// Pluggable
	virtual const std::string& getName() const;
	virtual const std::string& getDescription() const;
	virtual void plugHelper(Connector& connector, const EmuTime& time);
	virtual void unplugHelper(const EmuTime& time);

	// JoystickDevice
	virtual byte read(const EmuTime& time);
	virtual void write(byte value, const EmuTime& time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);
};

REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, MagicKey, "MagicKey");

} // namespace openmsx

#endif
