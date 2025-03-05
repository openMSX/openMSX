#ifndef Y8950PERIPHERY_HH
#define Y8950PERIPHERY_HH

#include "EmuTime.hh"

#include <cstdint>
#include <memory>
#include <string>

namespace openmsx {

using uint4_t = uint8_t;

/** Models the 4 general purpose I/O pins on the Y8950
  * (controlled by registers r#18 and r#19)
  */
class Y8950Periphery
{
public:
	virtual ~Y8950Periphery() = default;

	virtual void reset();

	/** Write to (some of) the pins
	  * @param outputs A '1' bit indicates the corresponding bit is
	  *                programmed as output.
	  * @param values The actual value that is written, only bits for
	  *               which the corresponding bit in the 'outputs'
	  *               parameter is set are meaningful.
	  * @param time The moment in time the write occurs
	  */
	virtual void write(uint4_t outputs, uint4_t values, EmuTime::param time) = 0;

	/** Read from (some of) the pins
	  * Some of the pins might be programmed as output, but this method
	  * doesn't care about that, it should return the value of all pins
	  * as-if they were all programmed as input.
	  * @param time The moment in time the read occurs
	  */
	[[nodiscard]] virtual uint4_t read(EmuTime::param time) = 0;

	/** SP-OFF bit (bit 3 in Y8950 register 7) */
	virtual void setSPOFF(bool value, EmuTime::param time);

	[[nodiscard]] virtual uint8_t readMem(uint16_t address, EmuTime::param time);
	[[nodiscard]] virtual uint8_t peekMem(uint16_t address, EmuTime::param time) const;
	virtual void writeMem(uint16_t address, uint8_t value, EmuTime::param time);
	[[nodiscard]] virtual const uint8_t* getReadCacheLine(uint16_t start) const;
	[[nodiscard]] virtual uint8_t* getWriteCacheLine(uint16_t start);
};

class MSXAudio;
class DeviceConfig;

class Y8950PeripheryFactory
{
public:
	[[nodiscard]] static std::unique_ptr<Y8950Periphery> create(
		MSXAudio& audio, DeviceConfig& config,
		const std::string& soundDeviceName);
};

} // namespace openmsx

#endif
