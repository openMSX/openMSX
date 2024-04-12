#ifndef CARTRIDGESLOTMANAGER_HH
#define CARTRIDGESLOTMANAGER_HH

#include "RecordedCommand.hh"
#include "MSXMotherBoard.hh"
#include "InfoTopic.hh"

#include "TclObject.hh"
#include "narrow.hh"
#include "ranges.hh"
#include "strCat.hh"

#include <array>
#include <cassert>
#include <optional>
#include <string_view>

namespace openmsx {

class HardwareConfig;

class CartridgeSlotManager
{
public:
	static constexpr unsigned MAX_SLOTS = 16 + 4;

public:
	explicit CartridgeSlotManager(MSXMotherBoard& motherBoard);
	~CartridgeSlotManager();

	static int getSlotNum(std::string_view slot);

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
	[[nodiscard]] int allocateAnyPrimarySlot(const HardwareConfig& hwConfig);
	[[nodiscard]] int allocateSpecificPrimarySlot(unsigned slot, const HardwareConfig& hwConfig);
	void freePrimarySlot(int ps, const HardwareConfig& hwConfig);

	[[nodiscard]] bool isExternalSlot(int ps, int ss, bool convert) const;

	// TODO allow to query more info like cart or ext inserted ...
	[[nodiscard]] bool slotExists(unsigned slot) const {
		assert(slot < MAX_SLOTS);
		return slots[slot].exists();
	}
	[[nodiscard]] int getNumberOfSlots() const {
		return narrow<int>(ranges::count_if(slots, &Slot::exists));
	}
	[[nodiscard]] const HardwareConfig* getConfigForSlot(unsigned slot) const {
		assert(slot < MAX_SLOTS);
		return slots[slot].config;
	}
	[[nodiscard]] std::pair<int, int> getPsSs(unsigned slot) const {
		assert(slot < MAX_SLOTS);
		return {slots[slot].ps, slots[slot].ss};
	}
	[[nodiscard]] std::string getPsSsString(unsigned slot) const {
		auto [ps, ss] = getPsSs(slot);
		std::string result = strCat(ps);
		if (ss != -1) strAppend(result, '-', ss);
		return result;
	}
	[[nodiscard]] std::optional<unsigned> findSlotWith(const HardwareConfig& config) const {
		for (auto slot : xrange(MAX_SLOTS)) {
			if (slots[slot].config == &config) return slot;
		}
		return {};
	}

private:
	[[nodiscard]] unsigned getSlot(int ps, int ss) const;

private:
	MSXMotherBoard& motherBoard;

	class CartCmd final : public RecordedCommand {
	public:
		CartCmd(CartridgeSlotManager& manager, MSXMotherBoard& motherBoard,
			std::string_view commandName);
		void execute(std::span<const TclObject> tokens, TclObject& result,
			     EmuTime::param time) override;
		[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;
		[[nodiscard]] bool needRecord(std::span<const TclObject> tokens) const override;
	private:
		[[nodiscard]] const HardwareConfig* getExtensionConfig(std::string_view cartName) const;
		CartridgeSlotManager& manager;
		CliComm& cliComm;
	} cartCmd;

	struct CartridgeSlotInfo final : InfoTopic {
		explicit CartridgeSlotInfo(InfoCommand& machineInfoCommand);
		void execute(std::span<const TclObject> tokens,
			     TclObject& result) const override;
		[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
	} extSlotInfo;

	struct Slot : public MediaInfoProvider {
		~Slot();

		// MediaInfoProvider
		void getMediaInfo(TclObject& result) override;

		[[nodiscard]] bool exists() const;
		[[nodiscard]] bool used(const HardwareConfig* allowed = nullptr) const;

		std::optional<CartCmd> cartCommand;
		std::optional<ExtCmd> extCommand;
		const HardwareConfig* config = nullptr;
		unsigned useCount = 0;
		int ps = 0;
		int ss = 0;
		MSXCPUInterface* cpuInterface;
	};
	std::array<Slot, MAX_SLOTS> slots;
};

} // namespace openmsx

#endif
