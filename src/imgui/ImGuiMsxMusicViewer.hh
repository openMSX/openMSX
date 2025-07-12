#ifndef IMGUI_MSXMUSICVIEWER_HH
#define IMGUI_MSXMUSICVIEWER_HH

#include "ImGuiPart.hh"

#include <array>
#include <cstdint>
#include <functional>
#include <span>
#include <vector>

namespace openmsx {

class YM2413;

class ImGuiMsxMusicViewer final : public ImGuiPart
{
public:
	explicit ImGuiMsxMusicViewer(ImGuiManager& manager);
	void paint(MSXMotherBoard* motherBoard) override;

private:
	struct Hover {
		Hover() = default;
		Hover(int r, uint8_t m) : reg(uint8_t(r)), mask(m) {}

		uint8_t reg = 255;
		uint8_t mask = 0;

		[[nodiscard]] bool match(const Hover& h) const {
			return h.reg == reg && ((h.mask & mask) != 0);
		}
	};
	struct Hovers {
		Hovers() = default;
		Hovers(Hover h) : h1(h) {}
		Hovers(int r, uint8_t m) : h1(r, m) {}
		Hovers(int r1, uint8_t m1, int r2, uint8_t m2) : h1(r1, m1), h2(r2, m2) {}

		Hover h1, h2;

		[[nodiscard]] bool match(const Hovers& h) const {
			return h1.match(h.h1) || h1.match(h.h2) ||
			       h2.match(h.h1) || h2.match(h.h2);
		}
	};
	using HoverList = std::vector<Hovers>;
	[[nodiscard]] static bool match(const HoverList& list, Hovers hovers);

	static constexpr int historySize = 60;
	struct ChipState {
		std::array<std::array<float, historySize>, 9 + 5> keyHistory = {};
		HoverList hoverList; // items hovered on the previous frame
		int selectedInstr = 0;
	};

	void drawCell(ChipState& cs, HoverList& newHoverList, std::span<const uint8_t> regs, Hovers hovers,
	              std::function<void(uint32_t)> func,
	              float highlightWidth = 0.0f, gl::vec2* getPos = nullptr);

	void paintMelodicChannel(ChipState& cs, HoverList& newHoverList, std::span<const uint8_t, 64> regs, int ch);
	void paintRhythmChannel(ChipState& cs, HoverList& newHoverList, std::span<const uint8_t, 64> regs, int ch);
	void paintChannels(ChipState& cs, HoverList& newHoverList, std::span<const uint8_t, 64> regs);
	void paintInstrument(ChipState& cs, HoverList& newHoverList, std::span<const uint8_t, 64> regs);
	void paintRegisters(ChipState& cs, HoverList& newHoverList, std::span<const uint8_t, 64> regs);
	void paintMsxMusic(const YM2413& ym2413);

public:
	bool show = false;

private:
	std::map<std::string, ChipState> chips;
};

} // namespace openmsx

#endif
