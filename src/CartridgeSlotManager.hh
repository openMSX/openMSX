// $Id$

#ifndef __CARTRIDGESLOTMANAGER_HH__
#define __CARTRIDGESLOTMANAGER_HH__

namespace openmsx {

class HardwareConfig;

class CartridgeSlotManager
{
public:
	static CartridgeSlotManager& instance();

	void reserveSlot(int slot);
	void readConfig();
	void getSlot(int slot, int &ps, int &ss);
	void getSlot(int &ps, int &ss);

private:
	CartridgeSlotManager();
	~CartridgeSlotManager();
	void createExternal(unsigned ps, unsigned ss);

	int slots[16];
	int slotCounter;
	HardwareConfig& hardwareConfig;
};

} // namespace openmsx

#endif

