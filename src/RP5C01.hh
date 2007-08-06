// $Id$
//
// This class implements the RP5C01 chip (RTC)
//
// * For techncal details on RP5C01 see
//     http://w3.qahwah.net/joost/openMSX/RP5C01.pdf
//

#ifndef RP5C01_HH
#define RP5C01_HH

#include "Clock.hh"
#include "openmsx.hh"
#include "noncopyable.hh"
#include <memory>

namespace openmsx {

class CommandController;
class SRAM;
template <typename T> class EnumSetting;

class RP5C01 : private noncopyable
{
public:
	RP5C01(CommandController& commandController, SRAM& regs,
	       const EmuTime& time);
	~RP5C01();

	void reset(const EmuTime& time);
	nibble readPort(nibble port, const EmuTime& time);
	void writePort(nibble port, nibble value, const EmuTime& time);

private:
	void initializeTime();
	void updateTimeRegs(const EmuTime& time);
	void regs2Time();
	void time2Regs();
	void resetAlarm();

	enum RTCMode { EMUTIME, REALTIME };
	static const int FREQ = 16384;

	SRAM& regs;
	std::auto_ptr<EnumSetting<RTCMode> > modeSetting;

	Clock<FREQ> reference;
	int fraction;
	int seconds, minutes, hours;
	int dayWeek, days, months, years, leapYear;

	nibble modeReg, testReg, resetReg;
};

} // namespace openmsx

#endif
