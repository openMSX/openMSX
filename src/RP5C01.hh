// This class implements the RP5C01 chip (RTC)
//
// * For technical details on RP5C01 see
//     http://w3.qahwah.net/joost/openMSX/RP5C01.pdf

#ifndef RP5C01_HH
#define RP5C01_HH

#include "Clock.hh"
#include "EnumSetting.hh"

#include <cstdint>
#include <string>

namespace openmsx {

using uint4_t = uint8_t;

class CommandController;
class SRAM;

class RP5C01
{
public:
	enum class RTCMode : uint8_t { EMUTIME, REALTIME };

	RP5C01(CommandController& commandController, SRAM& regs,
	       EmuTime time, const std::string& name);

	void reset(EmuTime time);
	[[nodiscard]] uint4_t readPort(uint4_t port, EmuTime time);
	[[nodiscard]] uint4_t peekPort(uint4_t port) const;
	void writePort(uint4_t port, uint4_t value, EmuTime time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void initializeTime();
	void updateTimeRegs(EmuTime time);
	void regs2Time();
	void time2Regs();
	void resetAlarm();

private:
	static constexpr unsigned FREQ = 16384;

	SRAM& regs;
	EnumSetting<RTCMode> modeSetting;

	Clock<FREQ> reference;
	unsigned fraction;
	unsigned seconds, minutes, hours;
	unsigned dayWeek, years, leapYear;
	int days, months; // these two can be -1

	uint4_t modeReg, testReg, resetReg;
};

} // namespace openmsx

#endif
