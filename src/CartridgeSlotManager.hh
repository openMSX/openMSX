// $Id$

#ifndef CARTRIDGESLOTMANAGER_HH
#define CARTRIDGESLOTMANAGER_HH

#include <string>

namespace openmsx {

class MSXMotherBoard;
class XMLElement;

class CartridgeSlotManager
{
public:
	explicit CartridgeSlotManager(MSXMotherBoard& motherBoard);
	~CartridgeSlotManager();

	void readConfig(const XMLElement& config);

	void reserveSlot(int slot);
	void unreserveSlot(int slot);

	int getReservedSlot(int slot, int& ps, int& ss);
	int getAnyFreeSlot(int& ps, int& ss);
	void freeSlot(int slot);

	bool isExternalSlot(int ps, int ss) const;

	static int getSlotNum(const std::string& slot);

private:
	int getFreePrimarySlot();
	void createExternal(int ps, int ss);

	struct Slot {
		Slot() : ps(0), ss(0)
		       , reserved(false), exists(false), used(false) {}
		int ps;
		int ss;
		bool reserved;
		bool exists;
		bool used;
	};
	Slot slots[16];
	MSXMotherBoard& motherBoard;
};

} // namespace openmsx

#endif
