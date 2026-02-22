#ifndef PLOTTER_HH
#define PLOTTER_HH

#include "PlotterPaper.hh"
#include "Printer.hh" // TODO split off PrinterCore

#include "BooleanSetting.hh"
#include "EnumSetting.hh"

#include "gl_vec.hh"

#include <array>
#include <cstdint> // for uint8_t
#include <memory>
#include <string>

namespace openmsx {

// MSXPlotter: emulates Sony PRN-C41 color plotter with 4 pens
class MSXPlotter final : public PrinterCore
{
public:
	// Character set options for the plotter
	enum class CharacterSet { International, Japanese, DIN };
	enum class PenThickness { Standard, Thick };

public:
	explicit MSXPlotter(MSXMotherBoard& motherBoard);
	~MSXPlotter() override;

	// Pluggable
	[[nodiscard]] zstring_view getName() const override;
	[[nodiscard]] zstring_view getDescription() const override;

	// PrinterCore
	void write(uint8_t data) override;
	void forceFormFeed() override;

	[[nodiscard]] PlotterPaper* getPaper() { return paper.get(); }

	// Configuration settings (accessible by ImGui)
	[[nodiscard]] CharacterSet getCharacterSet() const;
	void setCharacterSet(CharacterSet cs);
	[[nodiscard]] bool getDipSwitch4() const;
	void setDipSwitch4(bool enabled);

	// Manual controls (for ImGui)
	void cyclePen();
	void moveStep(gl::vec2 delta);
	void ejectPaper();

	// Access to settings for persistence
	[[nodiscard]] EnumSetting<CharacterSet>& getCharacterSetSetting() const { return *charSetSetting; }
	[[nodiscard]] BooleanSetting& getDipSwitch4Setting() const { return *dipSwitch4Setting; }
	[[nodiscard]] EnumSetting<PenThickness>& getPenThicknessSetting() const { return *penThicknessSetting; }

	[[nodiscard]] gl::vec2 getPlotterPos() const { return penPosition + origin; }
	[[nodiscard]] unsigned getSelectedPen() const { return selectedPen; }
	[[nodiscard]] bool isGraphicMode() const { return mode == Mode::GRAPHIC; }

	// PrinterPortDevice
	[[nodiscard]] bool getStatus(EmuTime time) override;

	// Re-initialize paper on re-plug
	void plugHelper(Connector& connector, EmuTime time) override;

	// Implement required pure virtuals from ImagePrinter
	void resetSettings();
	unsigned calcEscSequenceLength(uint8_t character);
	void processEscSequence();
	void ensurePrintPage();
	void flushEmulatedPrinter();

	// (Optional) serialization
	template<typename Archive>
	void serialize(Archive&, unsigned)
	{
		// TODO
	}

private:
	void updateLineFeed();
	void processTextMode(uint8_t data);
	void processGraphicMode(uint8_t data);
	void executeGraphicCommand();
	void moveTo(gl::vec2 pos);
	void lineTo(gl::vec2 pos);
	void drawLine(gl::vec2 from, gl::vec2 to);
	void drawCharacter(uint8_t c, bool hasNextChar = false);
	void drawDot(gl::vec2 pos);
	void drawDashedLine(gl::vec2 A, gl::vec2 B, float halfPeriod);
	void drawSolidLine(gl::vec2 A, gl::vec2 B);
	[[nodiscard]] gl::vec2 toPaperPos(gl::vec2 plotterPos) const;
	[[nodiscard]] float penRadius() const;
	[[nodiscard]] gl::vec3 penColor() const;

private:
	MSXMotherBoard& motherBoard;
	std::shared_ptr<IntegerSetting> dpiSetting;
	std::unique_ptr<PlotterPaper> paper;

	float pixelSize;

	// Mode state
	enum class Mode { TEXT, GRAPHIC };
	Mode mode = Mode::TEXT;

	// ESC sequence parsing state
	enum class EscState { NONE, ESC, ESC_C, ESC_S, ESC_S_EXP_DIGIT };
	EscState escState = EscState::NONE;
	enum class TerminatorSkip { NONE, START, SEEN_CR };
	TerminatorSkip terminatorSkip = TerminatorSkip::NONE;
	bool printNext = false; // For 0x01 literal prefix

	// Pen/color state
	uint8_t selectedPen = 0; // 0=black, 1=blue, 2=green, 3=red
	bool penDown = true; // pen starts down for drawing   TODO this isn't used ??

	// Plotter head position (logical steps, relative to origin)
	gl::vec2 penPosition{0.0f, 0.0f};

	// Origin offset (physical steps)
	gl::vec2 origin{0.0f, 0.0f};

	// Line style
	uint8_t lineType = 0; // 0=solid, 1-14=dashed
	float dashDistance  = 0.0f;
	float maxLineHeight = 0.0f;
	float lineFeed      = 18.0f;

	// Character rotation (0-3)
	unsigned rotation = 0;

	// Pending character gap (to be removed if Q command follows)
	float pendingCharGap        = 0.0f;
	unsigned pendingGapRotation = 0;

	// Character scale (0-15, 0 is smallest)
	unsigned charScale         = 0;
	unsigned pendingScaleDigit = 0;

	// Configuration settings (persistent)
	std::shared_ptr<EnumSetting<CharacterSet>> charSetSetting;
	std::shared_ptr<BooleanSetting> dipSwitch4Setting;
	std::shared_ptr<EnumSetting<PenThickness>> penThicknessSetting;

	// Graphic mode command buffer
	std::string graphicCmdBuffer;

public:
	// X-axis is the short axis (carriage), Y-axis is the long axis (paper feed).
	static constexpr auto A4_SIZE = gl::vec2{210.0f, 297.0f}; // portrait A4 in mm
	static constexpr gl::vec2 FULL_AREA = A4_SIZE * 5; // 5 steps/mm  ->  1050 x 1485 steps

	// A4 Plotting Area: 192mm x 276.8mm => 960 x 1384 steps.
	static constexpr gl::vec2 PLOT_AREA = {960.0f, 1384.0f};
	static constexpr auto RIGHT_BORDER = PLOT_AREA.x;

	static constexpr gl::vec2 MARGIN = (FULL_AREA - PLOT_AREA) * 0.5f;

	// Pen colors: 0=no ink, 1=full ink for subtractive model
	static constexpr std::array<gl::vec3, 4> inkColors = {{
		{0.80f, 0.80f, 0.80f}, // black
		{0.80f, 0.80f, 0.00f}, // blue
		{0.65f, 0.32f, 0.52f}, // green
		{0.00f, 0.80f, 0.80f}, // red
	}};
};

} // namespace openmsx

#endif
