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

	static int getSlotNum(const std::string& slot);

	void createExternalSlot(int ps);
	void createExternalSlot(int ps, int ss);
	void removeExternalSlot(int ps);
	void removeExternalSlot(int ps, int ss);

	int getSpecificSlot(int slot, int& ps, int& ss);
	int getAnyFreeSlot(int& ps, int& ss);
	int getFreePrimarySlot(int& ps);
	void freeSlot(int slot);

	bool isExternalSlot(int ps, int ss, bool convert = false) const;

private:

	struct Slot {
		Slot() : ps(0), ss(0), exists(false), used(false) {}
		int ps;
		int ss;
		bool exists;
		bool used;
	};
	static const int MAX_SLOTS = 16 + 4;
	Slot slots[MAX_SLOTS];
	MSXMotherBoard& motherBoard;
};

} // namespace openmsx

#endif
