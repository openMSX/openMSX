#include "YM2608.hh"

#include "DummyAY8910Periphery.hh"

#include "DeviceConfig.hh"
#include "Clock.hh"
#include "outer.hh"
#include "serialize.hh"
#include "stl.hh"

#include "3rdparty/ym2608/fmopn_2608rom.h"

#include <algorithm>
#include <numbers>

namespace openmsx {

static constexpr unsigned CLOCK = 8'000'000;

static constexpr uint8_t STATUS_BUSY = 0x80;
static constexpr uint8_t STATUS_ADPCM_B_EOS = 0x04;
static constexpr uint8_t STATUS_ADPCM_B_BRDY = 0x08;
static constexpr uint8_t STATUS_ADPCM_B_PLAYING = 0x20;

YM2608::YM2608(DeviceConfig& config, std::string_view name, EmuTime time)
	: busyEnd(time)
	, fm(config.getMotherBoard(), name)
	, adpcmB(config, name)
	, registers(config.getMotherBoard(), name)
	, fmPart(config, name)
	, ssg(strCat(name, " SSG"), DummyAY8910Periphery::instance(), config, time,
		AY8910::Type::YM2149, 2'000'000.0f)
{
	// Maximum normalization. Standard SSG volume replaces the old trim.
	ssg.setSoftwareVolume(16382.0f * std::numbers::sqrt2_v<float> * (2.0f / 3.0f) / (32768.0f * 4.3f), time);

	updatePrescale(fm.clock_prescale());
	reset(time);
}

void YM2608::reset(EmuTime time)
{
	updateStream(time);

	// reset the engines
	fm.reset(time);
	adpcmA.reset();
	adpcmB.reset();

	// configure ADPCM percussion sounds; these are present in an embedded ROM
	adpcmA.set_start_end(0, 0x0000, 0x01bf); // bass drum
	adpcmA.set_start_end(1, 0x01c0, 0x043f); // snare drum
	adpcmA.set_start_end(2, 0x0440, 0x1b7f); // top cymbal
	adpcmA.set_start_end(3, 0x1b80, 0x1cff); // high hat
	adpcmA.set_start_end(4, 0x1d00, 0x1f7f); // tom tom
	adpcmA.set_start_end(5, 0x1f80, 0x1fff); // rim shot

	// initialize our special interrupt states, then read the upper status
	// register, which updates the IRQs
	irqEnable = 0x1f;
	flagControl = 0x1c;
	readStatusHi(time);

	ssg.reset(time);
	applyRates(time);
	busyEnd = time;
}

uint8_t YM2608::readPort(unsigned port, EmuTime time)
{
	updateStream(time);

	switch (port & 3) {
	case 0: // status port, YM2203 compatible
		return readStatus(time);
	case 1: // data port (only SSG)
		return readData(time);
	case 2: // status port, extended
		return readStatusHi(time);
	case 3: // ADPCM-B data
		return readDataHi();
	}
	UNREACHABLE;
}

uint8_t YM2608::peekPort(unsigned port, EmuTime time) const
{
	// Debugger reads deliberately bypass readStatusHi()'s IRQ update and the
	// ADPCM data port's dummy reads, address advancement and flag changes.
	auto busy = (time < busyEnd) ? STATUS_BUSY : 0;
	switch (port & 3) {
	case 0:
		return (fm.status() & (fm_engine::STATUS_TIMERA | fm_engine::STATUS_TIMERB)) | busy;
	case 1:
		if (addressLatch < 0x10) {
			return ssg.peekRegister(addressLatch, time);
		}
		return (addressLatch == 0xff) ? 1 : 0;
	case 2:
		return statusHi() | busy;
	case 3:
		return ((addressLatch & 0xff) < 0x10) ? adpcmB.peek(addressLatch & 0x0f) : 0;
	}
	UNREACHABLE;
}


uint8_t YM2608::readStatus(EmuTime time)
{
	uint8_t result = fm.status() & (fm_engine::STATUS_TIMERA | fm_engine::STATUS_TIMERB);
	if (isBusy(time)) {
		result |= STATUS_BUSY;
	}
	return result;
}

uint8_t YM2608::readData(EmuTime time)
{
	if (addressLatch < 0x10) {
		return ssg.readRegister(addressLatch & 0x0f, time);
	} else {
		return addressLatch == 0xff ? 1 : 0;
	}
}

uint8_t YM2608::readStatusHi(EmuTime time)
{
	uint8_t status = statusHi();

	// update the status so that IRQs are propagated
	fm.set_reset_status(status, ~status);

	// merge in the busy flag
	if (isBusy(time)) {
		status |= STATUS_BUSY;
	}
	return status;
}

uint8_t YM2608::statusHi() const
{
	// fetch regular status
	uint8_t status = fm.status() & ~(STATUS_ADPCM_B_EOS | STATUS_ADPCM_B_BRDY | STATUS_ADPCM_B_PLAYING);

	// fetch ADPCM-B status, and merge in the bits
	uint8_t adpcmStatus = adpcmB.status();
	if ((adpcmStatus & adpcm_b_channel::STATUS_EOS) != 0) {
		status |= STATUS_ADPCM_B_EOS;
	}
	if ((adpcmStatus & adpcm_b_channel::STATUS_BRDY) != 0) {
		status |= STATUS_ADPCM_B_BRDY;
	}
	if ((adpcmStatus & adpcm_b_channel::STATUS_PLAYING) != 0) {
		status |= STATUS_ADPCM_B_PLAYING;
	}

	// turn off any bits that have been requested to be masked
	status &= ~(flagControl & 0x1f);

	return status;
}

uint8_t YM2608::readDataHi()
{
	if ((addressLatch & 0xff) < 0x10) {
		return adpcmB.read(addressLatch & 0x0f);
	} else {
		return 0;
	}
}

void YM2608::writePort(unsigned port, uint8_t value, EmuTime time)
{
	switch (port & 3) {
	case 0: // lower address port
		addressLatch = value;
		// special case: update the prescale
		if (0x2d <= addressLatch && addressLatch <= 0x2f) {
			// These address writes change the clocks. Finish the preceding
			// interval at the old rates before rebuilding either resampler.
			updateStream(time);
			if (addressLatch == 0x2d) {
				updatePrescale(6);
			} else if (addressLatch == 0x2e && fm.clock_prescale() == 6) {
				updatePrescale(3);
			} else if (addressLatch == 0x2f) {
				updatePrescale(2);
			}
		}
		break;
	case 2: // upper address port
		addressLatch = 0x100 | value;
		break;
	case 1: // lower data port
		if (addressLatch & 0x100) break; // ignore if paired with upper address
		writeRegister(addressLatch, value, time);
		break;
	case 3: // upper data port
		if ((addressLatch & 0x100) == 0) break; // ignore if paired with lower address
		writeRegister(addressLatch, value, time);
		break;
	}

	applyRates(time);
}

void YM2608::writeRegister(unsigned regnum, uint8_t data, EmuTime time)
{
	updateStream(time);

	if (regnum < 0x10) {
		// 00-0F: write to SSG
		ssg.writeRegister(regnum, data, time);
	} else if (regnum < 0x20) {
		// 10-1F: write to ADPCM-A
		adpcmA.write(regnum & 0x0f, data);
	} else if (regnum == 0x29) {
		// 29: special IRQ mask register
		irqEnable = data;
		fm.set_irq_mask(irqEnable & ~flagControl & 0x1f);
	} else if (0x100 <= regnum && regnum < 0x110) {
		// 100-10F: write to ADPCM-B
		adpcmB.write(regnum & 0x0f, data);
	} else if (regnum == 0x110) {
		// 110: IRQ flag control
		if (data & 0x80) {
			fm.set_reset_status(0, 0xff);
		} else {
			flagControl = data;
			fm.set_irq_mask(irqEnable & ~flagControl & 0x1f);
		}
	} else {
		// 20-28, 2A-FF, 111-1FF: write to FM
		fm.write(regnum, data, time);
	}

	// mark busy for a bit
	setBusyEnd(time, 32 * fm.clock_prescale());
}

uint8_t YM2608::peekRegister(unsigned regnum, EmuTime time) const
{
	assert(regnum < 0x200);
	if (regnum < 0x10) {
		return ssg.peekRegister(regnum, time);
	} else if (regnum < 0x20) {
		return adpcmA.regs().read(regnum & 0x0f);
	} else if (regnum == 0x29) {
		return irqEnable;
	} else if (0x100 <= regnum && regnum < 0x110) {
		return adpcmB.regs().read(regnum & 0x0f);
	} else if (regnum == 0x110) {
		return flagControl;
	} else {
		return fm.regs().read(regnum);
	}
}

void YM2608::updateStream(EmuTime time)
{
	fmPart.updateStream(time);
}

void YM2608::updatePrescale(uint8_t prescale)
{
	fm.set_clock_prescale(prescale);
}

unsigned YM2608::prescale() const
{
	return fm.clock_prescale();
}

unsigned YM2608::fmRate() const
{
	return (CLOCK + 12 * prescale()) / (24 * prescale());
}

unsigned YM2608::ssgRate() const
{
	return CLOCK / (prescale() == 6 ? 32
	              : prescale() == 3 ? 16
	                                :  8);
}

void YM2608::applyRates(EmuTime time)
{
	fmPart.rate(fmRate());
	ssg.setClockFrequency(float(8 * ssgRate()), time);
}

void YM2608::setBusyEnd(EmuTime time, uint32_t clocks)
{
	busyEnd = time + Clock<CLOCK>::duration(clocks);
}

bool YM2608::isBusy(EmuTime time) const
{
	return time < busyEnd;
}

void YM2608::generateFM(std::span<float*> buffers, unsigned num)
{
	if (adpcmB.silent()) {
		buffers[6] = nullptr;
	}
	for (unsigned c = 0; c < 6; ++c) {
		if (adpcmA.silent(c)) {
			buffers[c + 7] = nullptr;
		}
	}
	// ADPCM-A replays this counter. generate() advances the engine's copy.
	const uint32_t env = fm.envelope_counter();
	// Bit 7 of register 0x29 enables FM channels 3-5. generate() nulls every
	// channel that is outside this mask or that prepare() finds already quiet.
	const uint32_t fmMask = (irqEnable & 0x80) ? 0x3f : 0x07;
	fm.generate(buffers.template first<6>(), num, fmMask);
	adpcmB.generate(buffers[6], num);
	adpcmA.generate(buffers.subspan(7, 6).template first<6>(), num, env);
}

template<typename Archive>
void YM2608::serialize(Archive& ar, unsigned /*version*/)
{
	if constexpr (!Archive::IS_LOADER) {
		updateStream(fm.getCurrentTime());
	}
	ar.serialize("address",     addressLatch,
	             "irqEnable",   irqEnable,
	             "flagControl", flagControl,
	             "fm",          fm,
	             "adpcmA",      adpcmA,
	             "adpcmB",      adpcmB,
	             "busyEnd",     busyEnd,
	             "ssg",         ssg);
	if constexpr (Archive::IS_LOADER) {
		applyRates(fm.getCurrentTime());
	}
}
INSTANTIATE_SERIALIZE_METHODS(YM2608);


YM2608::FmPart::FmPart(DeviceConfig& config, std::string_view name_)
	: ResampledSoundDevice(
		config.getMotherBoard(), name_,
		"Makoto FM, rhythm and ADPCM", 13, (CLOCK + 72) / 144, true)
{
	registerSound(config);
}

YM2608::FmPart::~FmPart()
{
	unregisterSound();
}

void YM2608::FmPart::updateStream(EmuTime time)
{
	SoundDevice::updateStream(time);
}

void YM2608::FmPart::rate(unsigned value)
{
	if (getInputRate() != value) {
		setInputRate(value);
		createResampler();
	}
}

void YM2608::FmPart::generateChannels(std::span<float*> buffers, unsigned num)
{
	assert(buffers.size() == 13);
	OUTER(YM2608, fmPart).generateFM(buffers, num);
}


YM2608::Registers::Registers(MSXMotherBoard& board, std::string_view name_)
	: SimpleDebuggable(board, strCat(name_, " registers"), "Effective YM2608 core registers", 512)
{
}

uint8_t YM2608::Registers::read(unsigned address, EmuTime time)
{
	return OUTER(YM2608, registers).peekRegister(address, time);
}

void YM2608::Registers::write(unsigned address, uint8_t value, EmuTime time)
{
	// Address selection itself has effects for 2Dh-2Fh. Match a normal
	// address/data pair, then preserve the running program's selection.
	auto& ym2608 = OUTER(YM2608, registers);
	auto savedAddress = ym2608.addressLatch;
	unsigned port = (address & 0x100) ? 2 : 0;
	ym2608.writePort(port, uint8_t(address), time);
	ym2608.writePort(port + 1, value, time);
	ym2608.addressLatch = savedAddress;
}


// --- FM engine tables and operators ------------------------------------------

//*********************************************************
//  GLOBAL TABLE LOOKUPS
//*********************************************************

//-------------------------------------------------
//  abs_sin_attenuation - given a sin (phase) input
//  where the range 0-2*PI is mapped onto 10 bits,
//  return the absolute value of sin(input),
//  logarithmically-adjusted and treated as an
//  attenuation value, in 4.8 fixed point format
//-------------------------------------------------

constexpr unsigned abs_sin_attenuation(unsigned input)
{
	// the values here are stored as 4.8 logarithmic values for 1/4 phase
	// this matches the internal format of the OPN chip, extracted from the die
	static constexpr uint16_t s_sin_table[256] =
	{
		0x859,0x6c3,0x607,0x58b,0x52e,0x4e4,0x4a6,0x471,0x443,0x41a,0x3f5,0x3d3,0x3b5,0x398,0x37e,0x365,
		0x34e,0x339,0x324,0x311,0x2ff,0x2ed,0x2dc,0x2cd,0x2bd,0x2af,0x2a0,0x293,0x286,0x279,0x26d,0x261,
		0x256,0x24b,0x240,0x236,0x22c,0x222,0x218,0x20f,0x206,0x1fd,0x1f5,0x1ec,0x1e4,0x1dc,0x1d4,0x1cd,
		0x1c5,0x1be,0x1b7,0x1b0,0x1a9,0x1a2,0x19b,0x195,0x18f,0x188,0x182,0x17c,0x177,0x171,0x16b,0x166,
		0x160,0x15b,0x155,0x150,0x14b,0x146,0x141,0x13c,0x137,0x133,0x12e,0x129,0x125,0x121,0x11c,0x118,
		0x114,0x10f,0x10b,0x107,0x103,0x0ff,0x0fb,0x0f8,0x0f4,0x0f0,0x0ec,0x0e9,0x0e5,0x0e2,0x0de,0x0db,
		0x0d7,0x0d4,0x0d1,0x0cd,0x0ca,0x0c7,0x0c4,0x0c1,0x0be,0x0bb,0x0b8,0x0b5,0x0b2,0x0af,0x0ac,0x0a9,
		0x0a7,0x0a4,0x0a1,0x09f,0x09c,0x099,0x097,0x094,0x092,0x08f,0x08d,0x08a,0x088,0x086,0x083,0x081,
		0x07f,0x07d,0x07a,0x078,0x076,0x074,0x072,0x070,0x06e,0x06c,0x06a,0x068,0x066,0x064,0x062,0x060,
		0x05e,0x05c,0x05b,0x059,0x057,0x055,0x053,0x052,0x050,0x04e,0x04d,0x04b,0x04a,0x048,0x046,0x045,
		0x043,0x042,0x040,0x03f,0x03e,0x03c,0x03b,0x039,0x038,0x037,0x035,0x034,0x033,0x031,0x030,0x02f,
		0x02e,0x02d,0x02b,0x02a,0x029,0x028,0x027,0x026,0x025,0x024,0x023,0x022,0x021,0x020,0x01f,0x01e,
		0x01d,0x01c,0x01b,0x01a,0x019,0x018,0x017,0x017,0x016,0x015,0x014,0x014,0x013,0x012,0x011,0x011,
		0x010,0x00f,0x00f,0x00e,0x00d,0x00d,0x00c,0x00c,0x00b,0x00a,0x00a,0x009,0x009,0x008,0x008,0x007,
		0x007,0x007,0x006,0x006,0x005,0x005,0x005,0x004,0x004,0x004,0x003,0x003,0x003,0x002,0x002,0x002,
		0x002,0x001,0x001,0x001,0x001,0x001,0x001,0x001,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000
	};

	// if the top bit is set, we're in the second half of the curve
	// which is a mirror image, so invert the index
	if (bitfield(input, 8)) {
		input = ~input;
	}

	// return the value from the table
	return s_sin_table[input & 0xff];
}

// 10-bit phase, sign in bit 15. Built once from abs_sin_attenuation().
static constexpr unsigned WAVEFORM_LENGTH = 0x400;
static constexpr auto s_waveform = generate_array<WAVEFORM_LENGTH>([](size_t index) {
	unsigned i = unsigned(index);
	return uint16_t(abs_sin_attenuation(i) | (bitfield(i, 9) << 15));
});


//-------------------------------------------------
//  attenuation_to_volume - given a 5.8 fixed point
//  logarithmic attenuation value, return a 13-bit
//  linear volume
//-------------------------------------------------

inline uint32_t attenuation_to_volume(uint32_t input)
{
	// the values here are 10-bit mantissas with an implied leading bit
	// this matches the internal format of the OPN chip, extracted from the die

	// as a nod to performance, the implicit 0x400 bit is pre-incorporated, and
	// the values are left-shifted by 2 so that a simple right shift is all that
	// is needed; also the order is reversed to save a NOT on the input
#define X(a) (((a) | 0x400) << 2)
	static uint16_t const s_power_table[256] =
	{
		X(0x3fa),X(0x3f5),X(0x3ef),X(0x3ea),X(0x3e4),X(0x3df),X(0x3da),X(0x3d4),
		X(0x3cf),X(0x3c9),X(0x3c4),X(0x3bf),X(0x3b9),X(0x3b4),X(0x3ae),X(0x3a9),
		X(0x3a4),X(0x39f),X(0x399),X(0x394),X(0x38f),X(0x38a),X(0x384),X(0x37f),
		X(0x37a),X(0x375),X(0x370),X(0x36a),X(0x365),X(0x360),X(0x35b),X(0x356),
		X(0x351),X(0x34c),X(0x347),X(0x342),X(0x33d),X(0x338),X(0x333),X(0x32e),
		X(0x329),X(0x324),X(0x31f),X(0x31a),X(0x315),X(0x310),X(0x30b),X(0x306),
		X(0x302),X(0x2fd),X(0x2f8),X(0x2f3),X(0x2ee),X(0x2e9),X(0x2e5),X(0x2e0),
		X(0x2db),X(0x2d6),X(0x2d2),X(0x2cd),X(0x2c8),X(0x2c4),X(0x2bf),X(0x2ba),
		X(0x2b5),X(0x2b1),X(0x2ac),X(0x2a8),X(0x2a3),X(0x29e),X(0x29a),X(0x295),
		X(0x291),X(0x28c),X(0x288),X(0x283),X(0x27f),X(0x27a),X(0x276),X(0x271),
		X(0x26d),X(0x268),X(0x264),X(0x25f),X(0x25b),X(0x257),X(0x252),X(0x24e),
		X(0x249),X(0x245),X(0x241),X(0x23c),X(0x238),X(0x234),X(0x230),X(0x22b),
		X(0x227),X(0x223),X(0x21e),X(0x21a),X(0x216),X(0x212),X(0x20e),X(0x209),
		X(0x205),X(0x201),X(0x1fd),X(0x1f9),X(0x1f5),X(0x1f0),X(0x1ec),X(0x1e8),
		X(0x1e4),X(0x1e0),X(0x1dc),X(0x1d8),X(0x1d4),X(0x1d0),X(0x1cc),X(0x1c8),
		X(0x1c4),X(0x1c0),X(0x1bc),X(0x1b8),X(0x1b4),X(0x1b0),X(0x1ac),X(0x1a8),
		X(0x1a4),X(0x1a0),X(0x19c),X(0x199),X(0x195),X(0x191),X(0x18d),X(0x189),
		X(0x185),X(0x181),X(0x17e),X(0x17a),X(0x176),X(0x172),X(0x16f),X(0x16b),
		X(0x167),X(0x163),X(0x160),X(0x15c),X(0x158),X(0x154),X(0x151),X(0x14d),
		X(0x149),X(0x146),X(0x142),X(0x13e),X(0x13b),X(0x137),X(0x134),X(0x130),
		X(0x12c),X(0x129),X(0x125),X(0x122),X(0x11e),X(0x11b),X(0x117),X(0x114),
		X(0x110),X(0x10c),X(0x109),X(0x106),X(0x102),X(0x0ff),X(0x0fb),X(0x0f8),
		X(0x0f4),X(0x0f1),X(0x0ed),X(0x0ea),X(0x0e7),X(0x0e3),X(0x0e0),X(0x0dc),
		X(0x0d9),X(0x0d6),X(0x0d2),X(0x0cf),X(0x0cc),X(0x0c8),X(0x0c5),X(0x0c2),
		X(0x0be),X(0x0bb),X(0x0b8),X(0x0b5),X(0x0b1),X(0x0ae),X(0x0ab),X(0x0a8),
		X(0x0a4),X(0x0a1),X(0x09e),X(0x09b),X(0x098),X(0x094),X(0x091),X(0x08e),
		X(0x08b),X(0x088),X(0x085),X(0x082),X(0x07e),X(0x07b),X(0x078),X(0x075),
		X(0x072),X(0x06f),X(0x06c),X(0x069),X(0x066),X(0x063),X(0x060),X(0x05d),
		X(0x05a),X(0x057),X(0x054),X(0x051),X(0x04e),X(0x04b),X(0x048),X(0x045),
		X(0x042),X(0x03f),X(0x03c),X(0x039),X(0x036),X(0x033),X(0x030),X(0x02d),
		X(0x02a),X(0x028),X(0x025),X(0x022),X(0x01f),X(0x01c),X(0x019),X(0x016),
		X(0x014),X(0x011),X(0x00e),X(0x00b),X(0x008),X(0x006),X(0x003),X(0x000)
	};
#undef X

	// look up the fractional part, then shift by the whole
	return s_power_table[input & 0xff] >> (input >> 8);
}


//-------------------------------------------------
//  attenuation_increment - given a 6-bit ADSR
//  rate value and a 3-bit stepping index,
//  return a 4-bit increment to the attenutaion
//  for this step (or for the attack case, the
//  fractional scale factor to decrease by)
//-------------------------------------------------

inline uint32_t attenuation_increment(uint32_t rate, uint32_t index)
{
	static uint32_t const s_increment_table[64] =
	{
		0x00000000, 0x00000000, 0x10101010, 0x10101010,  // 0-3    (0x00-0x03)
		0x10101010, 0x10101010, 0x11101110, 0x11101110,  // 4-7    (0x04-0x07)
		0x10101010, 0x10111010, 0x11101110, 0x11111110,  // 8-11   (0x08-0x0B)
		0x10101010, 0x10111010, 0x11101110, 0x11111110,  // 12-15  (0x0C-0x0F)
		0x10101010, 0x10111010, 0x11101110, 0x11111110,  // 16-19  (0x10-0x13)
		0x10101010, 0x10111010, 0x11101110, 0x11111110,  // 20-23  (0x14-0x17)
		0x10101010, 0x10111010, 0x11101110, 0x11111110,  // 24-27  (0x18-0x1B)
		0x10101010, 0x10111010, 0x11101110, 0x11111110,  // 28-31  (0x1C-0x1F)
		0x10101010, 0x10111010, 0x11101110, 0x11111110,  // 32-35  (0x20-0x23)
		0x10101010, 0x10111010, 0x11101110, 0x11111110,  // 36-39  (0x24-0x27)
		0x10101010, 0x10111010, 0x11101110, 0x11111110,  // 40-43  (0x28-0x2B)
		0x10101010, 0x10111010, 0x11101110, 0x11111110,  // 44-47  (0x2C-0x2F)
		0x11111111, 0x21112111, 0x21212121, 0x22212221,  // 48-51  (0x30-0x33)
		0x22222222, 0x42224222, 0x42424242, 0x44424442,  // 52-55  (0x34-0x37)
		0x44444444, 0x84448444, 0x84848484, 0x88848884,  // 56-59  (0x38-0x3B)
		0x88888888, 0x88888888, 0x88888888, 0x88888888   // 60-63  (0x3C-0x3F)
	};
	return bitfield(s_increment_table[rate], 4*index, 4);
}


//-------------------------------------------------
//  detune_adjustment - given a 5-bit key code
//  value and a 3-bit detune parameter, return a
//  6-bit signed phase displacement; this table
//  has been verified against Nuked's equations,
//  but the equations are rather complicated, so
//  we'll keep the simplicity of the table
//-------------------------------------------------

inline int32_t detune_adjustment(uint32_t detune, uint32_t keycode)
{
	static uint8_t const s_detune_adjustment[32][4] =
	{
		{ 0,  0,  1,  2 },  { 0,  0,  1,  2 },  { 0,  0,  1,  2 },  { 0,  0,  1,  2 },
		{ 0,  1,  2,  2 },  { 0,  1,  2,  3 },  { 0,  1,  2,  3 },  { 0,  1,  2,  3 },
		{ 0,  1,  2,  4 },  { 0,  1,  3,  4 },  { 0,  1,  3,  4 },  { 0,  1,  3,  5 },
		{ 0,  2,  4,  5 },  { 0,  2,  4,  6 },  { 0,  2,  4,  6 },  { 0,  2,  5,  7 },
		{ 0,  2,  5,  8 },  { 0,  3,  6,  8 },  { 0,  3,  6,  9 },  { 0,  3,  7, 10 },
		{ 0,  4,  8, 11 },  { 0,  4,  8, 12 },  { 0,  4,  9, 13 },  { 0,  5, 10, 14 },
		{ 0,  5, 11, 16 },  { 0,  6, 12, 17 },  { 0,  6, 13, 19 },  { 0,  7, 14, 20 },
		{ 0,  8, 16, 22 },  { 0,  8, 16, 22 },  { 0,  8, 16, 22 },  { 0,  8, 16, 22 }
	};
	int32_t result = s_detune_adjustment[keycode][detune & 3];
	return bitfield(detune, 2) ? -result : result;
}


//-------------------------------------------------
//  opn_lfo_pm_phase_adjustment - given the 7 most
//  significant frequency number bits, plus a 3-bit
//  PM depth value and a signed 5-bit raw PM value,
//  return a signed PM adjustment to the frequency;
//  algorithm written to match Nuked behavior
//-------------------------------------------------

inline int32_t opn_lfo_pm_phase_adjustment(uint32_t fnum_bits, uint32_t pm_sensitivity, int32_t lfo_raw_pm)
{
	// this table encodes 2 shift values to apply to the top 7 bits
	// of fnum; it is effectively a cheap multiply by a constant
	// value containing 0-2 bits
	static uint8_t const s_lfo_pm_shifts[8][8] =
	{
		{ 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77 },
		{ 0x77, 0x77, 0x77, 0x77, 0x72, 0x72, 0x72, 0x72 },
		{ 0x77, 0x77, 0x77, 0x72, 0x72, 0x72, 0x17, 0x17 },
		{ 0x77, 0x77, 0x72, 0x72, 0x17, 0x17, 0x12, 0x12 },
		{ 0x77, 0x77, 0x72, 0x17, 0x17, 0x17, 0x12, 0x07 },
		{ 0x77, 0x77, 0x17, 0x12, 0x07, 0x07, 0x02, 0x01 },
		{ 0x77, 0x77, 0x17, 0x12, 0x07, 0x07, 0x02, 0x01 },
		{ 0x77, 0x77, 0x17, 0x12, 0x07, 0x07, 0x02, 0x01 }
	};

	// look up the relevant shifts
	int32_t abs_pm = (lfo_raw_pm < 0) ? -lfo_raw_pm : lfo_raw_pm;
	uint32_t const shifts = s_lfo_pm_shifts[pm_sensitivity][bitfield(abs_pm, 0, 3)];

	// compute the adjustment
	int32_t adjust = (fnum_bits >> bitfield(shifts, 0, 4)) + (fnum_bits >> bitfield(shifts, 4, 4));
	if (pm_sensitivity > 5) {
		adjust <<= pm_sensitivity - 5;
	}
	adjust >>= 2;

	// every 16 cycles it inverts sign
	return (lfo_raw_pm < 0) ? -adjust : adjust;
}



//*********************************************************
//  FM OPERATOR
//*********************************************************

//-------------------------------------------------
//  reset - reset the operator state
//-------------------------------------------------

void fm_operator::reset()
{
	m_phase = 0;
	m_env_attenuation = 0x3ff;
	m_env_state = EG_RELEASE;
	m_ssg_inverted = false;
	m_key_state = false;
	m_keyon_live = 0;
}

//-------------------------------------------------
//  prepare - prepare for clocking
//-------------------------------------------------

bool fm_operator::prepare(opna_registers& regs, unsigned choffs, unsigned opoffs)
{
	// cache the data
	regs.cache_operator_data(choffs, opoffs, m_cache);

	// clock the key state
	clock_keystate(m_keyon_live != 0);
	m_keyon_live &= ~(1 << KEYON_CSM);

	// Only an enabled SSG-EG can invert. Turning it off mid-note leaves the
	// flag set, so it is cleared here, after start_release() has had its look
	// at the old value.
	if (!m_cache.ssg_eg_enable) {
		m_ssg_inverted = false;
	}

	// we're active until we're quiet after the release
	return (m_env_state != EG_RELEASE || m_env_attenuation < EG_QUIET);
}


//-------------------------------------------------
//  clock - master clocking function
//-------------------------------------------------

void fm_operator::clock(uint32_t env_counter, int32_t lfo_raw_pm)
{
	// clock the SSG-EG state (OPN/OPNA); prepare() cleared the inversion if
	// it is disabled
	if (m_cache.ssg_eg_enable) {
		clock_ssg_eg_state();
	}

	// clock the envelope if on an envelope cycle; env_counter is a x.2 value
	if (bitfield(env_counter, 0, 2) == 0) {
		clock_envelope(env_counter >> 2);
	}

	// clock the phase
	clock_phase(lfo_raw_pm);
}


//-------------------------------------------------
//  compute_volume - compute the 14-bit signed
//  volume of this operator, given a phase
//  modulation and an AM LFO offset
//-------------------------------------------------

int32_t fm_operator::compute_volume(uint32_t phase, uint32_t am_offset) const
{
	// the low 10 bits of phase represents a full 2*PI period over
	// the full sin wave

	// early out if the envelope is effectively off
	if (m_env_attenuation > EG_QUIET) return 0;

	// get the absolute value of the sin, as attenuation, as a 4.8 fixed point value
	uint32_t sin_attenuation = s_waveform[phase & (WAVEFORM_LENGTH - 1)];

	// get the attenuation from the evelope generator as a 4.6 value, shifted up to 4.8
	uint32_t env_attenuation = envelope_attenuation(am_offset) << 2;

	// combine into a 5.8 value, then convert from attenuation to 13-bit linear volume
	int32_t result = attenuation_to_volume((sin_attenuation & 0x7fff) + env_attenuation);

	// negate if in the negative part of the sin wave (sign bit gives 14 bits)
	return bitfield(sin_attenuation, 15) ? -result : result;
}


//-------------------------------------------------
//  keyonoff - signal a key on/off event
//-------------------------------------------------

void fm_operator::keyonoff(bool on, keyon_type type)
{
	m_keyon_live = (m_keyon_live & ~(1 << type)) | (uint8_t(on) << type);
}


//-------------------------------------------------
//  start_attack - start the attack phase; called
//  when a keyon happens or when an SSG-EG cycle
//  is complete and restarts
//-------------------------------------------------

void fm_operator::start_attack(bool is_restart)
{
	// don't change anything if already in attack state
	if (m_env_state == EG_ATTACK) return;
	m_env_state = EG_ATTACK;

	// generally not inverted at start, except if SSG-EG is enabled and
	// one of the inverted modes is specified; leave this alone on a
	// restart, as it is managed by the clock_ssg_eg_state() code
	if (!is_restart) {
		m_ssg_inverted = m_cache.ssg_eg_enable && bitfield(m_cache.ssg_eg_mode, 2);
	}

	// reset the phase when we start an attack due to a key on
	// (but not when due to an SSG-EG restart except in certain cases
	// managed directly by the SSG-EG code)
	if (!is_restart) {
		m_phase = 0;
	}

	// if the attack rate >= 62 then immediately go to max attenuation
	if (m_cache.eg_rate[EG_ATTACK] >= 62) {
		m_env_attenuation = 0;
	}
}


//-------------------------------------------------
//  start_release - start the release phase;
//  called when a keyoff happens
//-------------------------------------------------

void fm_operator::start_release()
{
	// don't change anything if already in release state
	if (m_env_state >= EG_RELEASE) return;
	m_env_state = EG_RELEASE;

	// if attenuation if inverted due to SSG-EG, snap the inverted attenuation
	// as the starting point
	if (m_ssg_inverted) {
		m_env_attenuation = (0x200 - m_env_attenuation) & 0x3ff;
		m_ssg_inverted = false;
	}
}


//-------------------------------------------------
//  clock_keystate - clock the keystate to match
//  the incoming keystate
//-------------------------------------------------

void fm_operator::clock_keystate(bool keystate)
{
	// has the key changed?
	if (keystate != m_key_state) {
		m_key_state = keystate;

		// if the key has turned on, start the attack
		if (keystate) {
			start_attack();
		} else {
			// otherwise, start the release
			start_release();
		}
	}
}


//-------------------------------------------------
//  clock_ssg_eg_state - clock the SSG-EG state;
//  should only be called if SSG-EG is enabled
//-------------------------------------------------

void fm_operator::clock_ssg_eg_state()
{
	// work only happens once the attenuation crosses above 0x200
	if (!bitfield(m_env_attenuation, 9)) return;

	// 8 SSG-EG modes:
	//    000: repeat normally
	//    001: run once, hold low
	//    010: repeat, alternating between inverted/non-inverted
	//    011: run once, hold high
	//    100: inverted repeat normally
	//    101: inverted run once, hold low
	//    110: inverted repeat, alternating between inverted/non-inverted
	//    111: inverted run once, hold high
	uint32_t mode = m_cache.ssg_eg_mode;

	// hold modes (1/3/5/7)
	if (bitfield(mode, 0)) {
		// set the inverted flag to the end state (0 for modes 1/7, 1 for modes 3/5)
		m_ssg_inverted = bitfield(mode, 2) ^ bitfield(mode, 1);

		// if holding, force the attenuation to the expected value once we're
		// past the attack phase
		if (m_env_state != EG_ATTACK) {
			m_env_attenuation = m_ssg_inverted ? 0x200 : 0x3ff;
		}
	} else {
		// continuous modes (0/2/4/6)
		// toggle invert in alternating mode (even in attack state)
		m_ssg_inverted ^= bitfield(mode, 1);

		// restart attack if in decay/sustain states
		if (m_env_state == EG_DECAY || m_env_state == EG_SUSTAIN) {
			start_attack(true);
		}

		// phase is reset to 0 in modes 0/4
		if (bitfield(mode, 1) == 0) {
			m_phase = 0;
		}
	}

	// in all modes, once we hit release state, attenuation is forced to maximum
	if (m_env_state == EG_RELEASE) {
		m_env_attenuation = 0x3ff;
	}
}


//-------------------------------------------------
//  clock_envelope - clock the envelope state
//  according to the given count
//-------------------------------------------------

void fm_operator::clock_envelope(uint32_t env_counter)
{
	// handle attack->decay transitions
	if (m_env_state == EG_ATTACK && m_env_attenuation == 0) {
		m_env_state = EG_DECAY;
	}

	// handle decay->sustain transitions; it is important to do this immediately
	// after the attack->decay transition above in the event that the sustain level
	// is set to 0 (in which case we will skip right to sustain without doing any
	// decay); as an example where this can be heard, check the cymbals sound
	// in channel 0 of shinobi's test mode sound #5
	if (m_env_state == EG_DECAY && m_env_attenuation >= m_cache.eg_sustain) {
		m_env_state = EG_SUSTAIN;
	}

	// fetch the appropriate 6-bit rate value from the cache
	uint32_t rate = m_cache.eg_rate[m_env_state];

	// compute the rate shift value; this is the shift needed to
	// apply to the env_counter such that it becomes a 5.11 fixed
	// point number
	uint32_t rate_shift = rate >> 2;
	env_counter <<= rate_shift;

	// see if the fractional part is 0; if not, it's not time to clock
	if (bitfield(env_counter, 0, 11) != 0) return;

	// determine the increment based on the non-fractional part of env_counter
	uint32_t relevant_bits = bitfield(env_counter, (rate_shift <= 11) ? 11 : rate_shift, 3);
	uint32_t increment = attenuation_increment(rate, relevant_bits);

	// attack is the only one that increases
	if (m_env_state == EG_ATTACK) {
		// glitch means that attack rates of 62/63 don't increment if
		// changed after the initial key on (where they are handled
		// specially); nukeykt confirms this happens on OPM, OPN, OPL/OPLL
		// at least so assuming it is true for everyone
		if (rate < 62) {
			m_env_attenuation += (~m_env_attenuation * increment) >> 4;
		}
	} else {
		// all other cases are similar
		// non-SSG-EG cases just apply the increment
		if (!m_cache.ssg_eg_enable) {
			m_env_attenuation += increment;
		} else if (m_env_attenuation < 0x200) {
			// SSG-EG only applies if less than mid-point, and then at 4x
			m_env_attenuation += 4 * increment;
		}

		// clamp the final attenuation
		if (m_env_attenuation >= 0x400) {
			m_env_attenuation = 0x3ff;
		}
	}
}


//-------------------------------------------------
//  clock_phase - clock the 10.10 phase value; the
//  OPN version of the logic has been verified
//  against the Nuked phase generator
//-------------------------------------------------

void fm_operator::clock_phase(int32_t lfo_raw_pm)
{
	// read from the cache, or recalculate if PM active
	uint32_t phase_step = m_cache.phase_step;
	if (phase_step == opdata_cache::PHASE_STEP_DYNAMIC) {
		phase_step = opna_registers::compute_phase_step(m_cache, lfo_raw_pm);
	}

	// finally apply the step to the current phase value
	m_phase += phase_step;
}


//-------------------------------------------------
//  envelope_attenuation - return the effective
//  attenuation of the envelope
//-------------------------------------------------

uint32_t fm_operator::envelope_attenuation(uint32_t am_offset) const
{
	uint32_t result = m_env_attenuation;

	// invert if necessary due to SSG-EG
	if (m_ssg_inverted) {
		result = (0x200 - result) & 0x3ff;
	}

	// add in LFO AM modulation
	if (m_cache.lfo_am_enable) {
		result += am_offset;
	}

	// add in total level from the cache
	result += m_cache.total_level;

	// clamp to max and return
	return std::min<uint32_t>(result, 0x3ff);
}



//*********************************************************
//  FM CHANNEL
//*********************************************************

//-------------------------------------------------
//  reset - reset the channel state
//-------------------------------------------------

void fm_channel::reset()
{
	// reset our data
	m_feedback[0] = m_feedback[1] = 0;
	m_feedback_in = 0;
	for (auto& op : m_op) {
		op.reset();
	}
}


//-------------------------------------------------
//  keyonoff - signal key on/off to our operators
//-------------------------------------------------

void fm_channel::keyonoff(uint32_t states, keyon_type type)
{
	for (unsigned opnum = 0; opnum < m_op.size(); opnum++) {
		m_op[opnum].keyonoff(bitfield(states, opnum) != 0, type);
	}
}


//-------------------------------------------------
//  prepare - prepare for clocking
//-------------------------------------------------

bool fm_channel::prepare(opna_registers& regs, unsigned chnum)
{
	// prepare all operators and determine if any of them is active
	bool active = false;
	const unsigned choffs = opna_registers::channel_offset(chnum);
	auto const& map = opna_registers::OPERATOR_MAP[chnum];
	for (unsigned slot = 0; slot < 4; slot++) {
		if (m_op[slot].prepare(regs, choffs, opna_registers::operator_offset(map[slot]))) {
			active = true;
		}
	}

	return active;
}


//-------------------------------------------------
//  clock - master clock of all operators
//-------------------------------------------------

void fm_channel::clock(uint32_t env_counter, int32_t lfo_raw_pm)
{
	// clock the feedback through
	m_feedback[0] = m_feedback[1];
	m_feedback[1] = m_feedback_in;

	for (auto& op : m_op) {
		op.clock(env_counter, lfo_raw_pm);
	}
}


//-------------------------------------------------
//  s_algorithm_ops - operator routing per algorithm
//
//  OPNA offers 8 connection algorithms for its 4 operators.
//
//  The operators are computed in order, with the inputs pulled from
//  an array of values (opout) that is populated as we go:
//     0 = 0
//     1 = O1
//     2 = O2
//     3 = O3
//     4 = (O4)
//     5 = O1+O2
//     6 = O1+O3
//     7 = O2+O3
//
//  Each row lists the opout[] index that feeds operators 2, 3 and 4,
//  and a mask of which of operators 1-3 also go to the output (operator 4
//  always does).
//-------------------------------------------------

#define ALGORITHM(op2in, op3in, op4in, op1out, op2out, op3out) \
	{ uint8_t(op2in), uint8_t(op3in), uint8_t(op4in), \
	  uint8_t((op1out) | ((op2out) << 1) | ((op3out) << 2)) }
static constexpr struct { uint8_t op2in, op3in, op4in, carrier_mask; } s_algorithm_ops[8] =
{
	ALGORITHM(1,2,3, 0,0,0),    //  0: O1 -> O2 -> O3 -> O4 -> out (O4)
	ALGORITHM(0,5,3, 0,0,0),    //  1: (O1 + O2) -> O3 -> O4 -> out (O4)
	ALGORITHM(0,2,6, 0,0,0),    //  2: (O1 + (O2 -> O3)) -> O4 -> out (O4)
	ALGORITHM(1,0,7, 0,0,0),    //  3: ((O1 -> O2) + O3) -> O4 -> out (O4)
	ALGORITHM(1,0,3, 0,1,0),    //  4: ((O1 -> O2) + (O3 -> O4)) -> out (O2+O4)
	ALGORITHM(1,1,1, 0,1,1),    //  5: ((O1 -> O2) + (O1 -> O3) + (O1 -> O4)) -> out (O2+O3+O4)
	ALGORITHM(1,0,0, 0,1,1),    //  6: ((O1 -> O2) + O3 + O4) -> out (O2+O3+O4)
	ALGORITHM(0,0,0, 1,1,1)     //  7: (O1 + O2 + O3 + O4) -> out (O1+O2+O3+O4)
};
#undef ALGORITHM


//-------------------------------------------------
//  make_output_plan - read the register fields that
//  hold for the whole buffer
//-------------------------------------------------

fm_channel::output_plan fm_channel::make_output_plan(const opna_registers& regs, unsigned chnum) const
{
	output_plan plan;
	const unsigned choffs = opna_registers::channel_offset(chnum);
	auto const& alg = s_algorithm_ops[regs.ch_algorithm(choffs)];
	plan.op2in = alg.op2in;
	plan.op3in = alg.op3in;
	plan.op4in = alg.op4in;
	plan.carrier_mask = alg.carrier_mask;
	plan.feedback = uint8_t(regs.ch_feedback(choffs));

	plan.output_mask = 0;
	if (regs.ch_output_0(choffs)) {
		plan.output_mask |= 1;
	}
	if (regs.ch_output_1(choffs)) {
		plan.output_mask |= 2;
	}
	return plan;
}


//-------------------------------------------------
//  output_4op - combine 4 operators according to
//  the specified algorithm, returning a sum
//-------------------------------------------------

int32_t fm_channel::output_4op(const output_plan& plan, uint32_t am_offset)
{
	// operator 1 has optional self-feedback
	int32_t opmod = 0;
	if (plan.feedback != 0) {
		opmod = (m_feedback[0] + m_feedback[1]) >> (10 - plan.feedback);
	}

	// compute the 14-bit volume/value of operator 1 and update the feedback
	int32_t op1value = m_feedback_in = m_op[0].compute_volume(m_op[0].phase() + opmod, am_offset);

	// now that the feedback has been computed, skip the rest if this channel
	// feeds no output; no need to do all this work for nothing
	if (plan.output_mask == 0) return 0;

	// populate the opout table
	int16_t opout[8];
	opout[0] = 0;
	opout[1] = op1value;

	// compute the 14-bit volume/value of operator 2
	opmod = opout[plan.op2in] >> 1;
	opout[2] = m_op[1].compute_volume(m_op[1].phase() + opmod, am_offset);
	opout[5] = opout[1] + opout[2];

	// compute the 14-bit volume/value of operator 3
	opmod = opout[plan.op3in] >> 1;
	opout[3] = m_op[2].compute_volume(m_op[2].phase() + opmod, am_offset);
	opout[6] = opout[1] + opout[3];
	opout[7] = opout[2] + opout[3];

	// compute the 14-bit volume/value of operator 4;
	// all algorithms consume OP4 output at a minimum
	opmod = opout[plan.op4in] >> 1;
	int32_t result = m_op[3].compute_volume(m_op[3].phase() + opmod, am_offset) >> OUTPUT_SHIFT;

	// optionally add OP1, OP2, OP3. compute_volume() cannot exceed the largest
	// power table entry, 8168, so even four unshifted carriers stay inside the
	// 16-bit range that the clamp here used to enforce.
	if (plan.carrier_mask & 1) {
		result += opout[1] >> OUTPUT_SHIFT;
	}
	if (plan.carrier_mask & 2) {
		result += opout[2] >> OUTPUT_SHIFT;
	}
	if (plan.carrier_mask & 4) {
		result += opout[3] >> OUTPUT_SHIFT;
	}

	return result;
}


//*********************************************************
//  FM ENGINE
//*********************************************************

static constexpr uint8_t DEFAULT_PRESCALE = 6;

//-------------------------------------------------
//  fm_engine - constructor
//-------------------------------------------------

fm_engine::fm_engine(MSXMotherBoard& motherboard, std::string_view name) :
	irq(motherboard, strCat(name, ".IRQ")),
	timers{Timer(motherboard.getScheduler(), 0),
	       Timer(motherboard.getScheduler(), 1)},
	m_env_counter(0),
	m_status(0),
	m_clock_prescale(DEFAULT_PRESCALE),
	m_irq_mask(STATUS_TIMERA | STATUS_TIMERB),
	m_timer_running{false, false},
	m_total_clocks(0),
	m_modified(false)
{
}


//-------------------------------------------------
//  reset - reset the overall state
//-------------------------------------------------

void fm_engine::reset(EmuTime time)
{
	for (auto& timer : timers) {
		timer.cancel();
	}

	// reset all status bits
	set_reset_status(0, 0xff);
	irq.reset();

	// register type-specific initialization
	m_regs.reset();

	// explicitly write to the mode register since it has side-effects
	// QUESTION: old cores initialize this to 0x30 -- who is right?
	write(opna_registers::REG_MODE, 0, time);

	// reset the channels
	for (auto& chan : m_channel) {
		chan.reset();
	}
}


//-------------------------------------------------
//  synthesize_fm_channel - clock one channel across a stretch of samples
//  with no prepare() in between. Write selects whether output is mixed.
//-------------------------------------------------

template<bool Write, bool Lfo>
static void synthesize_fm_channel(fm_channel& channel, opna_registers& regs,
                                  float* buf [[maybe_unused]], unsigned num, uint32_t env,
                                  unsigned chnum)
{
	// Registers hold for the whole buffer, the AM shift among them. Without
	// the LFO walk the AM offset is constant as well, so it is read once too.
	fm_channel::output_plan plan;
	uint32_t lfoMaxCount = 0;
	uint32_t am_shift = 0;
	uint32_t am_offset = 0;
	bool panLeft = false;
	bool panRight = false;
	if constexpr (Lfo) {
		lfoMaxCount = regs.lfo_max_count();
	}
	if constexpr (Write) {
		plan = channel.make_output_plan(regs, chnum);
		panLeft = (plan.output_mask & 1) != 0;
		panRight = (plan.output_mask & 2) != 0;
		am_shift = regs.lfo_am_shift(opna_registers::channel_offset(chnum));
		if constexpr (!Lfo) {
			am_offset = regs.lfo_am_offset(am_shift);
		}
	}

	auto tick = [&] {
		env = step_eg_counter(env);
		int32_t pm = 0;
		if constexpr (Lfo) {
			pm = regs.clock_lfo(lfoMaxCount);
			if constexpr (Write) {
				am_offset = regs.lfo_am_offset(am_shift);
			}
		}
		channel.clock(env, pm);
	};

	if constexpr (!Write) {
		for (unsigned index = 0; index < num; ++index) {
			tick();
		}
		return;
	}

	if (panLeft && panRight) {
		for (unsigned index = 0; index < num; ++index) {
			tick();
			float value = float(channel.output_4op(plan, am_offset));
			unsigned pos = index * 2;
			buf[pos + 0] += value;
			buf[pos + 1] += value;
		}
	} else if (panLeft) {
		for (unsigned index = 0; index < num; ++index) {
			tick();
			buf[index * 2] += float(channel.output_4op(plan, am_offset));
		}
	} else if (panRight) {
		for (unsigned index = 0; index < num; ++index) {
			tick();
			buf[index * 2 + 1] += float(channel.output_4op(plan, am_offset));
		}
	} else {
		for (unsigned index = 0; index < num; ++index) {
			tick();
			channel.output_4op(plan, am_offset);
		}
	}
}


//-------------------------------------------------
//  generate - one channel for the whole buffer, then the next
//-------------------------------------------------

void fm_engine::generate(std::span<float*, CHANNELS> buffers, unsigned num, uint32_t chanmask)
{
	static_assert(OPERATORS / CHANNELS == 4);

	// An empty buffer must not consume a pending key-on.
	if (num == 0) return;

	// Register writes and CSM key-on land before this call. A channel that is
	// quiet here cannot become active before the next generate(). The envelope
	// counter advances once per sample for ADPCM-A, even when no channel clocks.
	const uint32_t env0 = m_env_counter;
	const auto lfo0 = m_regs.save_lfo();
	const bool lfoEnabled = m_regs.lfo_enable();
	// Disabled LFO is a constant. One update serves every sample of the chunk.
	if (!lfoEnabled) {
		m_regs.hold_disabled_lfo();
	}

	bool lfoWalked = false;
	for (unsigned chnum = 0; chnum < m_channel.size(); ++chnum) {
		auto& channel = m_channel[chnum];
		// prepare() applies a new key and rebuilds the operator cache. With no
		// register or key change, the envelope state already decides the path.
		bool audible = m_modified ? channel.prepare(m_regs, chnum) : channel.audible();
		if (channel.finished()) {
			buffers[chnum] = nullptr;
			channel.quiesce_feedback(num);
			continue;
		}
		bool enabled = bitfield(chanmask, chnum) != 0;
		bool mix = audible && enabled;
		// Phase modulation matters only while the note is still running. AM is
		// read only when the channel is mixed.
		const unsigned choffs = opna_registers::channel_offset(chnum);
		bool walkLfo = lfoEnabled && ((audible && m_regs.ch_lfo_pm_sens(choffs) != 0)
			|| (mix && m_regs.ch_lfo_am_sens(choffs) != 0));
		if (walkLfo) {
			m_regs.restore_lfo(lfo0);
			lfoWalked = true;
		}
		// Closed pan still clocks and updates operator-1 feedback, but adds
		// nothing, so the mixer can skip the buffer.
		if (!mix || (!m_regs.ch_output_0(choffs) && !m_regs.ch_output_1(choffs))) {
			buffers[chnum] = nullptr;
		}
		if (mix && walkLfo) {
			synthesize_fm_channel<true, true>(channel, m_regs, buffers[chnum], num, env0, chnum);
		} else if (mix) {
			synthesize_fm_channel<true, false>(channel, m_regs, buffers[chnum], num, env0, chnum);
		} else if (walkLfo) {
			synthesize_fm_channel<false, true>(channel, m_regs, nullptr, num, env0, chnum);
		} else {
			synthesize_fm_channel<false, false>(channel, m_regs, nullptr, num, env0, chnum);
		}
	}

	m_env_counter = advance_eg_counter(env0, num);
	m_total_clocks = uint8_t(m_total_clocks + num);
	m_modified = false;
	if (lfoEnabled && !lfoWalked) {
		m_regs.restore_lfo(lfo0);
		const uint32_t lfoMaxCount = m_regs.lfo_max_count();
		for (unsigned i = 0; i < num; ++i) {
			m_regs.clock_lfo(lfoMaxCount);
		}
	}
}


//-------------------------------------------------
//  write - handle writes to the OPN registers
//-------------------------------------------------

void fm_engine::write(uint16_t regnum, uint8_t data, EmuTime time)
{
	// special case: writes to the mode register can impact IRQs;
	// schedule these writes to ensure ordering with timers
	// Consumed by the next generate(): rebuild caches and apply key changes.
	m_modified = true;

	if (regnum == opna_registers::REG_MODE) {
		mode_write(data, time);
		return;
	}

	// most writes are passive, consumed only when needed
	unsigned keyon_channel;
	unsigned keyon_opmask;
	if (m_regs.write(regnum, data, keyon_channel, keyon_opmask)) {
		// handle writes to the keyon register(s)
		if (keyon_channel < m_channel.size()) {
			// normal channel on/off
			m_channel[keyon_channel].keyonoff(keyon_opmask, KEYON_NORMAL);
		}
	}
}


//-------------------------------------------------
//  status - return the current state of the
//  status flags
//-------------------------------------------------

uint8_t fm_engine::status() const
{
	return m_status;
}


//-------------------------------------------------
//  update_timer - update the state of the given
//  timer
//-------------------------------------------------

void fm_engine::update_timer(unsigned tnum, bool enable, int32_t delta_clocks, EmuTime time)
{
	// if the timer is live, but not currently enabled, set the timer
	if (enable && !m_timer_running[tnum]) {
		// period comes from the registers, and is different for each
		uint32_t period = (tnum == 0) ? (1024 - m_regs.timer_a_value()) : 16 * (256 - m_regs.timer_b_value());

		// caller can also specify a delta to account for other effects
		period += delta_clocks;

		// reset it
		scheduleTimer(tnum, period * OPERATORS * m_clock_prescale, time);
		m_timer_running[tnum] = true;
	} else if (!enable) {
		// if the timer is not live, ensure it is not enabled
		scheduleTimer(tnum, -1, time);
		m_timer_running[tnum] = false;
	}
}

void fm_engine::scheduleTimer(unsigned timer, int32_t duration, EmuTime time)
{
	if (duration < 0) {
		timers[timer].cancel();
	} else {
		timers[timer].schedule(time + Clock<CLOCK>::duration(unsigned(duration)));
	}
}

fm_engine::Timer::Timer(Scheduler& scheduler_, uint8_t index_)
	: Schedulable(scheduler_)
	, index(index_)
{
}

void fm_engine::Timer::executeUntil(EmuTime time)
{
	// Same as OUTER(fm_engine, timers), plus one element when this is timers[1].
	auto addr = std::bit_cast<uintptr_t>(this) - offsetof(fm_engine, timers);
	if (index == 1) {
		addr -= sizeof(Timer);
	}
	auto& engine = *std::bit_cast<fm_engine*>(addr);
	engine.engine_timer_expired(index, time);
}

void fm_engine::engine_timer_expired(unsigned tnum, EmuTime time)
{
	assert(tnum == 0 || tnum == 1);

	// update status
	if (tnum == 0 && m_regs.enable_timer_a()) {
		set_reset_status(STATUS_TIMERA, 0);
	} else if (tnum == 1 && m_regs.enable_timer_b()) {
		set_reset_status(STATUS_TIMERB, 0);
	}

	// Timer A overflow in CSM mode keys channel 2. Flush the mixer first so
	// samples before this instant still use the old key state.
	if (tnum == 0 && m_regs.csm()) {
		OUTER(YM2608, fm).updateStream(time);
		m_modified = true;
		m_channel[2].keyonoff(0xf, KEYON_CSM);
	}

	// reset
	m_timer_running[tnum] = false;
	update_timer(tnum, true, 0, time);
}


//-------------------------------------------------
//  check_interrupts - check the interrupt sources
//  for interrupts
//-------------------------------------------------

void fm_engine::check_interrupts()
{
	irq.set((m_status & m_irq_mask) != 0);
}


//-------------------------------------------------
//  mode_write - handle a mode register write
//-------------------------------------------------

void fm_engine::mode_write(uint8_t data, EmuTime time)
{
	// actually write the mode register now
	uint32_t dummy1, dummy2;
	m_regs.write(opna_registers::REG_MODE, data, dummy1, dummy2);

	// reset timer status
	uint8_t reset_mask = 0;
	if (m_regs.reset_timer_b()) {
		reset_mask |= STATUS_TIMERB;
	}
	if (m_regs.reset_timer_a()) {
		reset_mask |= STATUS_TIMERA;
	}
	set_reset_status(0, reset_mask);

	// load timers; note that timer B gets a small negative adjustment because
	// the *16 multiplier is free-running, so the first tick of the clock
	// is a bit shorter
	update_timer(1, m_regs.load_timer_b(), -(m_total_clocks & 15), time);
	update_timer(0, m_regs.load_timer_a(), 0, time);
}


// --- FM / OPNA registers and engine ------------------------------------------

//*********************************************************
//  OPNA REGISTERS
//*********************************************************

//-------------------------------------------------
//  opna_registers - constructor
//-------------------------------------------------

opna_registers::opna_registers() :
	m_lfo_counter(0),
	m_lfo_am(0)
{
}


//-------------------------------------------------
//  reset - reset to initial state
//-------------------------------------------------

void opna_registers::reset()
{
	std::fill_n(&m_regdata[0], REGISTERS, 0);

	// enable output on both channels by default
	m_regdata[0xb4] = m_regdata[0xb5] = m_regdata[0xb6] = 0xc0;
	m_regdata[0x1b4] = m_regdata[0x1b5] = m_regdata[0x1b6] = 0xc0;
}


//-------------------------------------------------
//  write - handle writes to the register array
//-------------------------------------------------

bool opna_registers::write(uint16_t index, uint8_t data, unsigned& channel, unsigned& opmask)
{
	assert(index < REGISTERS);

	// writes in the 0xa0-af/0x1a0-af region are handled as latched pairs
	// borrow unused registers 0xb8-bf as temporary holding locations
	if ((index & 0xf0) == 0xa0) {
		if (bitfield(index, 0, 2) == 3) return false;

		uint32_t latchindex = 0xb8 | bitfield(index, 3);

		// writes to the upper half just latch (only low 6 bits matter)
		if (bitfield(index, 2)) {
			m_regdata[latchindex] = data & 0x3f;
		} else {
			// writes to the lower half also apply said latch
			m_regdata[index] = data;
			m_regdata[index | 4] = m_regdata[latchindex];
		}
		return false;
	} else if ((index & 0xf8) == 0xb8) {
		// registers 0xb8-0xbf are used internally
		return false;
	}

	// everything else is normal
	m_regdata[index] = data;

	// handle writes to the key on index
	if (index == 0x28) {
		channel = bitfield(data, 0, 2);
		if (channel == 3) return false;
		channel += bitfield(data, 2, 1) * 3;
		opmask = bitfield(data, 4, 4);
		return true;
	}
	return false;
}


//-------------------------------------------------
//  clock_lfo - clock the LFO, handling clock
//  division, depth, and waveform computations.
//  The caller has checked that it is enabled.
//-------------------------------------------------

int32_t opna_registers::clock_lfo(uint32_t max_count)
{
	uint32_t subcount = uint8_t(m_lfo_counter++);

	// when we cross the divider count, add enough to zero it and cause an
	// increment at bit 8; the 7-bit value lives from bits 8-14
	if (subcount >= max_count) {
		// note: to match the published values this should be 0x100 - subcount;
		// however, tests on the hardware and nuked bear out an off-by-one
		// error exists that causes the max LFO rate to be faster than published
		m_lfo_counter += 0x101 - subcount;
	}

	// AM value is 7 bits, staring at bit 8; grab the low 6 directly
	m_lfo_am = bitfield(m_lfo_counter, 8, 6);

	// first half of the AM period (bit 6 == 0) is inverted
	if (bitfield(m_lfo_counter, 8+6) == 0) {
		m_lfo_am ^= 0x3f;
	}

	// PM value is 5 bits, starting at bit 10; grab the low 3 directly
	int32_t pm = bitfield(m_lfo_counter, 10, 3);

	// PM is reflected based on bit 3
	if (bitfield(m_lfo_counter, 10+3)) {
		pm ^= 7;
	}

	// PM is negated based on bit 4
	return bitfield(m_lfo_counter, 10+4) ? -pm : pm;
}


//-------------------------------------------------
//  lfo_am_offset - return the AM offset from LFO
//  for the given shift
//-------------------------------------------------

uint32_t opna_registers::lfo_am_offset(uint32_t am_shift) const
{
	// QUESTION: max sensitivity should give 11.8dB range, but this value
	// is directly added to an x.8 attenuation value, which will only give
	// 126/256 or ~4.9dB range -- what am I missing? The calculation below
	// matches several other emulators, including the Nuked implemenation.

	// raw LFO AM value on OPN is 0-3F, scale that up by a factor of 2
	// (giving 7 bits) before applying the final shift
	return (m_lfo_am << 1) >> am_shift;
}


//-------------------------------------------------
//  cache_operator_data - fill the operator cache
//  with prefetched data
//-------------------------------------------------

void opna_registers::cache_operator_data(unsigned choffs, unsigned opoffs, opdata_cache& cache)
{
	// get frequency from the channel
	uint32_t block_freq = cache.block_freq = ch_block_freq(choffs);

	// if multi-frequency mode is enabled and this is channel 2,
	// fetch one of the special frequencies
	if (multi_freq() && choffs == 2) {
		if (opoffs == 2) {
			block_freq = cache.block_freq = multi_block_freq(1);
		} else if (opoffs == 10) {
			block_freq = cache.block_freq = multi_block_freq(2);
		} else if (opoffs == 6) {
			block_freq = cache.block_freq = multi_block_freq(0);
		}
	}

	// compute the keycode: block_freq is:
	//
	//     BBBFFFFFFFFFFF
	//     ^^^^???
	//
	// the 5-bit keycode uses the top 4 bits plus a magic formula
	// for the final bit
	uint32_t keycode = bitfield(block_freq, 10, 4) << 1;

	// lowest bit is determined by a mix of next lower FNUM bits
	// according to this equation from the YM2608 manual:
	//
	//   (F11 & (F10 | F9 | F8)) | (!F11 & F10 & F9 & F8)
	//
	// for speed, we just look it up in a 16-bit constant
	keycode |= bitfield(0xfe80, bitfield(block_freq, 7, 4));

	// detune adjustment
	cache.detune = detune_adjustment(op_detune(opoffs), keycode);

	// multiple value, as an x.1 value (0 means 0.5)
	cache.multiple = op_multiple(opoffs) * 2;
	if (cache.multiple == 0) {
		cache.multiple = 1;
	}

	// LFO PM sensitivity, read per sample otherwise
	cache.lfo_pm_sens = uint8_t(ch_lfo_pm_sens(choffs));

	// phase step, or PHASE_STEP_DYNAMIC if PM is active; this depends on
	// block_freq, detune, and multiple, so compute it after we've done those
	if (!lfo_enable() || cache.lfo_pm_sens == 0) {
		cache.phase_step = compute_phase_step(cache, 0);
	} else {
		cache.phase_step = opdata_cache::PHASE_STEP_DYNAMIC;
	}

	// total level, scaled by 8
	cache.total_level = op_total_level(opoffs) << 3;

	// 4-bit sustain level, but 15 means 31 so effectively 5 bits
	cache.eg_sustain = op_sustain_level(opoffs);
	cache.eg_sustain |= (cache.eg_sustain + 1) & 0x10;
	cache.eg_sustain <<= 5;

	// determine KSR adjustment for enevlope rates
	uint32_t ksrval = keycode >> (op_ksr(opoffs) ^ 3);
	cache.eg_rate[EG_ATTACK] = effective_rate(op_attack_rate(opoffs) * 2, ksrval);
	cache.eg_rate[EG_DECAY] = effective_rate(op_decay_rate(opoffs) * 2, ksrval);
	cache.eg_rate[EG_SUSTAIN] = effective_rate(op_sustain_rate(opoffs) * 2, ksrval);
	cache.eg_rate[EG_RELEASE] = effective_rate(op_release_rate(opoffs) * 4 + 2, ksrval);

	// SSG-EG shape and the LFO AM enable, read per sample otherwise
	cache.ssg_eg_mode = uint8_t(op_ssg_eg_mode(opoffs));
	cache.ssg_eg_enable = op_ssg_eg_enable(opoffs);
	cache.lfo_am_enable = op_lfo_am_enable(opoffs);
}


//-------------------------------------------------
//  compute_phase_step - compute the phase step
//-------------------------------------------------

uint32_t opna_registers::compute_phase_step(const opdata_cache& cache, int32_t lfo_raw_pm)
{
	// OPN phase calculation has only a single detune parameter
	// and uses FNUMs instead of keycodes

	// extract frequency number (low 11 bits of block_freq)
	uint32_t fnum = bitfield(cache.block_freq, 0, 11) << 1;

	// if there's a non-zero PM sensitivity, compute the adjustment
	if (cache.lfo_pm_sens != 0) {
		// apply the phase adjustment based on the upper 7 bits
		// of FNUM and the PM depth parameters
		fnum += opn_lfo_pm_phase_adjustment(bitfield(cache.block_freq, 4, 7), cache.lfo_pm_sens, lfo_raw_pm);

		// keep fnum to 12 bits
		fnum &= 0xfff;
	}

	// apply block shift to compute phase step
	uint32_t block = bitfield(cache.block_freq, 11, 3);
	uint32_t phase_step = (fnum << block) >> 2;

	// apply detune based on the keycode
	phase_step += cache.detune;

	// clamp to 17 bits in case detune overflows
	// QUESTION: is this specific to the YM2612/3438?
	phase_step &= 0x1ffff;

	// apply frequency multiplier (which is cached as an x.1 value)
	return (phase_step * cache.multiple) >> 1;
}

// --- ADPCM-A / ADPCM-B -------------------------------------------------------

//*********************************************************
// ADPCM "A" REGISTERS
//*********************************************************

//-------------------------------------------------
//  reset - reset the register state
//-------------------------------------------------

void adpcm_a_registers::reset()
{
	std::fill_n(&m_regdata[0], REGISTERS, 0);

	// initialize the pans to on by default, and max instrument volume;
	// some neogeo homebrews (for example ffeast) rely on this
	m_regdata[0x08] = m_regdata[0x09] = m_regdata[0x0a] =
	m_regdata[0x0b] = m_regdata[0x0c] = m_regdata[0x0d] = 0xdf;
}


//*********************************************************
// ADPCM "A" CHANNEL
//*********************************************************

//-------------------------------------------------
//  adpcm_a_channel - constructor
//-------------------------------------------------

adpcm_a_channel::adpcm_a_channel(adpcm_a_registers& regs) :
	m_regs(regs),
	m_curaddress(0),
	m_accumulator(0),
	m_step_index(0),
	m_playing(false),
	m_curnibble(0),
	m_curbyte(0)
{
}


//-------------------------------------------------
//  reset - reset the channel state
//-------------------------------------------------

void adpcm_a_channel::reset()
{
	m_playing = false;
	m_curnibble = 0;
	m_curbyte = 0;
	m_curaddress = 0;
	m_accumulator = 0;
	m_step_index = 0;
}


//-------------------------------------------------
//  keyonoff - signal key on/off
//-------------------------------------------------

void adpcm_a_channel::keyonoff(bool on, unsigned chnum)
{
	// QUESTION: repeated key ons restart the sample?
	m_playing = on;
	if (m_playing) {
		m_curaddress = m_regs.ch_start(chnum);
		m_curnibble = 0;
		m_curbyte = 0;
		m_accumulator = 0;
		m_step_index = 0;
	}
}


//-------------------------------------------------
//  clock - master clocking function
//-------------------------------------------------

void adpcm_a_channel::clock(unsigned chnum)
{
	// if not playing, hold a zero sample
	if (!m_playing) {
		m_accumulator = 0;
		return;
	}

	// if we're about to read nibble 0, fetch the data
	uint8_t data;
	if (m_curnibble == 0) {
		// stop when we hit the end address; apparently only low 20 bits are used for
		// comparison on the YM2610: this affects sample playback in some games, for
		// example twinspri character select screen music will skip some samples if
		// this is not correct
		//
		// note also: end address is inclusive, so wait until we are about to fetch
		// the sample just after the end before stopping; this is needed for nitd's
		// jump sound, for example
		uint32_t end = m_regs.ch_end(chnum) + 1;
		if (((m_curaddress ^ end) & 0xfffff) == 0) {
			m_playing = false;
			m_accumulator = 0;
			return;
		}

		m_curbyte = YM2608_ADPCM_ROM[m_curaddress++ & 0x1fff];
		data = m_curbyte >> 4;
		m_curnibble = 1;
	} else {
		// otherwise just extract from the previosuly-fetched byte
		data = m_curbyte & 0xf;
		m_curnibble = 0;
	}

	// compute the ADPCM delta
	static uint16_t const s_steps[49] =
	{
		 16,  17,   19,   21,   23,   25,   28,
		 31,  34,   37,   41,   45,   50,   55,
		 60,  66,   73,   80,   88,   97,  107,
		118, 130,  143,  157,  173,  190,  209,
		230, 253,  279,  307,  337,  371,  408,
		449, 494,  544,  598,  658,  724,  796,
		876, 963, 1060, 1166, 1282, 1411, 1552
	};
	int32_t delta = (2 * bitfield(data, 0, 3) + 1) * s_steps[m_step_index] / 8;
	if (bitfield(data, 3)) {
		delta = -delta;
	}

	// the 12-bit accumulator wraps on the ym2610 and ym2608 (like the msm5205)
	m_accumulator = int16_t((m_accumulator + delta) & 0xfff);

	// adjust ADPCM step
	static int8_t const s_step_inc[8] = { -1, -1, -1, -1, 2, 5, 7, 9 };
	m_step_index = int8_t(std::clamp(m_step_index + s_step_inc[bitfield(data, 0, 3)], 0, 48));
}


//-------------------------------------------------
//  silent - output stays zero until a register write
//-------------------------------------------------

bool adpcm_a_channel::silent(unsigned chnum) const
{
	// A stopped channel forces the accumulator to 0 on the next clock that
	// includes it. Until that clock, sample() still emits the held value.
	// Key-on only happens from a register write.
	if (!m_playing && m_accumulator == 0) return true;

	// Instrument level, total level and pan are registers. clock() does
	// not change them, and a write ends the current buffer first.
	return make_output_plan(chnum).pan_mask == 0;
}


//-------------------------------------------------
//  make_output_plan - read the register fields
//  that hold for the whole buffer
//-------------------------------------------------

adpcm_a_channel::output_plan adpcm_a_channel::make_output_plan(unsigned chnum) const
{
	output_plan plan;

	// volume combines instrument and total levels
	int vol = (m_regs.ch_instrument_level(chnum) ^ 0x1f) + (m_regs.total_level() ^ 0x3f);

	// convert into a shift and a multiplier
	// QUESTION: verify this from other sources
	plan.mul = 15 - (vol & 7);
	plan.shift = 4 + 1 + (vol >> 3);

	// a maximum combined volume adds nothing, and neither does a closed pan
	plan.pan_mask = 0;
	if (vol < 63) {
		if (m_regs.ch_pan_left(chnum)) {
			plan.pan_mask |= 1;
		}
		if (m_regs.ch_pan_right(chnum)) {
			plan.pan_mask |= 2;
		}
	}
	return plan;
}



//*********************************************************
// ADPCM "A" ENGINE
//*********************************************************

//-------------------------------------------------
//  adpcm_a_engine - constructor
//-------------------------------------------------

adpcm_a_engine::adpcm_a_engine() :
	m_channel(generate_array<CHANNELS>([&](size_t /*chnum*/) {
		return adpcm_a_channel(m_regs); }))
{
}


//-------------------------------------------------
//  reset - reset the engine state
//-------------------------------------------------

void adpcm_a_engine::reset()
{
	// reset register state
	m_regs.reset();

	// reset each channel
	for (auto& chan : m_channel) {
		chan.reset();
	}
}


//-------------------------------------------------
//  generate - one channel for the whole buffer, then the next
//-------------------------------------------------

void adpcm_a_engine::generate(std::span<float*, CHANNELS> buffers, unsigned num, uint32_t envStart)
{
	for (unsigned chnum = 0; chnum < m_channel.size(); ++chnum) {
		auto& channel = m_channel[chnum];
		if (channel.resting()) continue;
		// Volume and pan are registers, so they are read once per buffer.
		// An empty pan mask means this channel adds nothing.
		const auto plan = channel.make_output_plan(chnum);
		float* buf = (plan.pan_mask != 0) ? buffers[chnum] : nullptr;
		uint32_t env = envStart;
		// Channels 0-3 clock on every ADPCM tick. Channels 4-5 clock on
		// every other tick, when envelope bit 2 is clear.
		const bool low = chnum < 4;
		if (buf == nullptr) {
			for (unsigned i = 0; i < num; ++i) {
				env = step_eg_counter(env);
				if ((env & 3) == 0 && (low || (env & 4) == 0)) {
					channel.clock(chnum);
				}
			}
		} else {
			// Only clock() changes the accumulator, so the scaled sample is
			// recomputed there instead of once per sample. A closed pan side
			// keeps adding zero.
			const bool pan_left = (plan.pan_mask & 1) != 0;
			const bool pan_right = (plan.pan_mask & 2) != 0;
			float left = 0.0f;
			float right = 0.0f;
			auto rescale = [&] {
				float value = float(channel.sample(plan));
				left = pan_left ? value : 0.0f;
				right = pan_right ? value : 0.0f;
			};
			rescale();
			for (unsigned i = 0; i < num; ++i) {
				env = step_eg_counter(env);
				if ((env & 3) == 0 && (low || (env & 4) == 0)) {
					channel.clock(chnum);
					rescale();
				}
				unsigned pos = i * 2;
				buf[pos + 0] += left;
				buf[pos + 1] += right;
			}
		}
	}
}


//-------------------------------------------------
//  write - handle writes to the ADPCM-A registers
//-------------------------------------------------

void adpcm_a_engine::write(uint32_t regnum, uint8_t data)
{
	// store the raw value to the register array;
	// most writes are passive, consumed only when needed
	m_regs.write(regnum, data);

	// actively handle writes to the control register
	if (regnum == 0x00) {
		for (unsigned chnum = 0; chnum < m_channel.size(); chnum++) {
			if (bitfield(data, chnum)) {
				m_channel[chnum].keyonoff(bitfield(~data, 7) != 0, chnum);
			}
		}
	}
}



//*********************************************************
// ADPCM "B" REGISTERS
//*********************************************************

//-------------------------------------------------
//  reset - reset the register state
//-------------------------------------------------

void adpcm_b_registers::reset()
{
	std::fill_n(&m_regdata[0], REGISTERS, 0);

	// default limit to wide open
	m_regdata[0x0c] = m_regdata[0x0d] = 0xff;
}



//*********************************************************
// ADPCM "B" CHANNEL
//*********************************************************

//-------------------------------------------------
//  adpcm_b_channel - constructor
//-------------------------------------------------

adpcm_b_channel::adpcm_b_channel(Ram& ram_, adpcm_b_registers& regs) :
	m_regs(regs),
	ram(ram_),
	m_curaddress(0),
	m_position(0),
	m_accumulator(0),
	m_prev_accum(0),
	m_adpcm_step(STEP_MIN),
	m_status(STATUS_BRDY),
	m_curnibble(0),
	m_curbyte(0),
	m_dummy_read(0),
	m_cpu_write_active(false)
{
}


//-------------------------------------------------
//  reset - reset the channel state
//-------------------------------------------------

void adpcm_b_channel::reset()
{
	m_status = STATUS_BRDY;
	m_curnibble = 0;
	m_curbyte = 0;
	m_dummy_read = 0;
	m_position = 0;
	m_curaddress = 0;
	m_accumulator = 0;
	m_prev_accum = 0;
	m_adpcm_step = STEP_MIN;
	m_cpu_write_active = false;
}


//-------------------------------------------------
//  consume_nibble - one sample after the position has wrapped
//-------------------------------------------------

bool adpcm_b_channel::consume_nibble()
{
	// if we're about to process nibble 0, fetch sample
	if (m_curnibble == 0) {
		// playing from RAM/ROM
		if (m_regs.external()) {
			m_curbyte = ram[m_curaddress & 0x3ffff];
		}
	}

	// extract the nibble from our current byte
	uint8_t data = uint8_t(m_curbyte << (4 * m_curnibble)) >> 4;
	m_curnibble ^= 1;

	// we just processed the last nibble
	if (m_curnibble == 0) {
		// if playing from RAM/ROM, check the end/limit address or advance
		if (m_regs.external()) {
			// handle the sample end, either repeating or stopping
			if (at_end()) {
				// if repeating, go back to the start
				if (m_regs.repeat()) {
					load_start();
				} else {
					// otherwise, done; set the EOS bit
					m_accumulator = 0;
					m_prev_accum = 0;
					m_status = (m_status & ~STATUS_PLAYING) | STATUS_EOS;
					return false;
				}
			} else if (at_limit()) {
				// wrap at the limit address
				m_curaddress = 0;
			} else {
				// otherwise, advance the current address
				m_curaddress++;
				m_curaddress &= 0xffffff;
			}
		} else {
			// if CPU-driven, copy the next byte and request more
			m_curbyte = m_regs.cpudata();
			m_status |= STATUS_BRDY;
		}
	}

	// remember previous value for interpolation
	m_prev_accum = m_accumulator;

	// forecast to next forecast: 1/8, 3/8, 5/8, 7/8, 9/8, 11/8, 13/8, 15/8
	int32_t delta = (2 * bitfield(data, 0, 3) + 1) * m_adpcm_step / 8;
	if (bitfield(data, 3)) {
		delta = -delta;
	}

	// add and clamp to 16 bits
	m_accumulator = int16_t(std::clamp(int(m_accumulator) + delta, -32768, 32767));

	// scale the ADPCM step: 0.9, 0.9, 0.9, 0.9, 1.2, 1.6, 2.0, 2.4
	static uint8_t const s_step_scale[8] = { 57, 57, 57, 57, 77, 102, 128, 153 };
	m_adpcm_step = int16_t(std::clamp(int(m_adpcm_step) * s_step_scale[bitfield(data, 0, 3)] / 64,
	                                  int(STEP_MIN), int(STEP_MAX)));
	return true;
}


//-------------------------------------------------
//  clock_n - several clocks, batching position steps
//-------------------------------------------------

void adpcm_b_channel::clock_n(unsigned num)
{
	if (num == 0) return;

	// Not decoding: a clock only clears PLAYING. One store covers the run.
	if (!decoding()) {
		m_status &= ~STATUS_PLAYING;
		return;
	}

	const uint32_t delta = m_regs.delta_n();
	// Adding zero never reaches the next nibble.
	if (delta == 0) return;

	while (num != 0) {
		// Clocks until and including the next 16-bit overflow.
		uint32_t room = 0x10000u - m_position;
		uint32_t steps = (room + delta - 1) / delta;
		if (steps > num) {
			m_position = uint16_t(uint32_t(m_position) + uint64_t(num) * delta);
			return;
		}
		// The low 16 bits are the position after the overflowing add.
		m_position = uint16_t(uint32_t(m_position) + uint64_t(steps) * delta);
		num -= steps;
		// End-without-repeat stops here. Later clocks would only clear
		// PLAYING, which consume_nibble() already cleared.
		if (!consume_nibble()) return;
	}
}


//-------------------------------------------------
//  generate - num clocks with output
//-------------------------------------------------

void adpcm_b_channel::generate(float* buffer, unsigned num, const output_plan& plan)
{
	if (num == 0) return;

	const bool pan_left = (plan.pan_mask & 1) != 0;
	const bool pan_right = (plan.pan_mask & 2) != 0;

	// The decode state and delta-N are registers, so they are read once.
	// Without decoding the position never moves, and neither does it without
	// a step, so in both cases the held sample repeats for the whole run.
	uint32_t delta = 0;
	if (decoding()) {
		delta = m_regs.delta_n();
	} else {
		m_status &= ~STATUS_PLAYING;
	}

	auto tick = [&] {
		// End-without-repeat zeroes both interpolator ends, so the rest of
		// the run holds that zero.
		if (delta != 0 && !advance(delta)) {
			delta = 0;
		}
		return float(sample(plan));
	};

	if (pan_left && pan_right) {
		for (unsigned i = 0; i < num; ++i) {
			float value = tick();
			unsigned pos = i * 2;
			buffer[pos + 0] += value;
			buffer[pos + 1] += value;
		}
	} else if (pan_left) {
		for (unsigned i = 0; i < num; ++i) {
			buffer[i * 2] += tick();
		}
	} else {
		for (unsigned i = 0; i < num; ++i) {
			buffer[i * 2 + 1] += tick();
		}
	}
}


//-------------------------------------------------
//  silent - output stays zero until a register write
//-------------------------------------------------

bool adpcm_b_channel::silent() const
{
	// Not advancing: a clock returns before it touches the accumulators.
	// Playback itself starts only from a register write. A held sample is
	// silent only when both ends of the interpolator are already zero.
	if (!decoding() && m_accumulator == 0 && m_prev_accum == 0) return true;

	// Level and pan are registers. clock() does not change them, and a
	// write ends the current buffer first.
	return pan_mask() == 0;
}


//-------------------------------------------------
//  read - handle special register reads
//-------------------------------------------------

uint8_t adpcm_b_channel::peek(uint32_t regnum) const
{
	// Observe the next CPU read without consuming dummy reads, advancing RAM,
	// changing EOS/BRDY or invoking a potentially destructive host read.
	if (regnum == 0x08 && !m_regs.execute() && !m_regs.record() && m_regs.external()) {
		if (m_cpu_write_active) return m_regs.cpudata();
		if (m_dummy_read == 0) return ram[m_curaddress & 0x3ffff];
	}
	return 0;
}

uint8_t adpcm_b_channel::read(uint32_t regnum)
{
	uint8_t result = 0;

	// register 8 reads over the bus under some conditions
	if (regnum == 0x08 && !m_regs.execute() && !m_regs.record() && m_regs.external()) {
		// A mode change alone does not terminate an unfinished RAM writer.
		// Until RESET, the CPU sees its last data-buffer byte (Makoto V4).
		if (m_cpu_write_active) return m_regs.cpudata();

		// two dummy reads are consumed first
		if (m_dummy_read != 0) {
			load_start();
			m_dummy_read--;
		} else {
			// read the data
			// read from outside of the chip
			result = ram[m_curaddress & 0x3ffff];

			// did we hit the end? if so, signal EOS
			if (at_end()) {
				m_status = STATUS_EOS | STATUS_BRDY;
			} else {
				// signal ready
				m_status = STATUS_BRDY;
			}

			// The limit is inclusive: consume its last byte before wrapping.
			if (at_limit()) {
				m_curaddress = 0;
			} else {
				m_curaddress++;
			}
		}
	}
	return result;
}


//-------------------------------------------------
//  write - handle special register writes
//-------------------------------------------------

void adpcm_b_channel::write(uint32_t regnum, uint8_t value)
{
	// register 0 can do a reset; also use writes here to reset the
	// dummy read counter
	if (regnum == 0x00) {
		if (m_regs.execute()) {
			load_start();
		} else {
			m_status &= ~STATUS_EOS;
		}
		if (m_regs.resetflag()) {
			reset();
		}
		if (m_regs.external()) {
			m_dummy_read = 2;
		}
	} else if (regnum == 0x08) {
		// register 8 writes over the bus under some conditions
		// if writing from the CPU during execute, clear the ready flag
		if (m_regs.execute() && !m_regs.record() && !m_regs.external()) {
			m_status &= ~STATUS_BRDY;
		} else if (!m_regs.execute() && m_regs.record() && m_regs.external()) {
			// if writing during "record", pass through as data
			// clear out dummy reads and set start address
			if (m_dummy_read != 0) {
				load_start();
				m_dummy_read = 0;
			}

			// The end register describes an inclusive chunk. Keep the CPU
			// address one past its last byte once the transfer has stopped.
			uint32_t end = (m_regs.end() + 1) << address_shift();
			if (m_curaddress != end) {
				ram[m_curaddress++ & 0x3ffff] = value;
				m_cpu_write_active = true;
			}

			if (m_curaddress == end) {
				m_cpu_write_active = false;
				m_status = STATUS_EOS | STATUS_BRDY;
			} else {
				m_status = STATUS_BRDY;
			}
		}
	}
}


//-------------------------------------------------
//  address_shift - compute the current address
//  shift amount based on register settings
//-------------------------------------------------

uint32_t adpcm_b_channel::address_shift() const
{
	// if ROM or 8-bit DRAM, shift is 5 bits
	if (m_regs.rom_ram()) return 5;
	if (m_regs.dram_8bit()) return 5;

	// otherwise, shift is 2 bits
	return 2;
}


//-------------------------------------------------
//  load_start - load the start address and
//  initialize the state
//-------------------------------------------------

void adpcm_b_channel::load_start()
{
	m_status = (m_status & ~STATUS_EOS) | STATUS_PLAYING;
	m_curaddress = m_regs.external() ? (m_regs.start() << address_shift()) : 0;
	m_curnibble = 0;
	m_curbyte = 0;
	m_position = 0;
	m_accumulator = 0;
	m_prev_accum = 0;
	m_adpcm_step = STEP_MIN;
	m_cpu_write_active = false;
}



//*********************************************************
// ADPCM "B" ENGINE
//*********************************************************

//-------------------------------------------------
//  adpcm_b_engine - constructor
//-------------------------------------------------

adpcm_b_engine::adpcm_b_engine(const DeviceConfig& config, std::string_view name) :
	ram(config, strCat(name, " ADPCM RAM"), "YM2608 ADPCM-B sample RAM", 0x40000),
	m_channel(ram, m_regs)
{
	ram.clear(0); // Deterministic emulator policy; hardware power-on contents are unknown.
}


//-------------------------------------------------
//  reset - reset the engine state
//-------------------------------------------------

void adpcm_b_engine::reset()
{
	// reset registers
	m_regs.reset();

	// reset each channel
	m_channel.reset();
}


//-------------------------------------------------
//  generate - the whole buffer for the single channel
//-------------------------------------------------

void adpcm_b_engine::generate(float* buffer, unsigned num)
{
	if (m_channel.resting()) return;
	// Level and pan are registers, so they are read once per buffer. An empty
	// pan mask means this channel adds nothing.
	const auto plan = m_channel.make_output_plan();
	if (buffer == nullptr || plan.pan_mask == 0) {
		m_channel.clock_n(num);
	} else {
		m_channel.generate(buffer, num, plan);
	}
}


//-------------------------------------------------
//  write - handle writes to the ADPCM-B registers
//-------------------------------------------------

void adpcm_b_engine::write(uint32_t regnum, uint8_t data)
{
	// store the raw value to the register array;
	// most writes are passive, consumed only when needed
	m_regs.write(regnum, data);

	// let the channel handle any special writes
	m_channel.write(regnum, data);
}

} // namespace openmsx
