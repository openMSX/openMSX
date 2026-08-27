#ifndef IMGUI_IOPORTS_HH
#define IMGUI_IOPORTS_HH

#include "ImGuiPart.hh"

#include "EmuTime.hh"

#include <cstdint>
#include <string>
#include <vector>

namespace openmsx {

class MSXCPUInterface;

class ImGuiIoPorts final : public ImGuiPart
{
public:
	using ImGuiPart::ImGuiPart;

	// Note: not "ioports", that's already taken by the hex editor on the
	// 'ioports' debuggable (DebuggableEditor::iniName() returns its title).
	[[nodiscard]] zstring_view iniName() const override { return "io ports"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void paint(MSXMotherBoard* motherBoard) override;

public:
	bool show = false;

private:
	/** A maximal range of ports with the same device(s) in the same
	  * direction(s).
	  */
	struct Row {
		uint8_t begin; // inclusive
		uint8_t end;   // inclusive
		bool in;
		bool out;
		bool overlap;       // these ports are shared with another device
		std::string device; // comma separated name(s)
	};
	[[nodiscard]] static std::vector<Row> getRows(MSXCPUInterface& cpuInterface);
	[[nodiscard]] static std::string portsText(const Row& row);
	[[nodiscard]] static std::string_view dirText(const Row& row);
	static void sortRows(std::vector<Row>& rows);

	void drawTable(MSXCPUInterface& cpuInterface, bool warnOverlap, EmuTime time);
	void drawValue(MSXCPUInterface& cpuInterface, const Row& row, EmuTime time);

	static constexpr auto persistentElements = std::tuple{
		PersistentElement{"show", &ImGuiIoPorts::show}
	};
};

} // namespace openmsx

#endif
