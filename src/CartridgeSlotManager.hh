#ifndef CARTRIDGESLOTMANAGER_HH
#define CARTRIDGESLOTMANAGER_HH

#include "RecordedCommand.hh"
#include "InfoTopic.hh"
#include "string_ref.hh"
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class ExtCmd;
class HardwareConfig;

class CartridgeSlotManager
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
	int allocateAnyPrimarySlot(const HardwareConfig& hwConfig);
	int allocateSpecificPrimarySlot(unsigned slot, const HardwareConfig& hwConfig);
	void freePrimarySlot(int ps, const HardwareConfig& hwConfig);

	bool isExternalSlot(int ps, int ss, bool convert) const;

private:
	int getSlot(int ps, int ss) const;

	MSXMotherBoard& motherBoard;

	class CartCmd final : public RecordedCommand {
	public:
		CartCmd(CartridgeSlotManager& manager, MSXMotherBoard& motherBoard,
			string_ref commandName);
		void execute(array_ref<TclObject> tokens, TclObject& result,
			     EmuTime::param time) override;
		std::string help(const std::vector<std::string>& tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;
		bool needRecord(array_ref<TclObject> tokens) const override;
	private:
		const HardwareConfig* getExtensionConfig(string_ref cartname);
		CartridgeSlotManager& manager;
		CliComm& cliComm;
	} cartCmd;

	struct CartridgeSlotInfo final : InfoTopic {
		CartridgeSlotInfo(InfoCommand& machineInfoCommand);
		void execute(array_ref<TclObject> tokens,
			     TclObject& result) const override;
		std::string help(const std::vector<std::string>& tokens) const override;
	} extSlotInfo;

	struct Slot {
		Slot();
		~Slot();
		bool exists() const;
		bool used(const HardwareConfig* allowed = nullptr) const;

		std::unique_ptr<CartCmd> cartCommand;
		std::unique_ptr<ExtCmd> extCommand;
		const HardwareConfig* config;
		unsigned useCount;
		int ps;
		int ss;
	};
	static const unsigned MAX_SLOTS = 16 + 4;
	Slot slots[MAX_SLOTS];
};

} // namespace openmsx

#endif
