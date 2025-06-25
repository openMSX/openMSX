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
	[[nodiscard]] bool getStatus(EmuTime time) override;
	void setStrobe(bool strobe, EmuTime time) override;
	void writeData(uint8_t data, EmuTime time) override;

	// Pluggable
	[[nodiscard]] std::string_view getName() const override;
	[[nodiscard]] std::string_view getDescription() const override;
	void plugHelper(Connector& connector, EmuTime time) override;
	void unplugHelper(EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	FilenameSetting logFilenameSetting;
	File file;
	uint8_t toPrint = 0; // Initialize to avoid a static analysis (cppcheck) warning.
	                     // For correctness it's not strictly needed to initialize
	                     // this variable. But understanding why exactly it's not
	                     // needed depends on the implementation details of a few
	                     // other classes, so let's simplify stuff and just
	                     // initialize.
	bool prevStrobe = true;
};

} // namespace openmsx

#endif
