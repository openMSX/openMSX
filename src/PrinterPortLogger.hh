#ifndef MSXPRINTERPORTLOGGER_HH
#define MSXPRINTERPORTLOGGER_HH

#include "PrinterPortDevice.hh"
#include "FilenameSetting.hh"
#include "File.hh"

namespace openmsx {

class CommandController;

class PrinterPortLogger final : public PrinterPortDevice
{
public:
	explicit PrinterPortLogger(CommandController& commandController);

	// PrinterPortDevice
	[[nodiscard]] bool getStatus(EmuTime::param time) override;
	void setStrobe(bool strobe, EmuTime::param time) override;
	void writeData(byte data, EmuTime::param time) override;

	// Pluggable
	[[nodiscard]] std::string_view getName() const override;
	[[nodiscard]] std::string_view getDescription() const override;
	void plugHelper(Connector& connector, EmuTime::param time) override;
	void unplugHelper(EmuTime::param time) override;

	void serialize(Archive auto& ar, unsigned version);

private:
	FilenameSetting logFilenameSetting;
	File file;
	byte toPrint;
	bool prevStrobe;
};

} // namespace openmsx

#endif
