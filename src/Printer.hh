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

#include <array>
#include <cstdint>
#include <memory>
#include <utility>

namespace openmsx {

class MSXMotherBoard;
class IntegerSetting;
class Paper;


// Abstract printer class
class PrinterCore : public PrinterPortDevice
{
public:
	// PrinterPortDevice
	[[nodiscard]] bool getStatus(EmuTime::param time) override;
	void setStrobe(bool strobe, EmuTime::param time) override;
	void writeData(uint8_t data, EmuTime::param time) override;

	// Pluggable
	void plugHelper(Connector& connector, EmuTime::param time) override;
	void unplugHelper(EmuTime::param time) override;

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
	void ensurePrintPage();
	void flushEmulatedPrinter();
	void printVisibleCharacter(uint8_t data);
	void plot9Dots(double x, double y, unsigned pattern);

	[[nodiscard]] virtual std::pair<unsigned, unsigned> getNumberOfDots() = 0;
	virtual void resetSettings() = 0;
	[[nodiscard]] virtual unsigned calcEscSequenceLength(uint8_t character) = 0;
	virtual void processEscSequence() = 0;
	virtual void processCharacter(uint8_t data) = 0;

protected:
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
	enum CountryCode : uint8_t {
		CC_USA              = 0,
		CC_FRANCE           = 1,
		CC_GERMANY          = 2,
		CC_UNITED_KINGDOM   = 3,
		CC_DENMARK          = 4,
		CC_SWEDEN           = 5,
		CC_ITALY            = 6,
		CC_SPAIN            = 7,
		CC_JAPAN            = 8
	};
	CountryCode countryCode = CC_USA;

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

private:
	MSXMotherBoard& motherBoard;
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
	[[nodiscard]] std::string_view getName() const override;
	[[nodiscard]] std::string_view getDescription() const override;

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
	explicit ImagePrinterEpson(MSXMotherBoard& motherBoard);

	// Pluggable
	[[nodiscard]] std::string_view getName() const override;
	[[nodiscard]] std::string_view getDescription() const override;

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
