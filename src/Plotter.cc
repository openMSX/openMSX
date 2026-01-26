#include "Plotter.hh"
#include "MSXCharacterSets.hh"
#include "MSXCliComm.hh"
#include "MSXMotherBoard.hh"
#include "Paper.hh"
#include "Printer.hh"
#include "gl_mat.hh"
#include "gl_vec.hh"
#include "ranges.hh"
#include "serialize.hh"
#include "strCat.hh"

#include <algorithm>
#include <array> // Added for std::array
#include <charconv>
#include <cmath>
#include <iostream>
#include <span>

namespace openmsx {

// Enable debug output by setting to 0
#if 1
template <typename... Args> static void printDebug(Args &&...) {
  // nothing
}
#else
template <typename... Args> static void printDebug(Args &&...args) {
  std::cerr << strCat(std::forward<Args>(args)...) << '\n';
}
#endif

MSXPlotter::MSXPlotter(MSXMotherBoard &motherBoard)
    : ImagePrinter(motherBoard, false) // graphicsHiLo=false (not used)
      ,
      charSetSetting(
          motherBoard.getSharedStuff<EnumSetting<PlotterCharacterSet>>(
              "plotter-charset", motherBoard.getCommandController(),
              "plotter-charset", "character set for the MSX plotter",
              PlotterCharacterSet::International,
              EnumSetting<PlotterCharacterSet>::Map{
                  {"international", PlotterCharacterSet::International},
                  {"japanese", PlotterCharacterSet::Japanese},
                  {"din", PlotterCharacterSet::DIN}})),
      dipSwitch4Setting(motherBoard.getSharedStuff<BooleanSetting>(
          "plotter-dipswitch4", motherBoard.getCommandController(),
          "plotter-dipswitch4", "dipswitch 4 setting for the MSX plotter",
          false)),
      penThicknessSetting(
          motherBoard.getSharedStuff<EnumSetting<PlotterPenThickness>>(
              "plotter-pen-thickness", motherBoard.getCommandController(),
              "plotter-pen-thickness", "pen thickness for the MSX plotter",
              PlotterPenThickness::Standard,
              EnumSetting<PlotterPenThickness>::Map{
                  {"standard", PlotterPenThickness::Standard},
                  {"thick", PlotterPenThickness::Thick}})) {
  // Initialize default font (8x8) if not loaded
  // This prevents crashes in printVisibleCharacter if rom is empty
  if (fontInfo.charWidth == 0) {
    fontInfo.charWidth = 8;
    // FontInfo struct does not have charHeight, and rom is std::array

    // Populate with a very simple pattern to distinguish chars?
    // Let's make it stripes: 0x55
    std::ranges::fill(fontInfo.rom, 0x55);

    // Proper fix: We should ideally load "printer.rom".
    // But for this patch, a dummy prevents the crash.
  }

  resetSettings();
  ensurePrintPage();
}

zstring_view MSXPlotter::getName() const { return "MSXPlotter"; }

zstring_view MSXPlotter::getDescription() const {
  return "Sony PRN-C41 Color Plotter (4 pens)";
}

bool MSXPlotter::getStatus(EmuTime /*time*/) { return false; }

void MSXPlotter::setStrobe(bool strobe, EmuTime time) {
  ImagePrinter::setStrobe(strobe, time);
}

void MSXPlotter::writeData(uint8_t data, EmuTime time) {
  ImagePrinter::writeData(data, time);
}

PlotterCharacterSet MSXPlotter::getCharacterSet() const {
  return charSetSetting->getEnum();
}

void MSXPlotter::setCharacterSet(PlotterCharacterSet cs) {
  charSetSetting->setEnum(cs);
}

bool MSXPlotter::getDipSwitch4() const {
  return dipSwitch4Setting->getBoolean();
}

void MSXPlotter::setDipSwitch4(bool enabled) {
  dipSwitch4Setting->setBoolean(enabled);
}

void MSXPlotter::cyclePen() { selectedPen = (selectedPen + 1) % 4; }

void MSXPlotter::moveStep(gl::vec2 delta) {
  gl::vec2 oldAbsPos = plotter + origin;
  gl::vec2 newAbsPos =
      gl::clamp(oldAbsPos + delta, gl::vec2{0.0f},
                gl::vec2{float(PLOT_AREA_WIDTH), float(PLOT_AREA_HEIGHT)});
  plotter = newAbsPos - origin;
}

void MSXPlotter::ejectPaper() {
  ensurePrintPage();
  flushEmulatedPrinter();
  resetSettings(); // Resets pen pos and other states
  ensurePrintPage();
}

void MSXPlotter::processCharacter(uint8_t data) {
  // Debug: log every byte received
  printDebug("Plotter: received 0x", hex_string<2>(data),
             " mode=", (mode == Mode::TEXT ? "TEXT" : "GRAPHIC"),
             " escState=", int(escState));

  // Handle ESC sequence state first
  switch (escState) {
  case EscState::ESC:
    // Got ESC, waiting for next char
    if (data == '#') {
      // ESC # = enter graphic mode
      printDebug("Plotter: entering GRAPHIC mode");
      mode = Mode::GRAPHIC;
      graphicCmdBuffer.clear();
      escState = EscState::NONE;
      return;
    } else if (data == '$') {
      // ESC $ = return to text mode, flush any pending graphics
      printDebug("Plotter: returning to TEXT mode, flushing");
      flushEmulatedPrinter();
      mode = Mode::TEXT;
      escState = EscState::NONE;
      return;
    } else if (data == 'C') {
      // ESC C = color set, wait for color digit
      escState = EscState::ESC_C;
      return;
    } else {
      // Unknown ESC sequence, ignore
      motherBoard.getMSXCliComm().printWarning(
          strCat("Plotter: unknown ESC sequence 0x", hex_string<2>(data)));
      escState = EscState::NONE;
      return;
    }
  case EscState::ESC_C:
    // Got ESC C, waiting for color digit '0'-'3'
    if (data == ' ')
      return; // Ignore spaces
    if (data >= '0' && data <= '3') {
      selectedPen = data - '0';
      printDebug("Plotter: selected pen ", selectedPen);
    }
    escState = EscState::NONE;
    return;
  case EscState::NONE:
    break;
  }

  // Route to appropriate mode handler
  if (mode == Mode::TEXT) {
    processTextMode(data);
  } else {
    processGraphicMode(data);
  }
}

void MSXPlotter::processTextMode(uint8_t data) {
  if (printNext) {
    // CHR$(1) prefix: print alternate character (0-31) by subtracting 64
    // e.g., CHR$(1)+CHR$(65) prints character 1 (65-64=1)
    drawCharacter(static_cast<uint8_t>(data - 64));
    printNext = false;
    return;
  }

  switch (data) {
  case 0x01: // Header / Literal prefix
    printNext = true;
    break;
  case 0x08: // BS - Backspace
    plotter.x = std::max(plotter.x - (8 + 2 * charScale), 0.0f);
    break;
  case 0x0a: // LF - Line feed
    plotter.y -= float(lineFeed);
    if (plotter.y < -1354.0f) { // Page end reached
      ensurePrintPage();
      flushEmulatedPrinter();
      plotter.y = 30.0f; // Reset to top of new page
    }
    break;
  case 0x0b: // VT - Line up (feed paper to previous line)
    plotter.y += float(lineFeed);
    if (plotter.y > 30.0f) {
      plotter.y = 30.0f;
    }
    break;
  case 0x0C: // FF - Form feed (top of form)
    ensurePrintPage();
    flushEmulatedPrinter();
    plotter.y = 30.0f;
    plotter.x = 0.0f;
    break;
  case 0x0d: // CR - Carriage return
    plotter.x = 0.0f;
    break;
  case 0x12: // DC2 - Scale set prefix (wait for next char)
             // ... handling remains same (ignored)
    break;
  case 0x1b: // ESC - Start escape sequence
    escState = EscState::ESC;
    break;
  default:
    // Printable character in text mode (Allow all >= 32)
    if (data >= 32) {
      // Check for line wrap before drawing
      // Estimate width: 6 steps base * scaleFactor
      // Or simply check current position
      if (plotter.x >= float(rightBorder)) {
        // Auto CR/LF
        plotter.x = 0.0f;
        plotter.y -= float(lineFeed);
        if (plotter.y < -1354.0f) {
          ensurePrintPage();
          flushEmulatedPrinter();
          plotter.y = 30.0f;
        }
      }
      drawCharacter(data);
    }
    break;
  }
}

void MSXPlotter::processGraphicMode(uint8_t data) {
  // In graphic mode, accumulate command chars until delimiter
  // Commands are ASCII text like "J100,-100,50,-100" terminated by CR/LF or
  // next command

  if (data == one_of(0x0d, 0x0a)) {
    // CR or LF terminates command
    executeGraphicCommand();
  } else if (data == 0x1b) {
    // ESC in graphic mode - start escape sequence
    executeGraphicCommand();
    escState = EscState::ESC;
  } else if (data >= 32 && data < 127) {
    // Accumulate command characters
    graphicCmdBuffer += static_cast<char>(data);

    // Execute immediately if buffer starts with known command and we hit a
    // letter (next command) Commands start with a letter, followed by numbers
    // and commas
    if (graphicCmdBuffer.size() > 1) {
      // Special handling for P command which takes arbitrary text
      bool isPrintCmd = (graphicCmdBuffer[0] == 'P');

      char lastChar = graphicCmdBuffer.back();
      if (!isPrintCmd && std::isalpha(static_cast<unsigned char>(lastChar)) &&
          std::toupper(lastChar) != 'E' && lastChar != graphicCmdBuffer[0]) {
        // New command starting, execute previous
        char nextCmd = graphicCmdBuffer.back();
        graphicCmdBuffer.pop_back();
        executeGraphicCommand();
        graphicCmdBuffer = nextCmd;
      }
    }
  }
}

void MSXPlotter::executeGraphicCommand() {
  if (graphicCmdBuffer.empty())
    return;

  // Debug: log the command being executed
  printDebug("Plotter: executing graphic command '", graphicCmdBuffer, "'");

  char cmd = graphicCmdBuffer[0];
  std::string args = graphicCmdBuffer.substr(1);

  // Parse comma-separated numbers (integers or floats)
  std::vector<float> coords;
  size_t pos = 0;
  while (pos < args.size()) {
    // Skip whitespace and commas
    while (pos < args.size() && (args[pos] == ',' || args[pos] == ' ')) {
      ++pos;
    }
    if (pos >= args.size())
      break;

    // Try to parse a number
    size_t nextPos;
    try {
      // Using stod is easiest for double parsing, but we need to check if it
      // actually parses std::stod throws if no conversion. Manually check if
      // valid start of number? Valid: digit, +, -, or .
      char c = args[pos];
      if (std::isdigit(static_cast<unsigned char>(c)) ||
          c == one_of('-', '+', '.')) {
        // Check if it's just a lonely '+' or '-' or '.' which stof might handle
        // or reject Standard library stof parses.
        float val = std::stof(args.substr(pos), &nextPos);
        coords.push_back(val);
        pos += nextPos;
      } else {
        // Not a number, skip this character (it might be part of P command
        // text) For P command, we ideally shouldn't be parsing coords here, but
        // we do it generic. Just skip.
        ++pos;
      }
    } catch (...) {
      // parsing failed, skip char
      ++pos;
    }
  }

  switch (cmd) {
  case 'H': // Home - move to origin
    printDebug("Plotter: H - Home");
    plotter = gl::vec2{0.0f};
    break;

  case 'M': // Move (absolute) - M x,y
    if (coords.size() >= 2) {
      printDebug("Plotter: M - Move to (", coords[0], ",", coords[1], ")");
      moveTo({coords[0], coords[1]});
    }
    break;

  case 'R': // Relative move - R dx,dy
    if (coords.size() >= 2) {
      printDebug("Plotter: R - Relative move (", coords[0], ",", coords[1],
                 ")");
      moveTo(plotter + gl::vec2{coords[0], coords[1]});
    }
    break;

  case 'D': // Draw (absolute) - D x,y
    if (coords.size() >= 2) {
      gl::vec2 target{coords[0], coords[1]};
      printDebug("Plotter: D - Draw to (", target.x, ",", target.y, ") from (",
                 plotter.x, ",", plotter.y, ") penDown=", penDown);
      lineTo(target);
    }
    break;

  case 'J': // Draw relative - J dx,dy[,dx,dy,...]
    // The J command draws a series of relative line segments
    printDebug("Plotter: J - Draw relative, ", coords.size() / 2,
               " segments, penDown=", penDown);
    for (size_t i = 0; i + 1 < coords.size(); i += 2) {
      lineTo(plotter + gl::vec2{coords[i], coords[i + 1]});
    }
    break;

  case 'S': // Scale set - S n (0-15)
    if (!coords.empty()) {
      charScale = std::clamp(static_cast<int>(coords[0]), 0, 15);
      printDebug("Plotter: S - Scale set to ", charScale);
    }
    break;

  case 'L': // Line type - L n (0-15)
    if (!coords.empty()) {
      lineType = std::clamp(static_cast<int>(coords[0]), 0, 15);
      dashDistance = 0.0f; // Reset pattern phase
      printDebug("Plotter: L - Line type set to ", lineType);
    }
    break;

  case 'Q': // Alpha rotate - Q n (0-3)
    if (!coords.empty()) {
      // If a character was just printed, undo the gap
      // (the last char before Q should not have the trailing gap)
      if (pendingCharGap > 0.0f) {
        switch (pendingGapRotation) {
        case 0:
          plotter.x -= pendingCharGap;
          break;
        case 1:
          plotter.y += pendingCharGap;
          break;
        case 2:
          plotter.x += pendingCharGap;
          break;
        case 3:
          plotter.y -= pendingCharGap;
          break;
        }

        printDebug("Plotter: Q - Removed pending gap ", pendingCharGap);
        pendingCharGap = 0.0f;
      }
      rotation = std::clamp(static_cast<int>(coords[0]), 0, 3);
      printDebug("Plotter: Q - Rotation set to ", rotation);
    }
    break;

  case 'I': // Initialize
    // Sets current pen position as origin
    origin += plotter;
    plotter = gl::vec2{0.0f};
    printDebug("Plotter: I - Origin set to current pos. New Origin=(", origin.x,
               ",", origin.y, ")");
    break;

  case 'A': // All initialize

    printDebug("Plotter: A - All Initialize (Reset)");
    resetSettings();
    break;

  case 'F': // New line
    // Move to top of next line using max height encountered.
    // If empty line, default to standard line feed (12 steps + scale effect?).
    // If maxLineHeight is set, use it.
    {
      // Default drop if no characters printed: Height + Gap
      // Height = 6 * (1+S), Gap = 2 * (1+S) => Total 8 * (1+S)
      // For Scale 1: 8 * 2 = 16 steps.
      float drop = (maxLineHeight > 0.0f) ? maxLineHeight
                                          : 6.0f * (1.0f + float(charScale));

      // Add inter-line spacing (Gap)
      drop += 2.0f * (1.0f + float(charScale));

      // Apply Line Feed and Carriage Return based on rotation
      switch (rotation) {
      case 0: // Normal: Feed Y- (Down), CR X=0
        plotter.y -= drop;
        if (plotter.y < -1354.0f) { // Page End check
          ensurePrintPage();
          flushEmulatedPrinter();
          plotter.y = 30.0f;
        }
        plotter.x = 0.0f;
        break;
      case 1: // 90 CW (Down): Feed X- (Left), CR Y=0
        plotter.x -= drop;
        // Check X < 0 ? (Left margin)
        plotter.y = 0.0f;
        break;
      case 2: // 180 (Left): Feed Y+ (Up), CR X=0
        plotter.y += drop;
        plotter.x = 0.0f;
        break;
      case 3: // 270 CW (Up): Feed X+ (Right), CR Y=0
        plotter.x += drop;
        plotter.y = 0.0f;
        break;
      }

      maxLineHeight = 0.0f;
      printDebug("Plotter: F - New Line, drop=", drop, " rot=", rotation);
    }
    break;

  case 'P': // Print - P chrs
    if (!args.empty()) {
      // Characters after 'P' are printed literally, including spaces.
      // CHR$(1) prefix is used to print alternate characters (0-31).
      std::string_view text = args;
      printDebug("Plotter: P - Printing '", text, "'");

      bool altChar = false;
      size_t i = 0;
      while (i < text.size()) {
        char c = text[i];
        if (static_cast<uint8_t>(c) == 0x01) {
          altChar = true;
          ++i;
        } else if (altChar) {
          // CHR$(1)+CHR$(N) prints character N-64
          bool hasNext = (i + 1 < text.size());
          drawCharacter(static_cast<uint8_t>(c) - 64, hasNext);
          altChar = false;
          ++i;
        } else {
          bool hasNext = (i + 1 < text.size());
          drawCharacter(static_cast<uint8_t>(c), hasNext);
          ++i;
        }
      }
    }
    break;

  case 'C': // Color select - C n (0-3)
    if (!coords.empty() && coords[0] >= 0 && coords[0] <= 3) {
      unsigned newPen = static_cast<unsigned>(coords[0]);
      if (newPen != selectedPen) {
        printDebug("Plotter: C - Select color ", newPen,
                   " (Pen change delay applied)");
        selectedPen = newPen;
      }
    }
    break;

  default:
    // Unknown command, ignore
    motherBoard.getMSXCliComm().printWarning(
        strCat("Plotter: unknown graphic command '", cmd, "'"));
    break;
  }

  if (cmd != 'Q' && cmd != 'P') {
    pendingCharGap = 0.0f;
  }
  graphicCmdBuffer.clear();
}

void MSXPlotter::drawLine(gl::vec2 from, gl::vec2 to) {
  gl::vec2 delta = to - from;
  float totalDist = gl::length(delta);
  if (!penDown)
    return;
  ensurePrintPage();

  if (totalDist == 0.0f) {
    plotWithPen(from, 0.0f);
    return;
  }

  // High density sampling: e.g. 4 samples per plotter step (0.25 steps per dot)
  // This makes lines look solid rather than a series of dots.
  float stepSize = 0.25f;
  int steps = static_cast<int>(std::ceil(totalDist / stepSize));
  gl::vec2 inc = delta / float(steps);
  float dDist = totalDist / float(steps);

  gl::vec2 p = from;
  for (int i = 0; i <= steps; ++i) {
    plotWithPen(p, (i == 0) ? 0.0f : dDist);
    p += inc;
  }
}

void MSXPlotter::plotWithPen(gl::vec2 pos, float distMoved) {
  ensurePrintPage();
  auto *p = getPaper();
  if (!p) {
    motherBoard.getMSXCliComm().printWarning(
        "Plotter: plotWithPen called but NO PAPER");
    return;
  }

  const auto &color = penColors[selectedPen];
  // Convert logical plotter steps to physical paper steps with margins.
  // Origin (0,0) is bottom-left of the plotting area.
  // X-axis: MarginX + LogicalX
  // Y-axis: (PaperHeight - MarginY) - LogicalY (inverted for top-down PNG)
  gl::vec2 paperPos =
      gl::vec2{float(MARGIN_X), float(PAPER_HEIGHT_STEPS - MARGIN_Y)} +
      gl::vec2{1.0f, -1.0f} * (pos + origin);

  gl::vec2 pixelPos = paperPos * gl::vec2{float(pixelSizeX), float(pixelSizeY)};

  // Update print area bounds so flushEmulatedPrinter knows to save the file
  printAreaTop = std::min(printAreaTop, double(pixelPos.y));
  printAreaBottom =
      std::max(printAreaBottom, double(pixelPos.y + float(pixelSizeY)));

  // Handle Line Type (Dashing)
  // 0 and 15 are solid. 1-14 are broken lines.
  bool draw = true;
  if (lineType > 0 && lineType < 15) {
    // HP-GL/2 style or similar: start with a mark.
    // halfPeriod is defined by lineType.
    unsigned halfPeriod = lineType + 2;
    unsigned phase = static_cast<unsigned>(dashDistance / float(halfPeriod));
    if (phase % 2 == 0) { // Skip if 0 (even), Mark if 1 (odd)
      draw = false;
    }
  }
  dashDistance += distMoved;

  if (draw) {
    p->plotColor(double(pixelPos.x), double(pixelPos.y), color[0], color[1],
                 color[2]);
  }
}

void MSXPlotter::ensurePrintPage() {
  bool alreadyHadPaper = (getPaper() != nullptr);
  ImagePrinter::ensurePrintPage();
  if (!alreadyHadPaper) {
    // Sony PRN-C41 uses pens that are likely ~0.4-0.5mm wide.
    // Standard step is 0.2mm. Set dot size to 200% of step size for solid
    // lines.
    if (auto *p = getPaper()) {
      float sizeMultiplier =
          (getPenThicknessSetting().getEnum() == PlotterPenThickness::Thick)
              ? 1.5f
              : 1.0f;
      p->setDotSize(double(float(pixelSizeX) * sizeMultiplier),
                    double(float(pixelSizeY) * sizeMultiplier));
    }
  }
}

std::pair<unsigned, unsigned> MSXPlotter::getNumberOfDots() {
  // Return total steps that fit on the specified paper size.
  return {PAPER_WIDTH_STEPS, PAPER_HEIGHT_STEPS};
}

void MSXPlotter::resetSettings() {
  mode = Mode::TEXT;
  escState = EscState::NONE;
  selectedPen = 0;
  penDown = true;
  plotter = gl::vec2{0.0f, 30.0f};
  origin = gl::vec2{0.0f, 1354.0f};
  lineType = 0;
  dashDistance = 0.0f;
  rotation = 0;
  pendingCharGap = 0.0f;
  pendingGapRotation = 0;
  charScale = 1; // Default scale is 1
  maxLineHeight = 0.0f;
  graphicCmdBuffer.clear();
  printNext = false;

  // Set up printer settings for plotter
  lineFeed = 18.0; // Proportional to new character height
  leftBorder = 0;
  rightBorder = PLOT_AREA_WIDTH;
  graphDensity = 1.0;
  fontDensity = 1.0;
  pageTop = 0.0;
  lines = PLOT_AREA_HEIGHT / 18;
}

unsigned MSXPlotter::calcEscSequenceLength(uint8_t /*character*/) {
  // ESC sequences handled in processCharacter state machine
  return 0;
}

void MSXPlotter::processEscSequence() {
  // ESC sequences handled in processCharacter state machine
}

void MSXPlotter::moveTo(gl::vec2 pos) { plotter = pos; }

void MSXPlotter::lineTo(gl::vec2 pos) {
  // lineTo calls drawLine, which now handles addMoveDelay
  drawLine(plotter, pos);
  plotter = pos;
}

void MSXPlotter::drawCharacter(uint8_t c, bool /*hasNextChar*/) {
  auto savedLineType = lineType;
  auto savedDashDistance = dashDistance;
  lineType = 0;

  // auto font = getMSXFontRaw();
  //  Select font based on character set setting
  auto font = [&] {
    switch (getCharacterSet()) {
      using enum PlotterCharacterSet;
    default:
    case International:
      return getMSXFontRaw();
    case Japanese:
      return getMSXJPFontRaw();
    case DIN:
      return getMSXDINFontRaw();
    }
  }();
  auto glyph = subspan<8>(font, 8 * c);

  float scaleFactor = 1.0f + float(charScale);
  float gridSpacingX = 0.94f * scaleFactor;
  float gridSpacingY = 0.85f * scaleFactor;
  // float gridSpacingX = 0.54f * scaleFactor;
  // float gridSpacingY = 0.95f * scaleFactor;

  maxLineHeight = std::max(maxLineHeight, 8.0f * gridSpacingY);

  // Transform local (u, v) [0..width, 0..height] to global (px, py)
  // Cursor is at the START of the character baseline.
  // u=0 is at cursor, character extends in reading direction (+u).
  // v=0 is at baseline, character extends upward (+v).
  auto transform = [&](gl::vec2 p) -> gl::vec2 {
    // Each rotation matrix has columns [uDir, vDir] for that rotation
    static constexpr std::array<gl::mat2, 4> rotations = {{
        gl::mat2{gl::vec2{1.0f, 0.0f}, gl::vec2{0.0f, 1.0f}},   // 0°: identity
        gl::mat2{gl::vec2{0.0f, -1.0f}, gl::vec2{1.0f, 0.0f}},  // 90° CW
        gl::mat2{gl::vec2{-1.0f, 0.0f}, gl::vec2{0.0f, -1.0f}}, // 180°
        gl::mat2{gl::vec2{0.0f, 1.0f}, gl::vec2{-1.0f, 0.0f}}   // 270° CW
    }};
    return plotter + rotations[rotation] * p;
  };

  for (int dy = 0; dy < 8; ++dy) {
    uint8_t rowPattern = glyph[dy];
    // dy=0 is top row, dy=7 is bottom row.
    // v should be height from bottom (v=0 at dy=7).
    float v = (7.0f - float(dy)) * gridSpacingY;

    for (int dx = 0; dx < 8; ++dx) {
      // dx=0 is left, dx=7 is right.
      float u = float(dx) * gridSpacingX;

      // Check if dot is present
      if (rowPattern & (0x80 >> dx)) {
        gl::vec2 p = transform({u, v});

        // Check Right neighbor (dx+1, dy) -> Same v, u + gridX
        if (dx < 7) {
          if (rowPattern & (0x80 >> (dx + 1))) {
            drawLine(p, transform({u + gridSpacingX, v}));
          }
        }

        // Check Down neighbor (dx, dy+1) -> v - gridY, Same u
        // Note: dy+1 is "physically down" in the grid, so lower v.
        if (dy < 7) {
          uint8_t nextRow = glyph[dy + 1];
          if (nextRow & (0x80 >> dx)) {
            drawLine(p, transform({u, v - gridSpacingY}));
          }

          // Check Down-Right neighbor (dx+1, dy+1)
          if (dx < 7) {
            if (nextRow & (0x80 >> (dx + 1))) {
              // Only connect if NOT a filled 2x2 block
              bool hasRight = (rowPattern & (0x80 >> (dx + 1)));
              bool hasDown = (nextRow & (0x80 >> dx));

              if (!hasRight && !hasDown) {
                drawLine(p, transform({u + gridSpacingX, v - gridSpacingY}));
              }
            }
          }

          // Check Down-Left neighbor (dx-1, dy+1)
          if (dx > 0) {
            if (nextRow & (0x80 >> (dx - 1))) {
              bool hasLeft = (rowPattern & (0x80 >> (dx - 1)));
              bool hasDown = (nextRow & (0x80 >> dx));

              if (!hasLeft && !hasDown) {
                drawLine(p, transform({u - gridSpacingX, v - gridSpacingY}));
              }
            }
          }
        }

        plotWithPen(p, 0.0f);
      }
    }
  }
  // Advance cursor to next character position
  // Always use full width (char + gap). If Q rotation follows, it will undo the
  // gap.
  float charWidthOnly = 4.12f * gridSpacingX; // just the character
  float charGap =
      2.3f * gridSpacingX; // gap between characters (11.12 - 4.12 = 7.0)
  float charAdvance = charWidthOnly + charGap; // total advance

  plotter = transform({charAdvance, 0.0f});

  // Track the gap so Q command can undo it if it follows
  pendingCharGap = charGap;
  pendingGapRotation = rotation;

  printDebug("Plotter: Rotation ", rotation, " | charWidth ", charWidthOnly,
             " | charGap ", charGap, " | charAdvance ", charAdvance,
             " | plotter.x ", plotter.x, " | plotter.y ", plotter.y);

  lineType = savedLineType;
  dashDistance = savedDashDistance;
}

// Serialization and registration macros
INSTANTIATE_SERIALIZE_METHODS(MSXPlotter);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, MSXPlotter, "MSXPlotter");

} // namespace openmsx
