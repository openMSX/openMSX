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
#include "openmsx.hh"
#include <array>
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
	void writeData(byte data, EmuTime::param time) override;

	// Pluggable
	void plugHelper(Connector& connector, EmuTime::param time) override;
	void unplugHelper(EmuTime::param time) override;

protected:
	PrinterCore() = default;
	~PrinterCore() override = default;
	virtual void write(byte data) = 0;
	virtual void forceFormFeed() = 0;

private:
	byte toPrint = 0;
	bool prevStrobe = true;
};

/*
// sends data to host printer
class RawPrinter : public PrinterCore
{
public:
	RawPrinter();
	~RawPrinter();

	void write(byte data);
	void forceFormFeed();
	// Pluggable
	[[nodiscard]] std::string_view getName() const override;
	[[nodiscard]] std::string_view getDescription() const override;

private:
	HANDLE hFile;
};
*/

// Abstract image printer
class ImagePrinter : public PrinterCore
{
public:
	void write(byte data) override;
	void forceFormFeed() override;

protected:
	ImagePrinter(MSXMotherBoard& motherBoard, bool graphicsHiLo);
	~ImagePrinter() override;

	void resetEmulatedPrinter();
	void printGraphicByte(byte data);
	void seekPrinterHeadRelative(double offset);
	void ensurePrintPage();
	void flushEmulatedPrinter();
	void printVisibleCharacter(byte data);
	void plot9Dots(double x, double y, unsigned pattern);

	[[nodiscard]] virtual std::pair<unsigned, unsigned> getNumberOfDots() = 0;
	virtual void resetSettings() = 0;
	[[nodiscard]] virtual unsigned calcEscSequenceLength(byte character) = 0;
	virtual void processEscSequence() = 0;
	virtual void processCharacter(byte data) = 0;

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
	enum CountryCode {
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
	std::array<byte, MAX_ESC_CMDSIZE> abEscSeq;

	static constexpr int MAX_FONT_WIDTH = 12;
	struct FontInfo {
		std::array<byte, 256 * MAX_FONT_WIDTH> rom;
		std::array<byte, 256 * MAX_FONT_WIDTH> ram;
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
	void msxPrnSetFont(std::span<const byte, 256 * 8> msxBits);
	[[nodiscard]] unsigned parseNumber(unsigned sizeStart, unsigned sizeChars);

	[[nodiscard]] std::pair<unsigned, unsigned> getNumberOfDots() override;
	void resetSettings() override;
	[[nodiscard]] unsigned calcEscSequenceLength(byte character) override;
	void processEscSequence() override;
	void processCharacter(byte data) override;
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
	[[nodiscard]] unsigned calcEscSequenceLength(byte character) override;
	void processEscSequence() override;
	void processCharacter(byte data) override;
};

} // namespace openmsx

#endif
