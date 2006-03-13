// $Id$

#ifndef CARTRIDGESLOTMANAGER_HH
#define CARTRIDGESLOTMANAGER_HH

#include <string>
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class XMLElement;
class CartCmd;
class HardwareConfig;

class CartridgeSlotManager
{
public:
	explicit CartridgeSlotManager(MSXMotherBoard& motherBoard);
	~CartridgeSlotManager();

	static int getSlotNum(const std::string& slot);

	void createExternalSlot(int ps);
	void createExternalSlot(int ps, int ss);
	void removeExternalSlot(int ps);
	void removeExternalSlot(int ps, int ss);
	void testRemoveExternalSlot(int ps) const;
	void testRemoveExternalSlot(int ps, int ss) const;

	int getSpecificSlot(int slot, int& ps, int& ss,
	                    const HardwareConfig& hwConfig);
	int getAnyFreeSlot(int& ps, int& ss, const HardwareConfig& hwConfig);
	int getFreePrimarySlot(int& ps, const HardwareConfig& hwConfig);
	void freeSlot(int slot);

	bool isExternalSlot(int ps, int ss, bool convert = false) const;

private:
	int getSlot(int ps, int ss) const;

	struct Slot {
		Slot() : ps(0), ss(0), command(NULL), config(NULL) {}
		bool exists() const { return command; }
		bool used() const { return config; }

		int ps;
		int ss;
		CartCmd* command;
		const HardwareConfig* config;
	};
	static const int MAX_SLOTS = 16 + 4;
	Slot slots[MAX_SLOTS];
	MSXMotherBoard& motherBoard;
	std::auto_ptr<CartCmd> cartCmd;
	friend class CartCmd;
};

} // namespace openmsx

#endif
