// $Id$

#ifndef CARTRIDGESLOTMANAGER_HH
#define CARTRIDGESLOTMANAGER_HH

#include <string>
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class XMLElement;
class CartCmd;

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

	int getSpecificSlot(int slot, int& ps, int& ss);
	int getAnyFreeSlot(int& ps, int& ss);
	int getFreePrimarySlot(int& ps);
	void freeSlot(int slot);

	bool isExternalSlot(int ps, int ss, bool convert = false) const;

private:
	int getSlot(int ps, int ss) const;

	struct Slot {
		Slot() : ps(0), ss(0), used(false), command(NULL) {}
		bool exists() const { return command; }

		int ps;
		int ss;
		bool used;
		CartCmd* command;
	};
	static const int MAX_SLOTS = 16 + 4;
	Slot slots[MAX_SLOTS];
	MSXMotherBoard& motherBoard;
	std::auto_ptr<CartCmd> cartCmd;
};

} // namespace openmsx

#endif
