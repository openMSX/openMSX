#ifndef PLOTTER_HH
#define PLOTTER_HH

#include "Printer.hh"
#include "BooleanSetting.hh"
#include "EnumSetting.hh"
#include <array>
#include <memory>
#include <string>
#include <utility> // for std::pair
#include <cstdint> // for uint8_t

namespace openmsx {

// Character set options for the plotter
enum class PlotterCharacterSet { International, Japanese, DIN };

// MSXPlotter: emulates Sony PRN-C41 color plotter with 4 pens
class MSXPlotter final : public ImagePrinter {
public:
	explicit MSXPlotter(MSXMotherBoard& motherBoard);

	// Pluggable
	[[nodiscard]] zstring_view getName() const override;
	[[nodiscard]] zstring_view getDescription() const override;
	unsigned getSelectedPen() const;

	// Configuration settings (accessible by ImGui)
	[[nodiscard]] PlotterCharacterSet getCharacterSet() const;
	void setCharacterSet(PlotterCharacterSet cs);
	[[nodiscard]] bool getDipSwitch4() const;
	void setDipSwitch4(bool enabled);
	[[nodiscard]] bool getKanjiSupport() const;
	void setKanjiSupport(bool enabled);

	// Access to settings for persistence
	[[nodiscard]] EnumSetting<PlotterCharacterSet>& getCharacterSetSetting() const { return *charSetSetting; }
	[[nodiscard]] BooleanSetting& getDipSwitch4Setting() const { return *dipSwitch4Setting; }
	[[nodiscard]] BooleanSetting& getKanjiSupportSetting() const { return *kanjiSupportSetting; }

	// Force color output
	[[nodiscard]] bool useColor() const override { return true; }

	// A4 Landscape as specified in manual (296mm x 210mm)
	[[nodiscard]] std::pair<double, double> getPaperSize() const override {
		return {210.0, 296.0};
	}

	// Override processCharacter to handle plotter-specific commands
	void processCharacter(uint8_t data) override;

	// (Optional) serialization
	template<typename Archive>
	void serialize(Archive&, unsigned) {}

	// Implement required pure virtuals from ImagePrinter
	std::pair<unsigned, unsigned> getNumberOfDots() override;
	void resetSettings() override;
	unsigned calcEscSequenceLength(uint8_t character) override;
	void processEscSequence() override;
	void ensurePrintPage() override;

private:
	// Mode state
	enum class Mode { TEXT, GRAPHIC };
	Mode mode = Mode::TEXT;

	// ESC sequence parsing state
	enum class EscState { NONE, ESC, ESC_C };
	EscState escState = EscState::NONE;
	bool printNext = false; // For 0x01 literal prefix

	// Pen/color state
	unsigned selectedPen = 0; // 0=black, 1=blue, 2=green, 3=red
	bool penDown = true; // pen starts down for drawing

	// Plotter head position (logical steps, relative to origin)
	double plotterX = 0.0;
	double plotterY = 0.0;

    // Origin offset (physical steps)
    double originX = 0.0;
    double originY = 0.0;

    // Line style
    unsigned lineType = 0; // 0=solid, 1-14=dashed
    double dashDistance = 0.0;
    double maxLineHeight = 0.0;

    // Character rotation (0-3)
    unsigned rotation = 0;

	// Character scale (0-15, 0 is smallest)
	unsigned charScale = 0;

	// Configuration settings (persistent)
	std::shared_ptr<EnumSetting<PlotterCharacterSet>> charSetSetting;
	std::shared_ptr<BooleanSetting> dipSwitch4Setting;
	std::shared_ptr<BooleanSetting> kanjiSupportSetting;

	// Graphic mode command buffer
	std::string graphicCmdBuffer;

	// Pen colors: RGB tuples for each pen (0=black, 1=blue, 2=green, 3=red)
	static constexpr std::array<std::array<uint8_t, 3>, 4> penColors = {{
		{{0, 0, 0}},       // black
		{{0, 0, 255}},     // blue
		{{0, 255, 0}},     // green
		{{255, 0, 0}}      // red
	}};

	// A4 Paper: 210mm x 296mm => 1050 x 1480 steps.
	// X-axis is the short axis (carriage), Y-axis is the long axis (paper feed).
	static constexpr unsigned PAPER_WIDTH_STEPS = 1050;   // 210mm
	static constexpr unsigned PAPER_HEIGHT_STEPS = 1480;  // 296mm

	// A4 Plotting Area: 192mm x 276.8mm => 960 x 1384 steps.
	static constexpr unsigned PLOT_AREA_WIDTH = 960;
	static constexpr unsigned PLOT_AREA_HEIGHT = 1384;

	// Center the plotting area on the paper.
	static constexpr unsigned MARGIN_X = (PAPER_WIDTH_STEPS - PLOT_AREA_WIDTH) / 2; // 45 steps (9mm)
	static constexpr unsigned MARGIN_Y = (PAPER_HEIGHT_STEPS - PLOT_AREA_HEIGHT) / 2; // 48 steps (9.6mm)

	// Private methods
	void processTextMode(uint8_t data);
	void processGraphicMode(uint8_t data);
	void executeGraphicCommand();
	void plotWithPen(double x, double y, double distMoved);
	void moveTo(double x, double y);
	void lineTo(double x, double y);
	void drawLine(double x0, double y0, double x1, double y1);
	void drawCharacter(uint8_t c, bool hasNextChar = false);
};

} // namespace openmsx

#endif
