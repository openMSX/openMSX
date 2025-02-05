#ifndef IMGUI_DISASSEMBLY_HH
#define IMGUI_DISASSEMBLY_HH

#include "ImGuiPart.hh"

#include <optional>
#include <span>
#include <string>
#include <utility>

namespace openmsx {

class Debugger;
class MSXDevice;
class MSXCPUInterface;
class MSXRom;
class RomBlockDebuggableBase;
struct Symbol;
class SymbolManager;

class ImGuiDisassembly final : public ImGuiPart
{
public:
	ImGuiDisassembly(ImGuiManager& manager_, size_t index);

	[[nodiscard]] zstring_view iniName() const override { return title; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void loadEnd() override;
	void paint(MSXMotherBoard* motherBoard) override;

	void signalBreak() { if (scrollToPcOnBreak) syncDisassemblyWithPC = true; }
	void setGotoTarget(uint16_t target);
	void syncWithPC() { syncDisassemblyWithPC = true; }

public:
	static void actionToggleBp(MSXMotherBoard& motherBoard);
	[[nodiscard]] static std::pair<const MSXRom*, RomBlockDebuggableBase*>
		getRomBlocks(Debugger& debugger, const MSXDevice* device);

private:
	unsigned disassemble(
		const MSXCPUInterface& cpuInterface, unsigned addr, unsigned pc, EmuTime::param time,
		std::span<uint8_t, 4> opcodes, std::string& mnemonic,
		std::optional<uint16_t>& mnemonicAddr, std::span<const Symbol* const>& mnemonicLabels);
	void disassembleToClipboard(
		MSXCPUInterface& cpuInterface, unsigned pc, EmuTime::param time,
		unsigned minAddr, unsigned maxAddr);

public:
	bool show = true;

private:
	SymbolManager& symbolManager;
	std::string title;
	size_t cycleLabelsCounter = 0;

	std::string gotoAddr;
	std::string runToAddr;
	std::optional<unsigned> gotoTarget;
	std::optional<float> setDisassemblyScrollY;
	bool followPC = false;
	bool scrollToPcOnBreak = true;

	bool syncDisassemblyWithPC = false;
	float disassemblyScrollY = 0.0f;

	static constexpr auto persistentElements = std::tuple{
		PersistentElement{"show",              &ImGuiDisassembly::show},
		PersistentElement{"followPC",          &ImGuiDisassembly::followPC},
		PersistentElement{"scrollToPcOnBreak", &ImGuiDisassembly::scrollToPcOnBreak},
		PersistentElement{"disassemblyY",      &ImGuiDisassembly::disassemblyScrollY},
	};
};

} // namespace openmsx

#endif
