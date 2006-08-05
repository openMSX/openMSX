// $Id$

/** 
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
#include <memory>
#include <vector>

namespace openmsx {

class CommandController;
class CliComm;

class Paper
{
public:
	Paper(unsigned x, unsigned y);
	~Paper();

	std::string save() const;
	void setDotSize(double sizeX, double sizeY);
	void plot(double x, double y);

private:
	byte& dot(unsigned x, unsigned y);

	byte* buf;
	unsigned sizeX;
	unsigned sizeY;

	double radiusX;
	double radiusY;
	int radius16;
	std::vector<int> table;
};


// Abstract printer class
class PrinterCore : public PrinterPortDevice
{
public:
	// PrinterPortDevice
	virtual bool getStatus(const EmuTime& time);
	virtual void setStrobe(bool strobe, const EmuTime& time);
	virtual void writeData(byte data, const EmuTime& time);

	// Pluggable
	virtual void plugHelper(Connector& connector, const EmuTime& time);
	virtual void unplugHelper(const EmuTime& time);

protected:
	PrinterCore();
	virtual ~PrinterCore();
	virtual void write(byte data) = 0;
	virtual void forceFormFeed() = 0;

private:
	byte toPrint;
	bool prevStrobe;
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
	virtual const std::string& getName() const;
	virtual const std::string& getDescription() const;

private:
	HANDLE hFile;
};
*/

// Abstract image printer
class ImagePrinter : public PrinterCore
{
public:
	virtual void write(byte data);
	virtual void forceFormFeed();

protected:
	ImagePrinter(CommandController& commandController);
	~ImagePrinter();

	void resetEmulatedPrinter();
	void printGraphicByte(byte data);
	void seekPrinterHeadRelative(double offset);
	void ensurePrintPage();
	void flushEmulatedPrinter();
	void printVisibleCharacter(byte data);
	void plot9Dots(double x, double y, unsigned pattern);

	virtual void getNumberOfDots(unsigned& dotsX, unsigned& dotsY) = 0;
	virtual void resetSettings() = 0;
	virtual unsigned calcEscSequenceLength(byte character) = 0;
	virtual void processEscSequence() = 0;
	virtual void processCharacter(byte data) = 0;

	static const unsigned PIXEL_WIDTH = 8;

	bool letterQuality;
	bool bold;
	bool proportional;
	bool italic;
	bool superscript;
	bool subscript;
	bool doubleWidth;
	bool underline;
	bool doubleStrike;
	bool escSequence;
	bool alternateChar;
	bool detectPaperOut;
	bool japanese;
	bool normalAfterLine;
	bool ninePinGraphics;
	bool leftToRight;
	bool graphicsHiLo;
	bool elite;
	bool compressed;
	bool noHighEscapeCodes;
	int  eightBit;
	unsigned perforationSkip;
	unsigned leftBorder;
	unsigned rightBorder;
	unsigned fontWidth;
	unsigned remainingCommandBytes;
	unsigned sizeEscPos;
	static const int MAX_ESC_CMDSIZE = 8; 
	byte abEscSeq[MAX_ESC_CMDSIZE];
	unsigned sizeRemainingDataBytes;
	unsigned ramLoadOffset;
	unsigned ramLoadEnd;
	double graphDensity;
	double fontDensity;
	double hpos;
	double vpos;
	double pageTop;
	double lineFeed;
	double pageHeight;
	unsigned lines;
	double printAreaTop;
	double printAreaBottom;
	double pixelSizeX;
	double pixelSizeY;
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
	CountryCode countryCode;

	static const int MAX_FONT_WIDTH = 12;
	struct FontInfo {
		byte rom[256 * MAX_FONT_WIDTH];
		byte ram[256 * MAX_FONT_WIDTH];
		bool useRam;
		unsigned charWidth;
		double pixelDelta;
	} fontInfo;

private:
	CliComm& cliComm;
	std::auto_ptr<Paper> paper;
};

// emulated MSX printer
class ImagePrinterMSX : public ImagePrinter
{
public:
	ImagePrinterMSX(CommandController& commandController);

	// Pluggable
	virtual const std::string& getName() const;
	virtual const std::string& getDescription() const;

private:
	void msxPrnSetFont(const byte* msxBits);
	unsigned parseNumber(unsigned sizeStart, unsigned sizeChars);

	virtual void getNumberOfDots(unsigned& dotsX, unsigned& dotsY);
	virtual void resetSettings();
	virtual unsigned calcEscSequenceLength(byte character);
	virtual void processEscSequence();
	virtual void processCharacter(byte data);
};

// emulated Epson printer
class ImagePrinterEpson : public ImagePrinter
{
public:
	ImagePrinterEpson(CommandController& commandController);

	// Pluggable
	virtual const std::string& getName() const;
	virtual const std::string& getDescription() const;

private:
	unsigned parseNumber(unsigned sizeStart, unsigned sizeChars);

	virtual void getNumberOfDots(unsigned& dotsX, unsigned& dotsY);
	virtual void resetSettings();
	virtual unsigned calcEscSequenceLength(byte character);
	virtual void processEscSequence();
	virtual void processCharacter(byte data);
};

} // namespace openmsx

#endif
