#include "SensorKid.hh"
#include "Clock.hh"
#include "serialize.hh"
#include "Rom.hh"
#include "MSXException.hh"
#include <iostream>
#include <stdlib.h>

using std::cout;
using std::endl;

namespace openmsx {

SensorKid::SensorKid(const DeviceConfig& config)
	: MSXDevice(config)
{
	prev = 255; // previously written value to port 0
	mb4052_ana = 0; // the analog value that's currently being read
	mb4052_count = 0; // keep track of which bit we're reading
	analog[0] = rand() % 256;   // Brightness / HIKARI
	analog[1] = rand() % 256;   // Temperature /  ONDO
	analog[2] = rand() % 256;   // Sound / OTO
	analog[3] = 0xFF; // not connected??  always reads as 255
	analog_dir[0] = rand() % 2;
	analog_dir[1] = rand() % 2;
	analog_dir[2] = rand() % 2;
	analog_dir[3] = rand() % 2;
}

void SensorKid::writeIO(word port, byte value, EmuTime::param /* time */)
{
	if ((port & 0xFF) != 0) {
		return;
	}

	byte diff = prev ^ value;
	prev = value;

	if (diff & 0xC0) {
		// upper two bits changed
		putPort(value, diff);
	}

	if (diff & 0x10) {
		// CS bit changed
		if (value & 0x10) {
			// 0 -> 1 : start of A/D convert
			mb4052_ana = getAnalog(value & 0x0C);
			mb4052_count = 12;
		} else {
			// 1 -> 0 : end of A/D convert
			mb4052_count = 0;
		}
	}
	if ((diff & 0x01) && (value & 0x01)) {
		// CLK changed 0 -> 1
		if (mb4052_count > 0) --mb4052_count;
	}
}

byte SensorKid::readIO(word port, EmuTime::param /* time */)
{
	if ((port & 0xFF) == 0) {
		// port 0
		// mb4052_count can take on values [0, 12]
		//  for 12,11,1,0 we return 1
		//  for 10        we return 0
		//  for 9..2      we return one of the analog bits
		byte result;

		if (mb4052_count == 10) {
			result = 0;
		} else if ((mb4052_count < 10) && (mb4052_count > 1)) {
			result = (mb4052_ana >> (mb4052_count - 2)) & 1;
		} else {
			result = 1;
		}
		return 0xFE | result; // other bits read as '1'.
	} else if ((port & 0xFF) == 1) {
		// port 1
		// returns two of the previously written bits (shuffled in different positions)
		return 0xFC | ((prev >> 5) & 0x02) | ((prev >> 7) & 0x01);
	}
	return 0xFC | ((prev >> 5) & 0x02) | ((prev >> 7) & 0x01);

}

byte SensorKid::getAnalog(byte chi)
{
	// bits 2 and 3 in the 'port-0' byte select between 4 possible channels
	// for some reason bits 2 and 3 are swapped and then shifted down
	byte port = ((chi >> 1) & 2) | ((chi >> 3) & 1);

	if (analog_dir[port] == 0) {
		analog[port]--;
		if (analog[port] == 0) {
			analog_dir[port] = 1;
		}
	} else {
		analog[port]++;
		if (analog[port] == 255) {
			analog_dir[port] = 0;
		}
	}

	return analog[port];
}

void SensorKid::putPort(byte data, byte diff)
{
	// when the upper 2 bits (bit 6 and 7) change we send a message?
	// I assume the cartridge also has two digital output pins?
	if (diff & 0x80) {
		cout << "Status Port 0: " << (int)((data & 0x80) == 0) << endl;
	}
	if (diff & 0x40) {
		cout << "Status Port 1:  " << (int)((data & 0x40) == 0) << endl;
	}
}

template<typename Archive>
void SensorKid::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
}
INSTANTIATE_SERIALIZE_METHODS(SensorKid);
REGISTER_MSXDEVICE(SensorKid, "SensorKid");

}
