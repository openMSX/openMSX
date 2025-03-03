#ifndef EEPROM_93C46_HH
#define EEPROM_93C46_HH

#include "EmuTime.hh"
#include "SRAM.hh"

#include <climits>
#include <cstdint>
#include <string>

namespace openmsx {

class DeviceConfig;
class XMLElement;

class EEPROM_93C46
{
public:
	static constexpr uint8_t ADDRESS_BITS = 7;
	static constexpr uint32_t NUM_ADDRESSES = 1 << ADDRESS_BITS;
	static constexpr uint32_t ADDRESS_MASK = NUM_ADDRESSES - 1;
	static constexpr uint8_t DATA_BITS = 8; // only 8-bit mode implemented

public:
	explicit EEPROM_93C46(const XMLElement& xml); // unittest
	EEPROM_93C46(const std::string& name, const DeviceConfig& config);

	void reset();

	[[nodiscard]] bool read_DO(EmuTime::param time) const;
	void write_CS (bool value, EmuTime::param time);
	void write_CLK(bool value, EmuTime::param time);
	void write_DI (bool value, EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	// for unittest
	[[nodiscard]] const uint8_t* backdoor() const {
		return &sram[0];
	}

private:
	[[nodiscard]] uint8_t read(unsigned addr) const;
	void write(unsigned addr, uint8_t value, EmuTime::param time);
	void writeAll(uint8_t value, EmuTime::param time);
	void erase(unsigned addr, EmuTime::param time);
	void eraseAll(EmuTime::param time);

	[[nodiscard]] bool ready(EmuTime::param time) const;
	void clockEvent(EmuTime::param time);
	void execute_command(EmuTime::param time);

public: // for serialize
	enum class State : uint8_t {
		IN_RESET,
		WAIT_FOR_START_BIT,
		WAIT_FOR_COMMAND,
		READING_DATA,
		WAIT_FOR_WRITE,
		WAIT_FOR_WRITE_ALL,
	};

private:
	SRAM sram;
	EmuTime completionTime = EmuTime::zero();
	EmuTime csTime = EmuTime::zero();
	State state = State::IN_RESET;
	uint16_t shiftRegister = 0;
	uint8_t bits = 0;
	uint8_t address = 0;
	bool pinCS  = false;
	bool pinCLK = false;
	bool pinDI = false;
	bool writeProtected = true;

	static constexpr int SHIFT_REG_BITS = sizeof(shiftRegister) * CHAR_BIT;
};

} // namespace openmsx

#endif
