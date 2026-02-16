/** \file Printer.hh
 * Dot Matrix Printer Emulation
 *   code mostly copied from blueMSX
 *   but changed to:
 *      OO-style
 *      save to png images (with anti-aliased circles)
 *      fit in openMSX structure
 */

#ifndef PRINTER_HH
#define PRINTER_HH

#include "PrinterPortDevice.hh"

#include "MSXCharacterSets.hh"
#include "Paper.hh"
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace openmsx {

class MSXMotherBoard;
class IntegerSetting;

// Abstract printer class
class PrinterCore : public PrinterPortDevice
{
public:
	// PrinterPortDevice
	[[nodiscard]] bool getStatus(EmuTime time) override;
	void setStrobe(bool strobe, EmuTime time) override;
	void writeData(uint8_t data, EmuTime time) override;

	// Pluggable
	void plugHelper(Connector& connector, EmuTime time) override;
	void unplugHelper(EmuTime time) override;

protected:
	PrinterCore() = default;
	~PrinterCore() override = default;
	virtual void write(uint8_t data) = 0;
	virtual void forceFormFeed() = 0;

private:
	uint8_t toPrint = 0;
	bool prevStrobe = true;
};


// Abstract image printer
class ImagePrinter : public PrinterCore
{
public:
	void write(uint8_t data) override;
	void forceFormFeed() override;

protected:
	ImagePrinter(MSXMotherBoard& motherBoard, bool graphicsHiLo);
	~ImagePrinter() override;

	void resetEmulatedPrinter();
	void printGraphicByte(uint8_t data);
	void seekPrinterHeadRelative(double offset);
	virtual void ensurePrintPage();
	virtual void flushEmulatedPrinter();
	void printVisibleCharacter(uint8_t data);
	void plot9Dots(double x, double y, unsigned pattern);

	[[nodiscard]] virtual std::pair<unsigned, unsigned> getNumberOfDots() = 0;

	// Allow derived classes to specify if they want color output
	[[nodiscard]] virtual bool useColor() const { return false; }

	// Allow derived classes to specify paper size in mm (default A4 portrait)
	[[nodiscard]] virtual std::pair<double, double> getPaperSize() const {
		return {210.0, 297.0};
	}

	virtual void resetSettings() = 0;
	[[nodiscard]] virtual unsigned calcEscSequenceLength(uint8_t character) = 0;
	virtual void processEscSequence() = 0;
	virtual void processCharacter(uint8_t data) = 0;

	// Provide protected accessor for paper
	Paper *getPaper() { return paper.get(); }
	const Paper *getPaper() const { return paper.get(); }
	static constexpr unsigned PIXEL_WIDTH = 8;

	double graphDensity;
	double fontDensity;
	double hpos;
	double vpos;
	double pageTop;
	double lineFeed;
	double pageHeight;
	double printAreaTop = -1.0;
	double printAreaBottom = 0.0;
	double pixelSizeX;
	double pixelSizeY;
	int  eightBit = 0;
	unsigned perforationSkip = 0;
	unsigned leftBorder;
	unsigned rightBorder;
	unsigned fontWidth;
	unsigned remainingCommandBytes = 0;
	unsigned sizeEscPos = 0;
	unsigned sizeRemainingDataBytes = 0;
	unsigned ramLoadOffset = 0;
	unsigned ramLoadEnd = 0;
	unsigned lines;
	enum class CountryCode : uint8_t {
		USA              = 0,
		FRANCE           = 1,
		GERMANY          = 2,
		UNITED_KINGDOM   = 3,
		DENMARK          = 4,
		SWEDEN           = 5,
		ITALY            = 6,
		SPAIN            = 7,
		JAPAN            = 8
	};
	CountryCode countryCode = CountryCode::USA;

	static constexpr int MAX_ESC_CMDSIZE = 8;
	std::array<uint8_t, MAX_ESC_CMDSIZE> abEscSeq;

	static constexpr int MAX_FONT_WIDTH = 12;
	struct FontInfo {
		std::array<uint8_t, 256 * MAX_FONT_WIDTH> rom;
		std::array<uint8_t, 256 * MAX_FONT_WIDTH> ram;
		double pixelDelta;
		unsigned charWidth;
		bool useRam;
	} fontInfo;

	bool letterQuality = false;
	bool bold = false;
	bool proportional = false;
	bool italic = false;
	bool superscript = false;
	bool subscript = false;
	bool doubleWidth = false;
	bool underline = false;
	bool doubleStrike = false;
	bool escSequence = false;
	bool alternateChar = false;
	bool detectPaperOut = false;
	bool japanese = false;
	bool normalAfterLine = false;
	bool ninePinGraphics = false;
	bool leftToRight = false;
	bool elite = false;
	bool compressed = false;
	bool noHighEscapeCodes = false;

protected:
	MSXMotherBoard &motherBoard;

private:
	std::unique_ptr<Paper> paper;

	std::shared_ptr<IntegerSetting> dpiSetting;

	const bool graphicsHiLo;
};

// emulated MSX printer
class ImagePrinterMSX final : public ImagePrinter
{
public:
	explicit ImagePrinterMSX(MSXMotherBoard& motherBoard);

	// Pluggable
	[[nodiscard]] zstring_view getName() const override;
	[[nodiscard]] zstring_view getDescription() const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	[[nodiscard]] unsigned parseNumber(unsigned sizeStart, unsigned sizeChars);

	[[nodiscard]] std::pair<unsigned, unsigned> getNumberOfDots() override;
	void resetSettings() override;
	[[nodiscard]] unsigned calcEscSequenceLength(uint8_t character) override;
	void processEscSequence() override;
	void processCharacter(uint8_t data) override;
};

// emulated Epson printer
class ImagePrinterEpson final : public ImagePrinter
{
public:
	explicit ImagePrinterEpson(MSXMotherBoard &motherBoard);

	// Pluggable
	[[nodiscard]] zstring_view getName() const override;
	[[nodiscard]] zstring_view getDescription() const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	[[nodiscard]] unsigned parseNumber(unsigned sizeStart, unsigned sizeChars);

	[[nodiscard]] std::pair<unsigned, unsigned> getNumberOfDots() override;
	void resetSettings() override;
	[[nodiscard]] unsigned calcEscSequenceLength(uint8_t character) override;
	void processEscSequence() override;
	void processCharacter(uint8_t data) override;
};

} // namespace openmsx

#endif
