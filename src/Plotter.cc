#include "Plotter.hh"

#include "FileOperations.hh"
#include "IntegerSetting.hh"
#include "MSXCharacterSets.hh"
#include "MSXCliComm.hh"
#include "MSXMotherBoard.hh"
#include "PNG.hh"

#include "ScopedAssign.hh"
#include "gl_mat.hh"
#include "ranges.hh"
#include "serialize.hh"
#include "strCat.hh"

#include <algorithm>
#include <cmath>
#include <span>

namespace openmsx {

// Enable debug output by setting to 0
#if 1
template<typename... Args>
static void printDebug(Args&&...)
{
	// nothing
}
#else
template<typename... Args>
static void printDebug(Args&&... args)
{
	std::cerr << strCat(std::forward<Args>(args)...) << '\n';
}
#endif

MSXPlotter::MSXPlotter(MSXMotherBoard& motherBoard_)
	: motherBoard(motherBoard_)
	, dpiSetting(motherBoard.getSharedStuff<IntegerSetting>(
		"print-resolution",
		motherBoard.getCommandController(), "print-resolution",
		"resolution of the output image of emulated dot matrix printer in DPI",
		300, 72, 1200))
	, charSetSetting(motherBoard.getSharedStuff<EnumSetting<MSXPlotter::CharacterSet>>("plotter-charset",
		motherBoard.getCommandController(),
		"plotter-charset",
		"character set for the MSX plotter",
		MSXPlotter::CharacterSet::International,
		EnumSetting<MSXPlotter::CharacterSet>::Map{{"international", MSXPlotter::CharacterSet::International},
			{"japanese", MSXPlotter::CharacterSet::Japanese},
			{"din", MSXPlotter::CharacterSet::DIN}}))
	, dipSwitch4Setting(motherBoard.getSharedStuff<BooleanSetting>("plotter-dipswitch4",
		motherBoard.getCommandController(),
		"plotter-dipswitch4",
		"dipswitch 4 setting for the MSX plotter",
		false))
	, penThicknessSetting(motherBoard.getSharedStuff<EnumSetting<MSXPlotter::PenThickness>>("plotter-pen-thickness",
		motherBoard.getCommandController(),
		"plotter-pen-thickness",
		"pen thickness for the MSX plotter",
		MSXPlotter::PenThickness::Standard,
		EnumSetting<MSXPlotter::PenThickness>::Map{{"standard", MSXPlotter::PenThickness::Standard},
			{"thick", MSXPlotter::PenThickness::Thick}}))
{
	resetSettings();
	ensurePrintPage();
}

MSXPlotter::~MSXPlotter()
{
	flushEmulatedPrinter();
}

zstring_view MSXPlotter::getName() const
{
	return "MSXPlotter";
}

void MSXPlotter::plugHelper(Connector& connector_, EmuTime time)
{
	PrinterCore::plugHelper(connector_, time);
	resetSettings();
	ensurePrintPage();
}

zstring_view MSXPlotter::getDescription() const
{
	return "Sony PRN-C41 Color Plotter (4 pens)";
}

bool MSXPlotter::getStatus(EmuTime /*time*/)
{
	return false;
}

MSXPlotter::CharacterSet MSXPlotter::getCharacterSet() const
{
	return charSetSetting->getEnum();
}

void MSXPlotter::setCharacterSet(CharacterSet cs)
{
	charSetSetting->setEnum(cs);
}

bool MSXPlotter::getDipSwitch4() const
{
	return dipSwitch4Setting->getBoolean();
}

void MSXPlotter::setDipSwitch4(bool enabled)
{
	dipSwitch4Setting->setBoolean(enabled);
}

void MSXPlotter::cyclePen()
{
	selectedPen = (selectedPen + 1) % 4;
}

void MSXPlotter::moveStep(gl::vec2 delta)
{
	gl::vec2 oldAbsPos = penPosition + origin;
	gl::vec2 newAbsPos = gl::clamp(oldAbsPos + delta, gl::vec2{0.0f}, PLOT_AREA);
	penPosition = newAbsPos - origin;
}

void MSXPlotter::ejectPaper()
{
	ensurePrintPage();
	flushEmulatedPrinter();
	resetSettings(); // Resets pen pos and other states
	ensurePrintPage();
}

void MSXPlotter::write(uint8_t data)
{
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
			// ESC $ = return to text mode, execute any pending graphics
			printDebug("Plotter: returning to TEXT mode");
			if (!graphicCmdBuffer.empty()) {
				executeGraphicCommand();
				graphicCmdBuffer.clear();
			}
			mode	 = Mode::TEXT;
			escState = EscState::NONE;
			return;
		} else if (data == 'C') {
			// ESC C = color set, wait for color digit
			escState = EscState::ESC_C;
			return;
		} else {
			// Unknown ESC sequence, ignore
			motherBoard.getMSXCliComm().printWarning(
				"Plotter: unknown ESC sequence 0x", hex_string<2>(data));
			escState = EscState::NONE;
			return;
		}
	case EscState::ESC_C:
		// Got ESC C, waiting for color digit '0'-'3'
		if (data >= '0' && data <= '3') {
			selectedPen = data - '0';
			printDebug("Plotter: selected pen ", selectedPen);
			terminatorSkip = TerminatorSkip::START;
		}
		escState = EscState::NONE;
		return;
	case EscState::ESC_S:
		// Got DC2 (0x12)
		if (data <= 15) {
			// Raw byte 0-15
			charScale = data;
			updateLineFeed();
			printDebug("Plotter: text scale set to ", charScale);
			terminatorSkip = TerminatorSkip::START;
			escState = EscState::NONE;
		} else if (data >= '0' && data <= '9') {
			// ASCII digit
			unsigned val = data - '0';
			if (val == 0 || val >= 2) {
				// '0' or '2'-'9' -> single digit, final
				charScale = val;
				updateLineFeed();
				printDebug("Plotter: text scale set to ", charScale);
				terminatorSkip = TerminatorSkip::START;
				escState = EscState::NONE;
			} else {
				// '1' -> could be start of "0"-"15"
				pendingScaleDigit = val;
				escState = EscState::ESC_S_EXP_DIGIT;
			}
		} else {
			// Invalid scale parameter, ignore command but process char
			escState = EscState::NONE;
			processTextMode(data);
		}
		return;
	case EscState::ESC_S_EXP_DIGIT:
		// Got DC2 + '1', waiting for potential second digit
		if (data >= '0' && data <= '9') {
			unsigned val = pendingScaleDigit * 10 + (data - '0');
			if (val <= 15) {
				charScale = val;
				updateLineFeed();
				printDebug("Plotter: text scale set to ", charScale);
				terminatorSkip = TerminatorSkip::START;
				escState = EscState::NONE;
			} else {
				// > 15, valid first digit but invalid second.
				// e.g. "19". Interpret first digit as scale, second as text.
				// Or ignore? for now ignore
				escState = EscState::NONE;
				processTextMode(data);
			}
		} else {
			// Not a digit. e.g. "1A".
			// Execute the pending '1' as scale.
			charScale = pendingScaleDigit;
			updateLineFeed();
			printDebug("Plotter: text scale set to ", charScale);
			terminatorSkip =
				TerminatorSkip::START; // Should we skip terminators here too if the first digit was
				                       // valid? The command technically executed properly with the
				                       // single digit. But followed by 'A', user probably didn't
				                       // intend a terminator sequence immediately. However, strictly
				                       // speaking, if '1' sets scale, command is done.
			escState = EscState::NONE;
			processTextMode(data);
		}
		return;
	case EscState::NONE: break;
	}

	// Route to appropriate mode handler
	if (mode == Mode::TEXT) {
		processTextMode(data);
	} else {
		processGraphicMode(data);
	}
}

void MSXPlotter::forceFormFeed()
{
	flushEmulatedPrinter();
}

void MSXPlotter::processTextMode(uint8_t data)
{
	if (terminatorSkip != TerminatorSkip::NONE) {
		if (data == 0x0D) { // CR
			terminatorSkip = TerminatorSkip::SEEN_CR;
			return;
		} else if (data == 0x0A) { // LF
			terminatorSkip = TerminatorSkip::NONE;
			return;
		} else {
			terminatorSkip = TerminatorSkip::NONE;
			// continue to process data
		}
	}

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
		penPosition.x = std::max(penPosition.x - (8 + 2 * charScale), 0.0f);
		break;
	case 0x0a: // LF - Line feed
		penPosition.y -= float(lineFeed);
		if (penPosition.y < -1354.0f) { // Page end reached
			ensurePrintPage();
			flushEmulatedPrinter();
			ensurePrintPage();
			penPosition.y = 30.0f; // Reset to top of new page
		}
		break;
	case 0x0b: // VT - Line up (feed paper to previous line)
		penPosition.y += float(lineFeed);
		if (penPosition.y > 30.0f) {
			penPosition.y = 30.0f;
		}
		break;
	case 0x0C: // FF - Form feed (top of form)
		ensurePrintPage();
		flushEmulatedPrinter();
		ensurePrintPage();
		penPosition.y = 30.0f;
		penPosition.x = 0.0f;
		break;
	case 0x0d: // CR - Carriage return
		penPosition.x = 0.0f;
		break;
	case 0x12: // DC2 - Scale set prefix (wait for next char)
		escState = EscState::ESC_S;
		break;
	case 0x1b: // ESC - Start escape sequence for mode (text/graphics)
		escState = EscState::ESC;
		break;
	default:
		// Printable character in text mode (Allow all >= 32)
		if (data >= 32) {
			// Check for line wrap before drawing
			// Estimate width: 6 steps base * scaleFactor
			// Or simply check current position
			if (penPosition.x >= RIGHT_BORDER) {
				// Auto CR/LF
				penPosition.x = 0.0f;
				penPosition.y -= float(lineFeed);
				if (penPosition.y < -1354.0f) {
					ensurePrintPage();
					flushEmulatedPrinter();
					ensurePrintPage();
					penPosition.y = 30.0f;
				}
			}
			drawCharacter(data);
		}
		break;
	}
}

void MSXPlotter::processGraphicMode(uint8_t data)
{
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
			bool isAlpha  = (lastChar >= 'a' && lastChar <= 'z') || (lastChar >= 'A' && lastChar <= 'Z');
			if (!isPrintCmd && isAlpha && lastChar != one_of('E', 'e', graphicCmdBuffer.front())) {
				// New command starting, execute previous
				char nextCmd = graphicCmdBuffer.back();
				graphicCmdBuffer.pop_back();
				executeGraphicCommand();
				graphicCmdBuffer = nextCmd;
			}
		}
	}
}

void MSXPlotter::executeGraphicCommand()
{
	if (graphicCmdBuffer.empty()) return;

	// Debug: log the command being executed
	printDebug("Plotter: executing graphic command '", graphicCmdBuffer, "'");

	char cmd	 = graphicCmdBuffer[0];
	std::string args = graphicCmdBuffer.substr(1);

	// Parse comma-separated numbers (integers or floats)
	std::vector<float> coords;
	size_t pos = 0;
	while (pos < args.size()) {
		// Skip whitespace and commas
		while (pos < args.size() && (args[pos] == ',' || args[pos] == ' ')) {
			++pos;
		}
		if (pos >= args.size()) break;

		// Try to parse a number
		size_t nextPos;
		try {
			// Using stod is easiest for double parsing, but we need to check if it
			// actually parses std::stod throws if no conversion. Manually check if
			// valid start of number? Valid: digit, +, -, or .
			char c = args[pos];
			if ((c >= '0' && c <= '9') || c == one_of('-', '+', '.')) {
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
		penPosition = gl::vec2{0.0f};
		break;

	case 'M': // Move (absolute) - M x,y
		if (coords.size() >= 2) {
			printDebug("Plotter: M - Move to (", coords[0], ",", coords[1], ")");
			moveTo({coords[0], coords[1]});
		}
		break;

	case 'R': // Relative move - R dx,dy
		if (coords.size() >= 2) {
			printDebug("Plotter: R - Relative move (", coords[0], ",", coords[1], ")");
			moveTo(penPosition + gl::vec2{coords[0], coords[1]});
		}
		break;

	case 'D': // Draw (absolute) - D x,y[,x,y,...]
		// The D command draws a series of absolute line segments
		printDebug("Plotter: D - Draw absolute, ", coords.size() / 2, " segments, penDown=", penDown);
		for (size_t i = 0; i + 1 < coords.size(); i += 2) {
			gl::vec2 target{coords[i], coords[i + 1]};
			lineTo(target);
		}
		break;

	case 'J': // Draw relative - J dx,dy[,dx,dy,...]
		// The J command draws a series of relative line segments
		printDebug("Plotter: J - Draw relative, ", coords.size() / 2, " segments, penDown=", penDown);
		for (size_t i = 0; i + 1 < coords.size(); i += 2) {
			lineTo(penPosition + gl::vec2{coords[i], coords[i + 1]});
		}
		break;

	case 'S': // Scale set - S n (0-15)
		if (!coords.empty()) {
			charScale = std::clamp(static_cast<int>(coords[0]), 0, 15);
			updateLineFeed();
			printDebug("Plotter: S - Scale set to ", charScale);
		}
		break;

	case 'L': // Line type - L n (0-15)
		if (!coords.empty()) {
			lineType     = std::clamp(static_cast<int>(coords[0]), 0, 15);
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
				case 0: penPosition.x -= pendingCharGap; break;
				case 1: penPosition.y += pendingCharGap; break;
				case 2: penPosition.x += pendingCharGap; break;
				case 3: penPosition.y -= pendingCharGap; break;
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
		origin += penPosition;
		penPosition = gl::vec2{0.0f};
		printDebug("Plotter: I - Origin set to current pos. New Origin=(", origin.x, ",", origin.y, ")");
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
			float drop = (maxLineHeight > 0.0f) ? maxLineHeight : 6.0f * (1.0f + float(charScale));

			// Add inter-line spacing (Gap)
			drop += 2.0f * (1.0f + float(charScale));

			// Apply Line Feed and Carriage Return based on rotation
			switch (rotation) {
			case 0: // Normal: Feed Y- (Down), CR X=0
				penPosition.y -= drop;
				if (penPosition.y < -1354.0f) { // Page End check
					ensurePrintPage();
					flushEmulatedPrinter();
					ensurePrintPage();
					penPosition.y = 30.0f;
				}
				penPosition.x = 0.0f;
				break;
			case 1: // 90 CW (Down): Feed X- (Left), CR Y=0
				penPosition.x -= drop;
				// Check X < 0 ? (Left margin)
				penPosition.y = 0.0f;
				break;
			case 2: // 180 (Left): Feed Y+ (Up), CR X=0
				penPosition.y += drop;
				penPosition.x = 0.0f;
				break;
			case 3: // 270 CW (Up): Feed X+ (Right), CR Y=0
				penPosition.x += drop;
				penPosition.y = 0.0f;
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
			auto newPen = static_cast<unsigned>(coords[0]);
			if (newPen != selectedPen) {
				printDebug("Plotter: C - Select color ", newPen, " (Pen change delay applied)");
				selectedPen = newPen;
			}
		}
		break;

	default:
		// Unknown command, ignore
		motherBoard.getMSXCliComm().printWarning("Plotter: unknown graphic command '", cmd, "'");
		break;
	}

	if (cmd != 'Q' && cmd != 'P') {
		pendingCharGap = 0.0f;
	}
	graphicCmdBuffer.clear();
}

gl::vec2 MSXPlotter::toPaperPos(gl::vec2 plotterPos) const
{
	// Margin constants: center the plotting area on the paper.
	// MARGIN_X = (PAPER_WIDTH_STEPS - PLOT_AREA_WIDTH) / 2 = 45 steps (9mm)
	// MARGIN_Y = (PAPER_HEIGHT_STEPS - PLOT_AREA_HEIGHT) / 2 = 48 steps (9.6mm)
	// Margin constants: center the plotting area on the paper.
	// MARGIN_X = (PAPER_WIDTH_STEPS - PLOT_AREA_WIDTH) / 2 = 45 steps (9mm)
	// MARGIN_Y = (PAPER_HEIGHT_STEPS - PLOT_AREA_HEIGHT) / 2 = 48 steps (9.6mm)

	// Convert logical plotter steps to physical paper steps with margins.
	// Origin (0,0) is bottom-left of the plotting area.
	// X-axis: MarginX + LogicalX
	// Y-axis: (PaperHeight - MarginY) - LogicalY (inverted for top-down PNG)
	gl::vec2 paperPos = gl::vec2{MARGIN.x, FULL_AREA.y - MARGIN.y}
	                  + gl::vec2{1.0f, -1.0f} * (plotterPos + origin);
	return paperPos * pixelSize;
}

float MSXPlotter::penRadius() const
{
	// Sony PRN-C41 uses pens that are likely ~0.4-0.5mm wide.
	// Standard step is 0.2mm. Set dot size to 200% of step size for solid
	// lines.
	return pixelSize; // radius = 0.2mm  ->  line width = 0.4mm
}

gl::vec3 MSXPlotter::penColor() const
{
	return inkColors[selectedPen];
}

void MSXPlotter::drawDot(gl::vec2 pos)
{
	ensurePrintPage();
	assert(paper);
	paper->draw_dot(toPaperPos(pos), penRadius(), penColor());
}

void MSXPlotter::drawLine(gl::vec2 from, gl::vec2 to)
{
	if (!penDown) return; // TODO increase dashDistance?

	ensurePrintPage();
	// TODO handle begin/end point with draw_dot() ??
	if (0 < lineType && lineType < 15) {
		auto halfPeriod = float(lineType + 2);
		drawDashedLine(from, to, halfPeriod);
	} else {
		drawSolidLine(from, to);
		dashDistance += length(to - from); // needed ?
	}
}

void MSXPlotter::drawDashedLine(gl::vec2 A, gl::vec2 B, float halfPeriod)
{
	if (A == B) return;
	gl::vec2 AB = B - A;
	auto len = length(AB);

	auto dir = AB / len;
	auto period = 2 * halfPeriod;
	auto phase = std::fmod(dashDistance, period);
	dashDistance += len;

	// Peel first and align to next draw boundary
	float d = 0;
	if (phase < halfPeriod) {
		// start with draw
		auto step = std::min(halfPeriod - phase, len);
		drawSolidLine(A, A + dir * step);
		d = step + halfPeriod;
	} else {
		// start with gap
		d = period - phase;
	}

	// Draw full line+gap periods
	while (d + period <= len) {
		drawSolidLine(A + dir * d, A + dir * (d + halfPeriod));
		d += period;
	}

	// Handle partial tail segment
	if (d < len) {
		auto d2 = d + std::min(halfPeriod, len - d);
		drawSolidLine(A + dir * d, A + dir * d2);
	}
}

void MSXPlotter::drawSolidLine(gl::vec2 A, gl::vec2 B)
{
	assert(paper);
	paper->draw_motion(toPaperPos(A), toPaperPos(B), penRadius(), penColor());
}

void MSXPlotter::ensurePrintPage()
{
	if (paper) return;

	static constexpr auto MM_PER_INCH = 25.4f;
	auto paperSize = trunc(A4_SIZE / MM_PER_INCH * float(dpiSetting->getInt()));
	pixelSize = float(paperSize.x) / FULL_AREA.x; // pixels are round (x==y)
	paper = std::make_unique<PlotterPaper>(paperSize);
}

void MSXPlotter::flushEmulatedPrinter()
{
	if (!paper) return;

	if (!paper->empty()) {
		try {
			static constexpr std::string_view PRINT_DIR = "prints";
			static constexpr std::string_view PRINT_EXTENSION = ".png";
			auto filename = FileOperations::getNextNumberedFileName(PRINT_DIR, "page", PRINT_EXTENSION);

			auto rgb = paper->getRGB();
			auto size = rgb.size();
			small_buffer<const uint8_t*, 4096> rowPointers(std::views::transform(xrange(size.y),
				[&](int y) { return &rgb.getLine(y).data()->x; }));

			PNG::saveRGB(size.x, rowPointers, filename);

			motherBoard.getMSXCliComm().printInfo("Printed to ", filename);
		} catch (MSXException& e) {
			motherBoard.getMSXCliComm().printWarning("Failed to print: ", e.getMessage());
		}
	}
	paper.reset();
}

void MSXPlotter::resetSettings()
{
	mode = Mode::TEXT;
	escState = EscState::NONE;
	selectedPen = 0;
	penDown = true;
	penPosition = gl::vec2{0.0f, 30.0f};
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
	terminatorSkip = TerminatorSkip::NONE;

	// Set up printer settings for plotter
	updateLineFeed();
}

unsigned MSXPlotter::calcEscSequenceLength(uint8_t /*character*/)
{
	// ESC sequences handled in processCharacter state machine
	return 0;
}

void MSXPlotter::processEscSequence()
{
	// ESC sequences handled in processCharacter state machine
}

void MSXPlotter::moveTo(gl::vec2 pos)
{
	penPosition = pos;
}

void MSXPlotter::lineTo(gl::vec2 pos)
{
	// lineTo calls drawLine, which now handles addMoveDelay
	drawLine(penPosition, pos);
	penPosition = pos;
}

void MSXPlotter::drawCharacter(uint8_t c, bool /*hasNextChar*/)
{
	ScopedAssign savedLineType(lineType, 0);
	ScopedAssign savedDashDistance(dashDistance, dashDistance); // value doesn't matter, just restore at the end

	// Select font based on character set setting
	auto font = [&] {
		switch (getCharacterSet()) {
		using enum CharacterSet;
		default:
		case International: return getMSXFontRaw();
		case Japanese:      return getMSXJPFontRaw();
		case DIN:           return getMSXDINFontRaw();
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
			gl::mat2{gl::vec2{ 1.0f,  0.0f}, gl::vec2{ 0.0f,  1.0f}}, //   0°
			gl::mat2{gl::vec2{ 0.0f, -1.0f}, gl::vec2{ 1.0f,  0.0f}}, //  90° CW
			gl::mat2{gl::vec2{-1.0f,  0.0f}, gl::vec2{ 0.0f, -1.0f}}, // 180°
			gl::mat2{gl::vec2{ 0.0f,  1.0f}, gl::vec2{-1.0f,  0.0f}}, // 270° CW
		}};
		return penPosition + rotations[rotation] * p;
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
							bool hasDown  = (nextRow & (0x80 >> dx));

							if (!hasRight && !hasDown) {
								drawLine(p,
									transform(
										{u + gridSpacingX, v - gridSpacingY}));
							}
						}
					}

					// Check Down-Left neighbor (dx-1, dy+1)
					if (dx > 0) {
						if (nextRow & (0x80 >> (dx - 1))) {
							bool hasLeft = (rowPattern & (0x80 >> (dx - 1)));
							bool hasDown = (nextRow & (0x80 >> dx));

							if (!hasLeft && !hasDown) {
								drawLine(p,
									transform(
										{u - gridSpacingX, v - gridSpacingY}));
							}
						}
					}
				}

				drawDot(p);
			}
		}
	}
	// Advance cursor to next character position
	// Always use full width (char + gap). If Q rotation follows, it will undo the
	// gap.
	float charWidthOnly = 4.12f * gridSpacingX;  // just the character
	float charGap = 2.3f * gridSpacingX;         // gap between characters (11.12 - 4.12 = 7.0)
	float charAdvance = charWidthOnly + charGap; // total advance

	penPosition = transform({charAdvance, 0.0f});

	// Track the gap so Q command can undo it if it follows
	pendingCharGap = charGap;
	pendingGapRotation = rotation;

	printDebug("Plotter: Rotation ", rotation,
	           " | charWidth ", charWidthOnly,
	           " | charGap ", charGap,
	           " | charAdvance ", charAdvance,
	           " | plotter.x ", penPosition.x,
	           " | plotter.y ", penPosition.y);
}

void MSXPlotter::updateLineFeed()
{
	// Base line feed for scale 1 is 18 (9.0 * 2).
	// Formula: 9.0 * (1.0 + Scale) seems consistent.
	// Scale 0: 9 * 1 = 9
	// Scale 1: 9 * 2 = 18
	lineFeed = 9.0f * (1.0f + float(charScale));
}

// Serialization and registration macros
INSTANTIATE_SERIALIZE_METHODS(MSXPlotter);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, MSXPlotter, "MSXPlotter");

} // namespace openmsx
