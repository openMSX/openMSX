#ifndef PLOTTER_HH
#define PLOTTER_HH

#include "BooleanSetting.hh"
#include "EnumSetting.hh"
#include "Printer.hh"
#include "gl_vec.hh"
#include <cstdint> // for uint8_t
#include <memory>
#include <string>

namespace openmsx {

// Character set options for the plotter
enum class PlotterCharacterSet { International, Japanese, DIN };
enum class PlotterPenThickness { Standard, Thick };

// MSXPlotter: emulates Sony PRN-C41 color plotter with 4 pens
class MSXPlotter final : public ImagePrinter {
public:
  explicit MSXPlotter(MSXMotherBoard &motherBoard);

  // Pluggable
  [[nodiscard]] zstring_view getName() const override;
  [[nodiscard]] zstring_view getDescription() const override;

  // Configuration settings (accessible by ImGui)
  [[nodiscard]] PlotterCharacterSet getCharacterSet() const;
  void setCharacterSet(PlotterCharacterSet cs);
  [[nodiscard]] bool getDipSwitch4() const;
  void setDipSwitch4(bool enabled);

  // Manual controls (for ImGui)
  void cyclePen();
  void moveStep(gl::vec2 delta);
  void ejectPaper();

  // Access to settings for persistence
  [[nodiscard]] EnumSetting<PlotterCharacterSet> &
  getCharacterSetSetting() const {
    return *charSetSetting;
  }
  [[nodiscard]] BooleanSetting &getDipSwitch4Setting() const {
    return *dipSwitch4Setting;
  }
  [[nodiscard]] EnumSetting<PlotterPenThickness> &
  getPenThicknessSetting() const {
    return *penThicknessSetting;
  }

  [[nodiscard]] gl::vec2 getPlotterPos() const { return plotter + origin; }
  [[nodiscard]] unsigned getSelectedPen() const { return selectedPen; }
  [[nodiscard]] Paper *getPaper() { return ImagePrinter::getPaper(); }
  [[nodiscard]] const Paper *getPaper() const {
    return ImagePrinter::getPaper();
  }

  // PrinterPortDevice
  [[nodiscard]] bool getStatus(EmuTime time) override;
  void setStrobe(bool strobe, EmuTime time) override;
  void writeData(uint8_t data, EmuTime time) override;

  // Force color output
  [[nodiscard]] bool useColor() const override { return true; }

  // A4 Landscape as specified in manual (296mm x 210mm)
  [[nodiscard]] std::pair<double, double> getPaperSize() const override {
    return {210.0, 296.0};
  }

  // Override processCharacter to handle plotter-specific commands
  void processCharacter(uint8_t data) override;

  // (Optional) serialization
  template <typename Archive> void serialize(Archive &, unsigned) {}

  // Implement required pure virtuals from ImagePrinter
  std::pair<unsigned, unsigned> getNumberOfDots() override;
  void resetSettings() override;
  unsigned calcEscSequenceLength(uint8_t character) override;
  void processEscSequence() override;
  void ensurePrintPage() override;

private:
  void processTextMode(uint8_t data);
  void processGraphicMode(uint8_t data);
  void executeGraphicCommand();
  void plotWithPen(gl::vec2 pos, float distMoved);
  void moveTo(gl::vec2 pos);
  void lineTo(gl::vec2 pos);
  void drawLine(gl::vec2 from, gl::vec2 to);
  void drawCharacter(uint8_t c, bool hasNextChar = false);

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
  bool penDown = true;      // pen starts down for drawing

  // Plotter head position (logical steps, relative to origin)
  gl::vec2 plotter{0.0f, 0.0f};

  // Origin offset (physical steps)
  gl::vec2 origin{0.0f, 0.0f};

  // Line style
  unsigned lineType = 0; // 0=solid, 1-14=dashed
  float dashDistance = 0.0f;
  float maxLineHeight = 0.0f;

  // Character rotation (0-3)
  unsigned rotation = 0;

  // Pending character gap (to be removed if Q command follows)
  float pendingCharGap = 0.0f;
  unsigned pendingGapRotation = 0;

  // Character scale (0-15, 0 is smallest)
  unsigned charScale = 0;

  // Configuration settings (persistent)
  std::shared_ptr<EnumSetting<PlotterCharacterSet>> charSetSetting;
  std::shared_ptr<BooleanSetting> dipSwitch4Setting;
  std::shared_ptr<EnumSetting<PlotterPenThickness>> penThicknessSetting;

  // Graphic mode command buffer
  std::string graphicCmdBuffer;

  // A4 Paper: 210mm x 296mm => 1050 x 1480 steps.
  // X-axis is the short axis (carriage), Y-axis is the long axis (paper feed).
  static constexpr unsigned PAPER_WIDTH_STEPS = 1050;  // 210mm
  static constexpr unsigned PAPER_HEIGHT_STEPS = 1480; // 296mm

  // A4 Plotting Area: 192mm x 276.8mm => 960 x 1384 steps.
  static constexpr unsigned PLOT_AREA_WIDTH = 960;
  static constexpr unsigned PLOT_AREA_HEIGHT = 1384;

  // Center the plotting area on the paper.
  static constexpr unsigned MARGIN_X =
      (PAPER_WIDTH_STEPS - PLOT_AREA_WIDTH) / 2; // 45 steps (9mm)
  static constexpr unsigned MARGIN_Y =
      (PAPER_HEIGHT_STEPS - PLOT_AREA_HEIGHT) / 2; // 48 steps (9.6mm)
};

} // namespace openmsx

#endif
