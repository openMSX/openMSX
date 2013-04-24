#ifndef CARTRIDGESLOTMANAGER_HH
#define CARTRIDGESLOTMANAGER_HH

#include "noncopyable.hh"
#include "string_ref.hh"
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class CartCmd;
class HardwareConfig;
class CartridgeSlotInfo;

class CartridgeSlotManager : private noncopyable
{
public:
	explicit CartridgeSlotManager(MSXMotherBoard& motherBoard);
	~CartridgeSlotManager();

	static int getSlotNum(string_ref slot);

	void createExternalSlot(int ps);
	void createExternalSlot(int ps, int ss);
	void removeExternalSlot(int ps);
	void removeExternalSlot(int ps, int ss);
	void testRemoveExternalSlot(int ps, const HardwareConfig& allowed) const;
	void testRemoveExternalSlot(int ps, int ss, const HardwareConfig& allowed) const;

	// Query/allocate/free external slots
	void getSpecificSlot(unsigned slot, int& ps, int& ss) const;
	void getAnyFreeSlot(int& ps, int& ss) const;
	void allocateSlot(int ps, int ss, const HardwareConfig& hwConfig);
	void freeSlot(int ps, int ss, const HardwareConfig& hwConfig);

	// Allocate/free external primary slots
	void allocatePrimarySlot(int& ps, const HardwareConfig& hwConfig);
	void freePrimarySlot(int ps, const HardwareConfig& hwConfig);

	bool isExternalSlot(int ps, int ss, bool convert) const;

private:
	int getSlot(int ps, int ss) const;

	struct Slot {
		Slot();
		~Slot();
		bool exists() const;
		bool used(const HardwareConfig* allowed = nullptr) const;

		std::unique_ptr<CartCmd> command;
		const HardwareConfig* config;
		unsigned useCount;
		int ps;
		int ss;
	};
	static const unsigned MAX_SLOTS = 16 + 4;
	Slot slots[MAX_SLOTS];
	MSXMotherBoard& motherBoard;
	const std::unique_ptr<CartCmd> cartCmd;
	const std::unique_ptr<CartridgeSlotInfo> extSlotInfo;
	friend class CartCmd;
	friend class CartridgeSlotInfo;
};

} // namespace openmsx

#endif
