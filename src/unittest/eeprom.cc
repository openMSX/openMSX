#include "catch.hpp"
#include "EmuTime.hh"
#include "EEPROM_93C46.hh"
#include "XMLElement.hh"
#include "xrange.hh"

using namespace openmsx;

constexpr auto step = EmuDuration::usec(10);

static void input_pattern(EEPROM_93C46& eeprom, EmuTime& time, uint32_t pattern, unsigned len)
{
	assert(len <= 32);
	for (auto i : xrange(len)) {
		eeprom.write_DI(pattern & (1 << (len - 1 - i)), time);
		time += step;
		eeprom.write_CLK(true, time);
		time += step;
		eeprom.write_CLK(false, time);
		time += step;
	}
}

static void write(EEPROM_93C46& eeprom, EmuTime& time, unsigned addr, uint8_t value)
{
	assert(addr < EEPROM_93C46::NUM_ADDRESSES);

	eeprom.write_CS(true, time);
	time += step;

	uint32_t pattern = 0b101; // start-bit + write-opcode
	pattern <<= EEPROM_93C46::ADDRESS_BITS;
	pattern |= addr;
	pattern <<= EEPROM_93C46::DATA_BITS;
	pattern |= value;
	unsigned len = 3 + EEPROM_93C46::ADDRESS_BITS + EEPROM_93C46::DATA_BITS;
	input_pattern(eeprom, time, pattern, len);

	eeprom.write_CS(false, time);
	time += step;
}

static void write_all(EEPROM_93C46& eeprom, EmuTime& time, uint8_t value)
{
	eeprom.write_CS(true, time);
	time += step;

	uint32_t pattern = 0b100; // start-bit + write-opcode
	pattern <<= EEPROM_93C46::ADDRESS_BITS;
	pattern |= 0b0100000; // 0b01xxxxx
	pattern <<= EEPROM_93C46::DATA_BITS;
	pattern |= value;
	unsigned len = 3 + EEPROM_93C46::ADDRESS_BITS + EEPROM_93C46::DATA_BITS;
	input_pattern(eeprom, time, pattern, len);

	eeprom.write_CS(false, time);
	time += step;
}

static void write_enable(EEPROM_93C46& eeprom, EmuTime& time)
{
	eeprom.write_CS(true, time);
	time += step;

	uint32_t pattern = 0b100; // start-bit + '00'-opcode
	pattern <<= EEPROM_93C46::ADDRESS_BITS;
	pattern |= 0b1100000; // 0b11xxxxx
	unsigned len = 3 + EEPROM_93C46::ADDRESS_BITS;
	input_pattern(eeprom, time, pattern, len);

	eeprom.write_CS(false, time);
	time += step;
}

static void write_disable(EEPROM_93C46& eeprom, EmuTime& time)
{
	eeprom.write_CS(true, time);
	time += step;

	uint32_t pattern = 0b100; // start-bit + '00'-opcode
	pattern <<= EEPROM_93C46::ADDRESS_BITS;
	pattern |= 0b0000000; // 0b00xxxxx
	unsigned len = 3 + EEPROM_93C46::ADDRESS_BITS;
	input_pattern(eeprom, time, pattern, len);

	eeprom.write_CS(false, time);
	time += step;
}

static bool waitIdle(EEPROM_93C46& eeprom, EmuTime& time)
{
	eeprom.write_CS(true, time);
	time += step;

	bool wasBusy = false;
	int i = 0;
	while (i < 10'000) {
		bool ready = eeprom.read_DO(time);
		time += step;
		if (ready) break;
		wasBusy = true;
	}
	CHECK(i < 10'000); // we must not wait indefinitely

	eeprom.write_CS(false, time);
	time += step;

	return wasBusy;
}

static uint8_t read(EEPROM_93C46& eeprom, EmuTime& time, unsigned addr)
{
	assert(addr < EEPROM_93C46::NUM_ADDRESSES);

	eeprom.write_CS(true, time);
	time += step;

	uint32_t pattern = 0b110; // start-bit + write-opcode
	pattern <<= EEPROM_93C46::ADDRESS_BITS;
	pattern |= addr;
	unsigned len = 3 + EEPROM_93C46::ADDRESS_BITS;
	input_pattern(eeprom, time, pattern, len);

	CHECK(eeprom.read_DO(time) == false); // initial 0-bit
	uint8_t result = 0;
	for (auto i : xrange(EEPROM_93C46::DATA_BITS)) {
		(void)i;
		eeprom.write_CLK(true, time);
		time += step;
		result <<= 1;
		result |= eeprom.read_DO(time);
		time += step;
		eeprom.write_CLK(false, time);
		time += step;
	}

	eeprom.write_CS(false, time);
	time += step;

	return result;
}

static void read_block(EEPROM_93C46& eeprom, EmuTime& time, unsigned addr,
                       unsigned num, uint8_t* output)
{
	assert(addr < EEPROM_93C46::NUM_ADDRESSES);

	eeprom.write_CS(true, time);
	time += step;

	uint32_t pattern = 0b110; // start-bit + write-opcode
	pattern <<= EEPROM_93C46::ADDRESS_BITS;
	pattern |= addr;
	unsigned len = 3 + EEPROM_93C46::ADDRESS_BITS;
	input_pattern(eeprom, time, pattern, len);

	CHECK(eeprom.read_DO(time) == false); // initial 0-bit
	for (auto j : xrange(num)) {
		(void)j;
		*output = 0;
		for (auto i : xrange(EEPROM_93C46::DATA_BITS)) {
			(void)i;
			eeprom.write_CLK(true, time);
			time += step;
			*output <<= 1;
			*output |= eeprom.read_DO(time);
			time += step;
			eeprom.write_CLK(false, time);
			time += step;
		}
		++output;
	}

	eeprom.write_CS(false, time);
	time += step;
}

static void erase(EEPROM_93C46& eeprom, EmuTime& time, unsigned addr)
{
	assert(addr < EEPROM_93C46::NUM_ADDRESSES);

	eeprom.write_CS(true, time);
	time += step;

	uint32_t pattern = 0b111; // start-bit + '00'-opcode
	pattern <<= EEPROM_93C46::ADDRESS_BITS;
	pattern |= addr;
	unsigned len = 3 + EEPROM_93C46::ADDRESS_BITS;
	input_pattern(eeprom, time, pattern, len);

	eeprom.write_CS(false, time);
	time += step;
}

static void erase_all(EEPROM_93C46& eeprom, EmuTime& time)
{
	eeprom.write_CS(true, time);
	time += step;

	uint32_t pattern = 0b100; // start-bit + '00'-opcode
	pattern <<= EEPROM_93C46::ADDRESS_BITS;
	pattern |= 0b1000000; // 0b10xxxxx
	unsigned len = 3 + EEPROM_93C46::ADDRESS_BITS;
	input_pattern(eeprom, time, pattern, len);

	eeprom.write_CS(false, time);
	time += step;
}

TEST_CASE("EEPROM_93C46")
{
	static XMLElement* xml = [] {
		auto& doc = XMLDocument::getStaticDocument();
		return doc.allocateElement("dummy");
	}();
	EEPROM_93C46 eeprom(*xml);
	const uint8_t* data = eeprom.backdoor();
	EmuTime time = EmuTime::zero();

	// initially filled with 255
	for (auto addr : xrange(EEPROM_93C46::NUM_ADDRESSES)) {
		CHECK(data[addr] == 255);
	}

	// write 123 to address 45 .. but eeprom is still write-protected
	write(eeprom, time, 45, 123);
	CHECK(!waitIdle(eeprom, time));
	for (auto addr : xrange(EEPROM_93C46::NUM_ADDRESSES)) {
		CHECK(data[addr] == 255);
	}

	// again, but first write-enable
	write_enable(eeprom, time);
	CHECK(!waitIdle(eeprom, time));
	write(eeprom, time, 45, 123);
	CHECK(waitIdle(eeprom, time));
	for (auto addr : xrange(EEPROM_93C46::NUM_ADDRESSES)) {
		uint8_t expected = (addr == 45) ? 123 : 255;
		CHECK(data[addr] == expected);
	}

	// read address 45
	CHECK(int(read(eeprom, time, 45)) == 123); // contains 123
	CHECK(!waitIdle(eeprom, time)); // not busy after read
	CHECK(int(read(eeprom, time, 46)) == 255); // other addr still contains 255
	CHECK(!waitIdle(eeprom, time)); // not busy after read

	// write to address 45 without first erasing it
	//   You might expect to read the value 123 & 20 == 16, but the M93C46
	//   datasheet documents that the write command does an auto-erase.
	write(eeprom, time, 45, 20);
	CHECK(waitIdle(eeprom, time));
	for (auto addr : xrange(EEPROM_93C46::NUM_ADDRESSES)) {
		uint8_t expected = (addr == 45) ? 20 : 255;
		CHECK(data[addr] == expected);
	}

	// erase addr 99 -> doesn't influence addr 45
	erase(eeprom, time, 99);
	CHECK(waitIdle(eeprom, time));
	for (auto addr : xrange(EEPROM_93C46::NUM_ADDRESSES)) {
		uint8_t expected = (addr == 45) ? 20 : 255;
		CHECK(data[addr] == expected);
	}

	// erase addr 45 -> all 255 again
	erase(eeprom, time, 45);
	CHECK(waitIdle(eeprom, time));
	for (auto addr : xrange(EEPROM_93C46::NUM_ADDRESSES)) {
		CHECK(data[addr] == 255);
	}

	// write all
	write_all(eeprom, time, 77);
	CHECK(waitIdle(eeprom, time));
	for (auto addr : xrange(EEPROM_93C46::NUM_ADDRESSES)) {
		CHECK(data[addr] == 77);
	}

	// write to the end and the start of the address space
	write(eeprom, time, EEPROM_93C46::NUM_ADDRESSES - 2, 5);
	CHECK(waitIdle(eeprom, time));
	write(eeprom, time, EEPROM_93C46::NUM_ADDRESSES - 1, 11);
	CHECK(waitIdle(eeprom, time));
	write(eeprom, time, 0, 22);
	CHECK(waitIdle(eeprom, time));

	// write-disable and one more write
	write_disable(eeprom, time);
	CHECK(!waitIdle(eeprom, time));
	write(eeprom, time, 1, 33); // not executed
	CHECK(!waitIdle(eeprom, time));
	for (auto addr : xrange(EEPROM_93C46::NUM_ADDRESSES)) {
		uint8_t expected = (addr == 126) ?  5
		                 : (addr == 127) ? 11
		                 : (addr ==   0) ? 22
		                                 : 77;
		CHECK(data[addr] == expected);
	}

	// read-block, with wrap-around
	uint8_t buf[6];
	read_block(eeprom, time, EEPROM_93C46::NUM_ADDRESSES - 3, 6, buf);
	CHECK(!waitIdle(eeprom, time));
	CHECK(buf[0] == 77);
	CHECK(buf[1] ==  5);
	CHECK(buf[2] == 11);
	CHECK(buf[3] == 22);  // wrapped to address 0
	CHECK(buf[4] == 77); // was write protected
	CHECK(buf[5] == 77);

	// erase-all, but write-protected
	erase_all(eeprom, time);
	CHECK(!waitIdle(eeprom, time));
	CHECK(data[0] == 22); // not changed

	// erase-all, write-enabled
	write_enable(eeprom, time);
	CHECK(!waitIdle(eeprom, time));
	erase_all(eeprom, time);
	CHECK(waitIdle(eeprom, time));
	for (auto addr : xrange(EEPROM_93C46::NUM_ADDRESSES)) {
		CHECK(data[addr] == 255);
	}
}
