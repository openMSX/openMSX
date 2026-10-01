#include "makoto-reference/ReferenceYM2608.hh"
// YM2608 debugger access: compare peek values with real reads on a clone,
// while proving that peeks leave chip state and host side effects untouched.
// Build with ymfm_opn.cpp, ymfm_ssg.cpp, ymfm_adpcm.cpp (C++17 or newer).
#include "ymfm_opn.h"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

static void require(bool condition, const char* message)
{
	if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}
struct Host : ymfm::ymfm_interface
{
	bool busy = false;
	std::array<unsigned, 4> effects{}; // reads, writes, IRQ updates, timers
	static uint8_t value(ymfm::access_class type, uint32_t address)
	{
		return uint8_t((address * 73 + unsigned(type) * 19) ^ 0x5a);
	}
	uint8_t ymfm_external_read(ymfm::access_class type, uint32_t address) override
	{
		++effects[0]; return value(type, address);
	}
	uint8_t ymfm_external_peek(ymfm::access_class type, uint32_t address) override
	{
		return value(type, address);
	}
	void ymfm_external_write(ymfm::access_class, uint32_t, uint8_t) override { ++effects[1]; }
	void ymfm_update_irq(bool) override { ++effects[2]; }
	void ymfm_set_timer(uint32_t, int32_t) override { ++effects[3]; }
	bool ymfm_is_busy() override { return busy; }
};
static std::vector<uint8_t> save(ymfm::ym2608_reference& chip)
{
	std::vector<uint8_t> bytes;
	ymfm::ymfm_saved_state state(bytes, true); chip.save_restore(state);
	return bytes;
}
static void write(ymfm::ym2608_reference& chip, unsigned reg, uint8_t value)
{
	unsigned port = (reg >> 8) * 2;
	chip.write(port, uint8_t(reg)); chip.write(port + 1, value);
}
static unsigned checks = 0;
static void check(ymfm::ym2608_reference& chip, Host& host)
{
	auto bytes = save(chip);
	auto effects = host.effects;
	Host other; other.busy = host.busy;
	ymfm::ym2608_reference clone(other);
	for (unsigned port = 0; port < 4; ++port)
	{
		ymfm::ymfm_saved_state state(bytes, false); clone.save_restore(state);
		auto expected = clone.read(port);
		for (unsigned repeat = 0; repeat < 3; ++repeat)
			require(chip.peek(port) == expected, "Peek differs from a real read on the same state");
	}
	for (unsigned reg = 0; reg < 512; ++reg)
		(void)chip.peek_register(uint16_t(reg));
	require(bytes == save(chip), "Debugger changed the chip state");
	require(effects == host.effects, "Debugger caused a host read/write, IRQ or timer side effect");
	++checks;
}
int main()
{
	Host host; ymfm::ym2608_reference chip(host); chip.reset();
	check(chip, host);
	for (bool busy : {true, false})
	{
		host.busy = busy;
		for (unsigned bank = 0; bank < 2; ++bank)
			for (unsigned reg = 0; reg < 256; ++reg)
			{
				chip.write(bank * 2, uint8_t(reg));
				check(chip, host);
			}
	}
	// GPIO: input reads use the safe external callback; output reads use latches.
	for (auto mode : {0x00, 0x40, 0x80, 0xc0})
	{
		write(chip, 7, uint8_t(mode));
		write(chip, 14, 0x35); check(chip, host);
		write(chip, 15, 0x97); check(chip, host);
	}
	write(chip, 0x29, 0x83);
	require(chip.peek_register(0x29) == 0x83, "IRQ mask register missing");
	write(chip, 0x110, 0x04);
	require(chip.peek_register(0x110) == 4, "Flag control register missing");
	write(chip, 0x110, 0x80); // clear flags, not a new flag mask
	require(chip.peek_register(0x110) == 4, "Command replaced effective flag control");
	// Frequency high byte must remain pending until the low byte is written.
	write(chip, 0xa4, 0x22); write(chip, 0xa0, 0x69);
	write(chip, 0xa4, 0x35);
	require(chip.peek_register(0xa4) == 0x22, "Viewer shows last write instead of effective frequency");
	write(chip, 0xa0, 0x70);
	require(chip.peek_register(0xa4) == 0x35, "Frequency latch not reflected in viewer");
	// A mismatched bank write must leave the selected register untouched.
	write(chip, 8, 15); chip.write(0, 8); chip.write(3, 3);
	require(chip.peek_register(8) == 15, "Wrong-bank data write was accepted");
	// CPU ADPCM reads: two dummy reads, then RAM, EOS and limit wrapping.
	write(chip, 0x100, 1); write(chip, 0x101, 0xc0);
	write(chip, 0x102, 0); write(chip, 0x103, 0);
	write(chip, 0x104, 1); write(chip, 0x105, 0);
	write(chip, 0x10c, 2); write(chip, 0x10d, 0);
	write(chip, 0x100, 0x20); chip.write(2, 8);
	for (unsigned i = 0; i < 24; ++i)
	{
		check(chip, host); chip.read(3);
	}
	// Deliberately leave ADPCM flags unsynchronized with FM IRQ status.
	for (unsigned mask = 0; mask < 32; ++mask)
	{
		write(chip, 0x110, uint8_t(mask)); check(chip, host);
	}
	// Playback raises PCMBSY; clock it and check arbitrary intermediate states.
	write(chip, 0x109, 0xff); write(chip, 0x10a, 0xff);
	write(chip, 0x10b, 0xff); write(chip, 0x100, 0xa0);
	for (unsigned i = 0; i < 100; ++i)
	{
		ymfm::ym2608_reference::output_data samples[18]; chip.generate(samples, 18);
		check(chip, host);
	}
	// No destructive read fallback when the host has no debugger callback.
	struct NoPeek : ymfm::ymfm_interface {
		unsigned reads = 0;
		uint8_t ymfm_external_read(ymfm::access_class, uint32_t) override { ++reads; return 0; }
	} noPeek;
	ymfm::ym2608_reference isolated(noPeek); isolated.reset(); isolated.write_address(14);
	require(isolated.peek(1) == 0xff && noPeek.reads == 0, "Unsafe external read fallback");
	std::cout << checks << " states: all four ports match real reads, with no chip/host side effects.\n";
	std::cout << "Effective registers, GPIO, BUSY, ADPCM dummy reads/EOS/wrap/playback passed.\n";
}
