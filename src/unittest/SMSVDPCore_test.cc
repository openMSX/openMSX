#include "catch.hpp"
#include "SMSVDPCore.hh"

#include <array>
#include <cstdint>
#include <span>

using namespace openmsx;

namespace {

struct Bus {
	SMSVDPCore& core;
	int hpos = 100;
	void ctrl(uint8_t v) { core.writeControl(v, hpos); }
	void data(uint8_t v) { core.writeData(v); }
	void setReg(int r, uint8_t v) { ctrl(v); ctrl(uint8_t(0x80 | r)); }
	void vramWrite(uint16_t a, const uint8_t* d, int n) {
		ctrl(uint8_t(a & 0xff));
		ctrl(uint8_t(0x40 | ((a >> 8) & 0x3f)));
		for (int i = 0; i < n; ++i) data(d[i]);
	}
	void cramWrite(int index, uint8_t v) {
		ctrl(uint8_t(index & 0xff));
		ctrl(uint8_t(0xc0 | ((index >> 8) & 0x3f)));
		data(v);
	}
};

std::array<uint8_t, 320> renderLine(Bus& bus, int vpos, int outLine)
{
	// processLine must run first so the sprite selection matches.
	bus.core.processLine(vpos);
	std::array<uint8_t, 320> indices{};
	bus.core.drawLine(outLine, std::span<uint8_t, 320>(indices));
	return indices;
}

} // namespace

TEST_CASE("SMSVDPCore: register protocol")
{
	SMSVDPCore core(SMSVDPCore::Variant::SMS2, false);
	CHECK(core.getVariant() == SMSVDPCore::Variant::SMS2);
	CHECK(!core.isPal());

	Bus bus{core};
	bus.setReg(0x00, 0x04); // mode 4
	CHECK(core.peekMode() == 4);
	bus.setReg(0x02, 0x06);
	CHECK(core.peekReg(0x02) == 0x06);
	bus.setReg(0x01, 0x40); // display enable
	CHECK(core.peekReg(0x01) == 0x40);

	SMSVDPCore sms1(SMSVDPCore::Variant::SMS1, false);
	Bus bus1{sms1};
	bus1.setReg(0x00, 0x04);
	bus1.setReg(0x01, 0x40);
	sms1.writeControl(0x00, 100); // pending control write
	sms1.writeControl(0x88, 100); // odd register RMW path
	CHECK(sms1.peekReg(0x08) == 0x00);
}

TEST_CASE("SMSVDPCore: VRAM write/read with read-ahead")
{
	SMSVDPCore core(SMSVDPCore::Variant::SMS2, false);
	Bus bus{core};
	static const std::array<uint8_t, 16> pattern = {
		0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x23, 0x45, 0x67,
		0x89, 0xAB, 0xCD, 0xEF, 0x55, 0xAA, 0x0F, 0xF0
	};
	bus.vramWrite(0x0100, pattern.data(), 16);
	for (int i = 0; i < 16; ++i) {
		CHECK(core.peekVram(uint16_t(0x100 + i)) == pattern[i]);
	}

	bus.ctrl(0x00);
	bus.ctrl(0x01); // read setup at 0x0100 (read-ahead is loaded here)
	for (int i = 0; i < 16; ++i) {
		CHECK(core.readData() == pattern[i]);
	}
}

TEST_CASE("SMSVDPCore: V/H counters")
{
	SMSVDPCore core(SMSVDPCore::Variant::SMS2, false);
	// NTSC 192: active start = 3 + 13 + 27 = 43.
	CHECK(core.readVCounter(43, 100) == 0);
	CHECK(core.readVCounter(42, 100) == 0xff);
	CHECK(core.readVCounter(100, 100) == 57);
	CHECK(core.readVCounter(100, 10) == 56); // before hpos 23 uses previous line
	CHECK(core.peekMode() == 0); // counter read has no side effects

	core.latchHCounter(50);
	CHECK(core.readHCounter() == 1); // ((50-1-46)>>1) & 0xff
}

TEST_CASE("SMSVDPCore: VINT and /INT line")
{
	SMSVDPCore core(SMSVDPCore::Variant::SMS2, false);
	Bus bus{core};
	bus.setReg(0x00, 0x04);
	bus.setReg(0x01, 0x60); // display enable + VINT enable
	CHECK(((core.vintHpos() == 24) && (core.hintHpos() == 26) &&
	       (core.intHpos() == 26)));

	// NTSC 192: VINT is pending from vpos 236 on.
	core.processLine(236);
	core.updateInterrupts(core.vintHpos() - 1);
	CHECK(!core.intLineAsserted());
	core.updateInterrupts(core.intHpos());
	CHECK(core.intLineAsserted());
	const uint8_t status = core.readControl(30);
	CHECK((status & 0x80) != 0); // VINT status bit
	CHECK(!core.intLineAsserted()); // cleared by the status read
	CHECK((core.readControl(31) & 0xe0) == 0);
}

TEST_CASE("SMSVDPCore: HINT and /INT line")
{
	SMSVDPCore core(SMSVDPCore::Variant::SMS2, false);
	Bus bus{core};
	bus.setReg(0x00, 0x14); // mode 4 + HINT enable
	bus.setReg(0x01, 0x40); // display enable
	bus.setReg(0x0a, 0x00); // line counter -> HINT every line
	core.processLine(core.activeStart());
	core.updateInterrupts(core.hintHpos() - 1);
	CHECK(!core.intLineAsserted());
	core.updateInterrupts(core.intHpos());
	CHECK(core.intLineAsserted());
	const uint8_t status = core.readControl(30);
	CHECK((status & 0x80) == 0); // HINT does not set the VINT status bit
	CHECK(!core.intLineAsserted());
}

TEST_CASE("SMSVDPCore: /INT callback")
{
	SMSVDPCore core(SMSVDPCore::Variant::SMS2, false);
	int calls = 0;
	bool last = true;
	core.setNintCallback([&](bool asserted) { last = asserted; ++calls; });
	// Registering immediately synchronizes the listener.
	CHECK(calls == 1);
	CHECK(!last);

	Bus bus{core};
	bus.setReg(0x00, 0x04);
	bus.setReg(0x01, 0x60); // display enable + VINT enable
	core.processLine(236);  // VINT pending
	core.updateInterrupts(core.intHpos());
	CHECK(last); // asserted
	const int callsAfterAssert = calls;
	CHECK(callsAfterAssert >= 2);

	core.updateInterrupts(core.intHpos()); // same state, still notified
	CHECK(calls == callsAfterAssert + 1);
	CHECK(last);

	const uint8_t status = core.readControl(30);
	CHECK((status & 0x80) != 0);
	CHECK(!last); // cleared by the status read
}

TEST_CASE("SMSVDPCore: NTSC/PAL runtime switch")
{
	SMSVDPCore core(SMSVDPCore::Variant::SMS2, false);
	CHECK(((core.height() == 262) && (core.activeStart() == 43)));
	core.setPal(true);
	CHECK(((core.height() == 313) && (core.activeStart() == 70)));
	core.setPal(false);
	CHECK(((core.height() == 262) && (core.activeStart() == 43)));
}

TEST_CASE("SMSVDPCore: mode 4 scanline render")
{
	for (auto variant : {SMSVDPCore::Variant::SMS1, SMSVDPCore::Variant::SMS2}) {
		SMSVDPCore core(variant, false);
		Bus bus{core};
		bus.setReg(0x00, 0x04);    // mode 4
		bus.setReg(0x01, 0x40);    // display on
		bus.setReg(0x02, 0x06);    // name table base 0x1800
		bus.setReg(0x07, 0x00);    // backdrop = CRAM[0]
		bus.cramWrite(0x00, 0x00); // black
		bus.cramWrite(0x01, 0x3f); // white
		CHECK(core.peekCram(1) == 0x3f);
		// name table row 0: all 32 entries -> tile 1, no flags
		std::array<uint8_t, 64> nameTable;
		for (int i = 0; i < 32; ++i) {
			nameTable[2 * i] = 0x01;
			nameTable[2 * i + 1] = 0x00;
		}
		bus.vramWrite(0x1800, nameTable.data(), 64);
		// tile 1, row 0, plane0 = 0xff -> pen 1
		// (tile 0 pattern would overlap the sprite Y table at 0x0000)
		const uint8_t plane0 = 0xff;
		bus.vramWrite(0x0020, &plane0, 1);

		const auto indices = renderLine(bus, core.activeStart(), 24);
		// Mode 4: tile pen 1 maps to CRAM[1], the backdrop pen
		// (R#7 = 0 -> pen 0x10) maps to CRAM[0x10].
		const uint8_t pen1 = core.peekCram(1);
		const uint8_t backdrop = core.peekCram(0x10);
		CHECK(indices[32 + 0] == pen1);
		CHECK(indices[32 + 255] == pen1);
		CHECK(indices[0] == backdrop);
		CHECK(indices[319] == backdrop);
		// The chip color levels differ between the variants.
		const uint32_t white = (variant == SMSVDPCore::Variant::SMS1)
		                     ? 0xEEEEEEu : 0xFFFFFFu;
		CHECK(core.paletteColors()[63] == white);
	}
}

TEST_CASE("SMSVDPCore: sprite overflow and collision")
{
	{ // overflow: 9 sprites on the same line
		SMSVDPCore core(SMSVDPCore::Variant::SMS2, false);
		Bus bus{core};
		bus.setReg(0x00, 0x04);
		bus.setReg(0x01, 0x40);
		std::array<uint8_t, 9> ys;
		ys.fill(9);
		bus.vramWrite(0x0000, ys.data(), 9);
		for (int i = 0; i < 9; ++i) {
			const std::array<uint8_t, 2> e = {10, 0};
			bus.vramWrite(uint16_t(0x80 + 2 * i), e.data(), 2);
		}
		core.processLine(core.activeStart() + 10);
		core.checkPendingFlags(24);
		const uint8_t status = core.readControl(25);
		CHECK((status & 0x40) != 0); // sprite overflow
	}
	{ // collision: two sprites with a pixel at the same place
		SMSVDPCore core(SMSVDPCore::Variant::SMS2, false);
		Bus bus{core};
		bus.setReg(0x00, 0x04);
		bus.setReg(0x01, 0x40);
		const std::array<uint8_t, 2> ys = {9, 9};
		bus.vramWrite(0x0000, ys.data(), 2);
		const std::array<uint8_t, 4> e = {10, 1, 10, 1}; // X=10, tile=1
		bus.vramWrite(0x0080, e.data(), 4);
		const uint8_t plane0 = 0x80; // tile 1, row 0, pixel 0 set
		bus.vramWrite(0x0020, &plane0, 1);
		core.processLine(core.activeStart() + 10);
		std::array<uint8_t, 320> indices{};
		core.drawLine(24 + 10, std::span<uint8_t, 320>(indices));
		core.checkPendingFlags(100);
		const uint8_t status = core.readControl(101);
		CHECK((status & 0x20) != 0); // sprite collision
	}
}

TEST_CASE("SMSVDPCore: interrupt enable state")
{
	SMSVDPCore core(SMSVDPCore::Variant::SMS2, false);
	Bus bus{core};
	CHECK(!core.interruptsEnabled());
	bus.setReg(0x00, 0x14); // mode 4 + HINT enable
	CHECK(core.interruptsEnabled());
	bus.setReg(0x00, 0x04); // HINT off
	CHECK(!core.interruptsEnabled());
	bus.setReg(0x01, 0x60); // display + VINT enable
	CHECK(core.interruptsEnabled());
	bus.setReg(0x01, 0x40); // VINT off
	CHECK(!core.interruptsEnabled());
}

TEST_CASE("SMSVDPCore: side-effect-free peeks")
{
	SMSVDPCore core(SMSVDPCore::Variant::SMS2, false);
	Bus bus{core};
	bus.setReg(0x00, 0x04);
	bus.setReg(0x01, 0x60); // VINT enabled
	core.processLine(236);  // VINT pending

	// peekControl simulates the pending-flag promotion of a real read...
	CHECK((core.peekControl(30) & 0x80) != 0);
	// ...but does not clear anything: the real read still returns it.
	CHECK((core.readControl(30) & 0x80) != 0);
	CHECK((core.peekControl(31) & 0x80) == 0);

	// peekData returns the read-ahead value without consuming it.
	const uint8_t byte = 0x5a;
	bus.vramWrite(0x0000, &byte, 1);
	bus.ctrl(0x00);
	bus.ctrl(0x00); // read setup at 0x0000
	CHECK(core.peekData() == 0x5a);
	CHECK(core.peekData() == 0x5a);
	CHECK(core.readData() == 0x5a);

	// peekHCounter returns the latched value without clearing the latch.
	core.latchHCounter(50);
	CHECK(core.peekHCounter() == 1);
	CHECK(core.readHCounter() == 1);
}
