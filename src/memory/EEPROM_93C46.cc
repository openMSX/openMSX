#include "EEPROM_93C46.hh"
#include "serialize.hh"

namespace openmsx {

EEPROM_93C46::EEPROM_93C46(const XMLElement& xml)
	: sram(NUM_ADDRESSES, xml, SRAM::DontLoadTag{})
{
}

EEPROM_93C46::EEPROM_93C46(const std::string& name, const DeviceConfig& config)
	: sram(name + " EEPROM", NUM_ADDRESSES, config)
{
}

void EEPROM_93C46::reset()
{
	completionTime = EmuTime::zero();
	state = IN_RESET;
	writeProtected = true;
	bits = 0;
	address = 0;
	shiftRegister = 0;
}

uint8_t EEPROM_93C46::read(unsigned addr)
{
	return sram[addr];
}

void EEPROM_93C46::write(unsigned addr, uint8_t value, EmuTime::param time)
{
	// The ST M93C46 datasheet documents that erase before write is not
	// needed.
	sram.write(addr, value); // not: sram[addr] & value
	completionTime = time + EmuDuration::usec(1750);
}

void EEPROM_93C46::writeAll(uint8_t value, EmuTime::param time)
{
	sram.memset(0, value, NUM_ADDRESSES);
	//for (auto addr : xrange(NUM_ADDRESSES)) {
	//	sram.write(addr, sram[addr] & value);
	//}
	completionTime = time + EmuDuration::usec(8000);
}

void EEPROM_93C46::erase(unsigned addr, EmuTime::param time)
{
	sram.write(addr, 255);
	completionTime = time + EmuDuration::usec(1000);
}

void EEPROM_93C46::eraseAll(EmuTime::param time)
{
	sram.memset(0, 255, NUM_ADDRESSES);
	completionTime = time + EmuDuration::usec(8000);
}

bool EEPROM_93C46::ready(EmuTime::param time) const
{
	return time >= completionTime;
}

bool EEPROM_93C46::read_DO(EmuTime::param time) const
{
	if (state == READING_DATA) {
		return shiftRegister & (1 << (SHIFT_REG_BITS - 1));
	} else if (state == WAIT_FOR_START_BIT) {
		return ready(time);
	} else {
		return true;
	}
}

void EEPROM_93C46::write_CS(bool value, EmuTime::param time)
{
	if (pinCS == value) return;
	pinCS = value;

	if (pinCS) {
		csTime = time; // see clockEvent()

		assert(state == IN_RESET);
		state = WAIT_FOR_START_BIT;
	} else {
		state = IN_RESET;
	}
}

void EEPROM_93C46::write_CLK(bool value, EmuTime::param time)
{
	if (pinCLK == value) return;
	pinCLK = value;

	if (pinCLK) {
		clockEvent(time);
	}
}

void EEPROM_93C46::write_DI(bool value, EmuTime::param /*time*/)
{
	pinDI = value;
}

void EEPROM_93C46::clockEvent(EmuTime::param time)
{
	switch (state) {
	case IN_RESET:
		// nothing
		break;

	case WAIT_FOR_START_BIT:
		// Ignore simultaneous rising CLK and CS edge.
		if (pinDI && ready(time) && (time > csTime)) {
			shiftRegister = 0;
			bits = 0;
			state = WAIT_FOR_COMMAND;
		}
		break;

	case WAIT_FOR_COMMAND:
		shiftRegister = (shiftRegister << 1) | int(pinDI);
		++bits;
		if (bits == (2 + ADDRESS_BITS)) {
			execute_command(time);
		}
		break;

	case READING_DATA:
		if ((bits % DATA_BITS) == 0) {
			uint8_t value = read(address);
			shiftRegister = value << (SHIFT_REG_BITS - DATA_BITS);
			address = (address + 1) & ADDRESS_MASK;
		} else {
			shiftRegister <<= 1;
		}
		++bits;
		break;

	case WAIT_FOR_WRITE:
		shiftRegister = (shiftRegister << 1) | int(pinDI);
		++bits;
		if (bits == DATA_BITS) {
			if (writeProtected) {
				state = IN_RESET;
				break;
			}
			write(address, shiftRegister, time);
			state = IN_RESET;
		}
		break;

	case WAIT_FOR_WRITEALL:
		shiftRegister = (shiftRegister << 1) | int(pinDI);
		++bits;
		if (bits == DATA_BITS) {
			if (writeProtected) {
				state = IN_RESET;
				break;
			}
			writeAll(shiftRegister, time);
			state = IN_RESET;
		}
		break;
	}
}

void EEPROM_93C46::execute_command(EmuTime::param time)
{
	bits = 0;
	address = shiftRegister & ADDRESS_MASK;

	switch ((shiftRegister >> ADDRESS_BITS) & 3) {
	case 0:
		switch (address >> (ADDRESS_BITS - 2)) {
		case 0: // LOCK
			writeProtected = true;
			state = IN_RESET;
			break;

		case 1: // WRITEALL
			shiftRegister = 0;
			state = WAIT_FOR_WRITEALL;
			break;

		case 2: // ERASEALL
			if (writeProtected) {
				state = IN_RESET;
				break;
			}
			eraseAll(time);
			state = IN_RESET;
			break;

		case 3: // UNLOCK
			writeProtected = false;
			state = IN_RESET;
			break;
		}
		break;

	case 1: // WRITE
		shiftRegister = 0;
		state = WAIT_FOR_WRITE;
		break;

	case 2: // READ
		// Data is fetched after first CLK. Reset the shift register to
		// 0 to simulate the dummy 0 bit that happens prior to the
		// first clock
		shiftRegister = 0;
		state = READING_DATA;
		break;

	case 3: // ERASE
		if (writeProtected) {
			state = IN_RESET;
			break;
		}
		erase(address, time);
		state = IN_RESET;
		break;
	}
}

static constexpr std::initializer_list<enum_string<EEPROM_93C46::State>> stateInfo = {
	{ "IN_RESET",           EEPROM_93C46::IN_RESET           },
	{ "WAIT_FOR_START_BIT", EEPROM_93C46::WAIT_FOR_START_BIT },
	{ "WAIT_FOR_COMMAND",   EEPROM_93C46::WAIT_FOR_COMMAND   },
	{ "READING_DATA",       EEPROM_93C46::READING_DATA       },
	{ "WAIT_FOR_WRITE",     EEPROM_93C46::WAIT_FOR_WRITE     },
	{ "WAIT_FOR_WRITEALL",  EEPROM_93C46::WAIT_FOR_WRITEALL  },
};
SERIALIZE_ENUM(EEPROM_93C46::State, stateInfo);

void EEPROM_93C46::serialize(Archive auto& ar, unsigned /*version*/)
{
	ar.serialize("sram",           sram,
	             "completionTime", completionTime,
	             "csTime",         csTime,
	             "state",          state,
	             "shiftRegister",  shiftRegister,
	             "bits",           bits,
	             "address",        address,
	             "pinCS",          pinCS,
	             "pinCLK",         pinCLK,
	             "pinDI",          pinDI,
	             "writeProtected", writeProtected);
}
INSTANTIATE_SERIALIZE_METHODS(EEPROM_93C46);

} // namespace openmsx
