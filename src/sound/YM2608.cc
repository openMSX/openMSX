#include "YM2608.hh"

#include "DummyAY8910Periphery.hh"

#include "Clock.hh"
#include "DeviceConfig.hh"
#include "outer.hh"
#include "serialize.hh"
#include "stl.hh"
#include "unreachable.hh"

#include "3rdparty/ym2608/fmopn_2608rom.h"

#include <algorithm>
#include <numbers>

namespace openmsx {

static constexpr unsigned CLOCK = 8'000'000;

static constexpr uint8_t STATUS_BUSY = 0x80;
static constexpr uint8_t STATUS_ADPCM_B_EOS = 0x04;
static constexpr uint8_t STATUS_ADPCM_B_BRDY = 0x08;
static constexpr uint8_t STATUS_ADPCM_B_PLAYING = 0x20;

namespace ym2608 {

// Envelope counter shared by the FM engine and the ADPCM-A clock grid. OPNA
// divides the envelope by 3, which this counter models by skipping every value
// whose low two bits are 3, so those bits cycle 0, 1, 2.
[[nodiscard]] static constexpr uint32_t stepEgCounter(uint32_t counter)
{
	++counter;
	if ((counter & 3) == 3) {
		++counter;
	}
	return counter;
}

// Advance the same counter by steps samples in closed form. The low two bits
// never hold 3, so a zero step count adds nothing.
[[nodiscard]] static constexpr uint32_t advanceEgCounter(uint32_t counter, uint32_t steps)
{
	uint32_t low = counter & 3;
	assert(low < 3);
	return counter + steps + (steps + low) / 3;
}

// Apply KSR to the raw ADSR rate, ignoring ksr if the raw value is 0, and
// clamping to 63.
[[nodiscard]] static constexpr uint32_t effectiveRate(uint32_t rawRate, uint32_t ksr)
{
	return (rawRate == 0) ? 0 : std::min<uint32_t>(rawRate + ksr, 63);
}

// Absolute value of sin(input) as log attenuation in 4.8 fixed point.
// The input maps a full 0-2*PI onto 10 bits.
[[nodiscard]] static constexpr unsigned absSinAttenuation(unsigned input)
{
	// the values here are stored as 4.8 logarithmic values for 1/4 phase
	// this matches the internal format of the OPN chip, extracted from the die
	static constexpr std::array<uint16_t, 256> sinTable = {
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
		0x002,0x001,0x001,0x001,0x001,0x001,0x001,0x001,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,
	};

	// if the top bit is set, we're in the second half of the curve
	// which is a mirror image, so invert the index
	if (bitfield(input, 8)) {
		input = ~input;
	}

	// return the value from the table
	return sinTable[input & 0xff];
}

// 10-bit phase, sign in bit 15. Built once from absSinAttenuation().
static constexpr unsigned WAVEFORM_LENGTH = 0x400;
static constexpr auto waveform = generate_array<WAVEFORM_LENGTH>([](size_t index) {
	auto i = unsigned(index);
	return uint16_t(absSinAttenuation(i) | (bitfield(i, 9) << 15));
});

// Convert 5.8 log attenuation to 13-bit linear volume.
[[nodiscard]] static uint32_t attenuationToVolume(uint32_t input)
{
	// the values here are 10-bit mantissas with an implied leading bit
	// this matches the internal format of the OPN chip, extracted from the die

	// as a nod to performance, the implicit 0x400 bit is pre-incorporated, and
	// the values are left-shifted by 2 so that a simple right shift is all that
	// is needed; also the order is reversed to save a NOT on the input
	auto X = [](uint16_t a) { return (a | 0x400) << 2; };
	static constexpr std::array<uint16_t, 256> powerTable = {
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
		X(0x014),X(0x011),X(0x00e),X(0x00b),X(0x008),X(0x006),X(0x003),X(0x000),
	};

	// look up the fractional part, then shift by the whole
	return powerTable[input & 0xff] >> (input >> 8);
}

// 6-bit ADSR rate and 3-bit step index to a 4-bit envelope increment
// (for attack, a fractional scale factor to decrease by).
[[nodiscard]] static uint32_t attenuationIncrement(uint32_t rate, uint32_t index)
{
	static constexpr std::array<uint32_t, 64> incrementTable = {
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
		0x88888888, 0x88888888, 0x88888888, 0x88888888,  // 60-63  (0x3C-0x3F)
	};
	return bitfield(incrementTable[rate], 4 * index, 4);
}

// 5-bit key code and 3-bit detune to a 6-bit signed phase displacement.
// Table kept instead of Nuked's equations, which it has been checked against.
[[nodiscard]] static int32_t detuneAdjustment(uint32_t detune, uint32_t keycode)
{
	static constexpr std::array<std::array<uint8_t, 4>, 32> detuneTable = {{
		{0,  0,  1,  2}, {0,  0,  1,  2}, {0,  0,  1,  2}, {0,  0,  1,  2},
		{0,  1,  2,  2}, {0,  1,  2,  3}, {0,  1,  2,  3}, {0,  1,  2,  3},
		{0,  1,  2,  4}, {0,  1,  3,  4}, {0,  1,  3,  4}, {0,  1,  3,  5},
		{0,  2,  4,  5}, {0,  2,  4,  6}, {0,  2,  4,  6}, {0,  2,  5,  7},
		{0,  2,  5,  8}, {0,  3,  6,  8}, {0,  3,  6,  9}, {0,  3,  7, 10},
		{0,  4,  8, 11}, {0,  4,  8, 12}, {0,  4,  9, 13}, {0,  5, 10, 14},
		{0,  5, 11, 16}, {0,  6, 12, 17}, {0,  6, 13, 19}, {0,  7, 14, 20},
		{0,  8, 16, 22}, {0,  8, 16, 22}, {0,  8, 16, 22}, {0,  8, 16, 22},
	}};
	int32_t result = detuneTable[keycode][detune & 3];
	return bitfield(detune, 2) ? -result : result;
}

// Signed PM frequency adjustment from the top 7 F-number bits, 3-bit PM depth,
// and signed 5-bit raw PM. Written to match Nuked.
[[nodiscard]] static int32_t opnLfoPmPhaseAdjustment(uint32_t fnumBits, uint32_t pmSensitivity, int32_t lfoRawPm)
{
	// this table encodes 2 shift values to apply to the top 7 bits
	// of fnum; it is effectively a cheap multiply by a constant
	// value containing 0-2 bits
	static constexpr std::array<std::array<uint8_t, 8>, 8> lfoPmShifts = {{
		{0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77},
		{0x77, 0x77, 0x77, 0x77, 0x72, 0x72, 0x72, 0x72},
		{0x77, 0x77, 0x77, 0x72, 0x72, 0x72, 0x17, 0x17},
		{0x77, 0x77, 0x72, 0x72, 0x17, 0x17, 0x12, 0x12},
		{0x77, 0x77, 0x72, 0x17, 0x17, 0x17, 0x12, 0x07},
		{0x77, 0x77, 0x17, 0x12, 0x07, 0x07, 0x02, 0x01},
		{0x77, 0x77, 0x17, 0x12, 0x07, 0x07, 0x02, 0x01},
		{0x77, 0x77, 0x17, 0x12, 0x07, 0x07, 0x02, 0x01},
	}};

	// look up the relevant shifts
	int32_t absPm = (lfoRawPm < 0) ? -lfoRawPm : lfoRawPm;
	uint32_t shifts = lfoPmShifts[pmSensitivity][bitfield(absPm, 0, 3)];

	// compute the adjustment
	int32_t adjust = (fnumBits >> bitfield(shifts, 0, 4)) + (fnumBits >> bitfield(shifts, 4, 4));
	if (pmSensitivity > 5) {
		adjust <<= pmSensitivity - 5;
	}
	adjust >>= 2;

	// every 16 cycles it inverts sign
	return (lfoRawPm < 0) ? -adjust : adjust;
}


void FmOperator::reset()
{
	phaseAcc = 0;
	envAttenuation = 0x3ff;
	envState = EnvelopeState::RELEASE;
	ssgInverted = false;
	keyState = false;
	keyOnLive = 0;
}

bool FmOperator::prepare(const OpnaRegisters& regs, unsigned chOffs, unsigned opOffs)
{
	// cache the data
	regs.cacheOperatorData(chOffs, opOffs, cache);

	// clock the key state
	clockKeystate(keyOnLive != 0);
	keyOnLive &= ~(1 << std::to_underlying(KeyOnType::CSM));

	// Only an enabled SSG-EG can invert. Turning it off mid-note leaves the
	// flag set, so it is cleared here, after startRelease() has had its look
	// at the old value.
	if (!cache.ssgEgEnable) {
		ssgInverted = false;
	}

	// we're active until we're quiet after the release
	return (envState != EnvelopeState::RELEASE || envAttenuation < EG_QUIET);
}

bool FmOperator::finished() const
{
	return envState == EnvelopeState::RELEASE && envAttenuation >= 0x3ff;
}

bool FmOperator::audible() const
{
	return envState != EnvelopeState::RELEASE || envAttenuation < EG_QUIET;
}

void FmOperator::clock(uint32_t envCounter, int32_t lfoRawPm)
{
	// clock the SSG-EG state (OPN/OPNA); prepare() cleared the inversion if
	// it is disabled
	if (cache.ssgEgEnable) {
		clockSsgEgState();
	}

	// clock the envelope if on an envelope cycle; envCounter is a x.2 value
	if (bitfield(envCounter, 0, 2) == 0) {
		clockEnvelope(envCounter >> 2);
	}

	// clock the phase
	clockPhase(lfoRawPm);
}

int32_t FmOperator::computeVolume(uint32_t phase, uint32_t amOffset) const
{
	// the low 10 bits of phase represents a full 2*PI period over
	// the full sin wave

	// early out if the envelope is effectively off
	if (envAttenuation > EG_QUIET) return 0;

	// get the absolute value of the sin, as attenuation, as a 4.8 fixed point value
	uint32_t sinAttenuation = waveform[phase & (WAVEFORM_LENGTH - 1)];

	// get the attenuation from the evelope generator as a 4.6 value, shifted up to 4.8
	uint32_t envAtt = envelopeAttenuation(amOffset) << 2;

	// combine into a 5.8 value, then convert from attenuation to 13-bit linear volume
	int32_t result = attenuationToVolume((sinAttenuation & 0x7fff) + envAtt);

	// negate if in the negative part of the sin wave (sign bit gives 14 bits)
	return bitfield(sinAttenuation, 15) ? -result : result;
}

void FmOperator::keyOnOff(bool on, KeyOnType type)
{
	auto bit = std::to_underlying(type);
	keyOnLive = uint8_t((keyOnLive & ~(1u << bit)) | (uint8_t(on) << bit));
}

void FmOperator::endCsmPulse()
{
	// prepare() applied CSM then cleared that bit. A normal key still set in
	// keyOnLive must stay on; CSM-only leaves keyState set with keyOnLive clear.
	if (keyState && keyOnLive == 0) {
		clockKeystate(false);
	}
}

// Also used when an SSG-EG cycle restarts.
void FmOperator::startAttack(bool isRestart)
{
	// don't change anything if already in attack state
	if (envState == EnvelopeState::ATTACK) return;
	envState = EnvelopeState::ATTACK;

	// generally not inverted at start, except if SSG-EG is enabled and
	// one of the inverted modes is specified; leave this alone on a
	// restart, as it is managed by the clockSsgEgState() code
	if (!isRestart) {
		ssgInverted = cache.ssgEgEnable && bitfield(cache.ssgEgMode, 2);
	}

	// reset the phase when we start an attack due to a key on
	// (but not when due to an SSG-EG restart except in certain cases
	// managed directly by the SSG-EG code)
	if (!isRestart) {
		phaseAcc = 0;
	}

	// if the attack rate >= 62 then immediately go to max attenuation
	if (cache.egRate[std::to_underlying(EnvelopeState::ATTACK)] >= 62) {
		envAttenuation = 0;
	}
}

void FmOperator::startRelease()
{
	// don't change anything if already in release state
	if (envState >= EnvelopeState::RELEASE) return;
	envState = EnvelopeState::RELEASE;

	// if attenuation if inverted due to SSG-EG, snap the inverted attenuation
	// as the starting point
	if (ssgInverted) {
		envAttenuation = (0x200 - envAttenuation) & 0x3ff;
		ssgInverted = false;
	}
}

void FmOperator::clockKeystate(bool on)
{
	// has the key changed?
	if (on != keyState) {
		keyState = on;

		// if the key has turned on, start the attack
		if (on) {
			startAttack();
		} else {
			// otherwise, start the release
			startRelease();
		}
	}
}

// Call only when SSG-EG is enabled.
void FmOperator::clockSsgEgState()
{
	// work only happens once the attenuation crosses above 0x200
	if (!bitfield(envAttenuation, 9)) return;

	// 8 SSG-EG modes:
	//    000: repeat normally
	//    001: run once, hold low
	//    010: repeat, alternating between inverted/non-inverted
	//    011: run once, hold high
	//    100: inverted repeat normally
	//    101: inverted run once, hold low
	//    110: inverted repeat, alternating between inverted/non-inverted
	//    111: inverted run once, hold high
	uint32_t mode = cache.ssgEgMode;

	// hold modes (1/3/5/7)
	if (bitfield(mode, 0)) {
		// set the inverted flag to the end state (0 for modes 1/7, 1 for modes 3/5)
		ssgInverted = bitfield(mode, 2) ^ bitfield(mode, 1);

		// if holding, force the attenuation to the expected value once we're
		// past the attack phase
		if (envState != EnvelopeState::ATTACK) {
			envAttenuation = ssgInverted ? 0x200 : 0x3ff;
		}
	} else {
		// continuous modes (0/2/4/6)
		// toggle invert in alternating mode (even in attack state)
		ssgInverted ^= bitfield(mode, 1);

		// restart attack if in decay/sustain states
		if (envState == EnvelopeState::DECAY || envState == EnvelopeState::SUSTAIN) {
			startAttack(true);
		}

		// phase is reset to 0 in modes 0/4
		if (bitfield(mode, 1) == 0) {
			phaseAcc = 0;
		}
	}

	// in all modes, once we hit release state, attenuation is forced to maximum
	if (envState == EnvelopeState::RELEASE) {
		envAttenuation = 0x3ff;
	}
}

void FmOperator::clockEnvelope(uint32_t envCounter)
{
	// handle attack->decay transitions
	if (envState == EnvelopeState::ATTACK && envAttenuation == 0) {
		envState = EnvelopeState::DECAY;
	}

	// handle decay->sustain transitions; it is important to do this immediately
	// after the attack->decay transition above in the event that the sustain level
	// is set to 0 (in which case we will skip right to sustain without doing any
	// decay); as an example where this can be heard, check the cymbals sound
	// in channel 0 of shinobi's test mode sound #5
	if (envState == EnvelopeState::DECAY && envAttenuation >= cache.egSustain) {
		envState = EnvelopeState::SUSTAIN;
	}

	// fetch the appropriate 6-bit rate value from the cache
	uint32_t rate = cache.egRate[std::to_underlying(envState)];

	// compute the rate shift value; this is the shift needed to
	// apply to the envCounter such that it becomes a 5.11 fixed
	// point number
	uint32_t rateShift = rate >> 2;
	envCounter <<= rateShift;

	// see if the fractional part is 0; if not, it's not time to clock
	if (bitfield(envCounter, 0, 11) != 0) return;

	// determine the increment based on the non-fractional part of envCounter
	uint32_t relevantBits = bitfield(envCounter, (rateShift <= 11) ? 11 : rateShift, 3);
	uint32_t increment = attenuationIncrement(rate, relevantBits);

	// attack is the only one that increases
	if (envState == EnvelopeState::ATTACK) {
		// glitch means that attack rates of 62/63 don't increment if
		// changed after the initial key on (where they are handled
		// specially); nukeykt confirms this happens on OPM, OPN, OPL/OPLL
		// at least so assuming it is true for everyone
		if (rate < 62) {
			envAttenuation += (~envAttenuation * increment) >> 4;
		}
	} else {
		// all other cases are similar
		// non-SSG-EG cases just apply the increment
		if (!cache.ssgEgEnable) {
			envAttenuation += increment;
		} else if (envAttenuation < 0x200) {
			// SSG-EG only applies if less than mid-point, and then at 4x
			envAttenuation += 4 * increment;
		}

		// clamp the final attenuation
		if (envAttenuation >= 0x400) {
			envAttenuation = 0x3ff;
		}
	}
}

// 10.10 phase; OPN logic verified against Nuked.
void FmOperator::clockPhase(int32_t lfoRawPm)
{
	// read from the cache, or recalculate if PM active
	uint32_t phaseStep = cache.phaseStep;
	if (phaseStep == OpDataCache::PHASE_STEP_DYNAMIC) {
		phaseStep = OpnaRegisters::computePhaseStep(cache, lfoRawPm);
	}

	// finally apply the step to the current phase value
	phaseAcc += phaseStep;
}

uint32_t FmOperator::envelopeAttenuation(uint32_t amOffset) const
{
	uint32_t result = envAttenuation;

	// invert if necessary due to SSG-EG
	if (ssgInverted) {
		result = (0x200 - result) & 0x3ff;
	}

	// add in LFO AM modulation
	if (cache.lfoAmEnable) {
		result += amOffset;
	}

	// add in total level from the cache
	result += cache.totalLevel;

	// clamp to max and return
	return std::min<uint32_t>(result, 0x3ff);
}


void FmChannel::reset()
{
	// reset our data
	feedback[0] = feedback[1] = 0;
	feedbackIn = 0;
	for (auto& op : ops) {
		op.reset();
	}
}

void FmChannel::keyOnOff(uint32_t states, KeyOnType type)
{
	for (unsigned opnum = 0; opnum < ops.size(); opnum++) {
		ops[opnum].keyOnOff(bitfield(states, opnum) != 0, type);
	}
}

bool FmChannel::finished() const
{
	return std::ranges::all_of(ops, &FmOperator::finished);
}

bool FmChannel::audible() const
{
	return std::ranges::any_of(ops, &FmOperator::audible);
}

void FmChannel::quiesceFeedback(unsigned num)
{
	if (num >= 2) {
		feedback[0] = feedback[1] = feedbackIn;
	} else if (num == 1) {
		feedback[0] = feedback[1];
		feedback[1] = feedbackIn;
	}
}

bool FmChannel::prepare(const OpnaRegisters& regs, unsigned chNum)
{
	// prepare all operators and determine if any of them is active
	bool active = false;
	const unsigned chOffs = OpnaRegisters::channelOffset(chNum);
	auto const& map = OpnaRegisters::OPERATOR_MAP[chNum];
	for (unsigned slot = 0; slot < 4; slot++) {
		if (ops[slot].prepare(regs, chOffs, OpnaRegisters::operatorOffset(map[slot]))) {
			active = true;
		}
	}

	return active;
}

void FmChannel::clock(uint32_t envCounter, int32_t lfoRawPm)
{
	// clock the feedback through
	feedback[0] = feedback[1];
	feedback[1] = feedbackIn;

	for (auto& op : ops) {
		op.clock(envCounter, lfoRawPm);
	}
}

void FmChannel::endCsmPulse()
{
	for (auto& op : ops) {
		op.endCsmPulse();
	}
}

// OPNA offers 8 connection algorithms for its 4 operators.
//
// The operators are computed in order, with the inputs pulled from
// an array of values (opout) that is populated as we go:
//    0 = 0
//    1 = O1
//    2 = O2
//    3 = O3
//    4 = (O4)
//    5 = O1+O2
//    6 = O1+O3
//    7 = O2+O3
//
// Each row lists the opout[] index that feeds operators 2, 3 and 4,
// and a mask of which of operators 1-3 also go to the output (operator 4
// always does).
struct AlgorithmOp { uint8_t op2in, op3in, op4in, carrierMask; };
constexpr auto ALGORITHM = [](uint8_t op2in, uint8_t op3in, uint8_t op4in, uint8_t op1out, uint8_t op2out, uint8_t op3out) {
	return AlgorithmOp{
		.op2in = op2in, .op3in = op3in, .op4in = op4in,
		.carrierMask = uint8_t(op1out | (op2out << 1) | (op3out << 2)),
	};
};
static constexpr std::array<AlgorithmOp, 8> algorithmOps = {
	ALGORITHM(1,2,3, 0,0,0),    //  0: O1 -> O2 -> O3 -> O4 -> out (O4)
	ALGORITHM(0,5,3, 0,0,0),    //  1: (O1 + O2) -> O3 -> O4 -> out (O4)
	ALGORITHM(0,2,6, 0,0,0),    //  2: (O1 + (O2 -> O3)) -> O4 -> out (O4)
	ALGORITHM(1,0,7, 0,0,0),    //  3: ((O1 -> O2) + O3) -> O4 -> out (O4)
	ALGORITHM(1,0,3, 0,1,0),    //  4: ((O1 -> O2) + (O3 -> O4)) -> out (O2+O4)
	ALGORITHM(1,1,1, 0,1,1),    //  5: ((O1 -> O2) + (O1 -> O3) + (O1 -> O4)) -> out (O2+O3+O4)
	ALGORITHM(1,0,0, 0,1,1),    //  6: ((O1 -> O2) + O3 + O4) -> out (O2+O3+O4)
	ALGORITHM(0,0,0, 1,1,1),    //  7: (O1 + O2 + O3 + O4) -> out (O1+O2+O3+O4)
};

FmChannel::OutputPlan FmChannel::makeOutputPlan(const OpnaRegisters& regs, unsigned chNum) const
{
	OutputPlan plan;
	const unsigned chOffs = OpnaRegisters::channelOffset(chNum);
	const auto& alg = algorithmOps[regs.chAlgorithm(chOffs)];
	plan.op2in = alg.op2in;
	plan.op3in = alg.op3in;
	plan.op4in = alg.op4in;
	plan.carrierMask = alg.carrierMask;
	plan.feedback = uint8_t(regs.chFeedback(chOffs));

	plan.outputMask = 0;
	if (regs.chOutput0(chOffs)) {
		plan.outputMask |= 1;
	}
	if (regs.chOutput1(chOffs)) {
		plan.outputMask |= 2;
	}
	return plan;
}

int32_t FmChannel::output4Op(const OutputPlan& plan, uint32_t amOffset)
{
	// operator 1 has optional self-feedback
	int32_t opmod = 0;
	if (plan.feedback != 0) {
		opmod = (feedback[0] + feedback[1]) >> (10 - plan.feedback);
	}

	// compute the 14-bit volume/value of operator 1 and update the feedback
	int32_t op1value = feedbackIn = ops[0].computeVolume(ops[0].phase() + opmod, amOffset);

	// now that the feedback has been computed, skip the rest if this channel
	// feeds no output; no need to do all this work for nothing
	if (plan.outputMask == 0) return 0;

	// populate the opout table
	std::array<int16_t, 8> opout;
	opout[0] = 0;
	opout[1] = op1value;

	// compute the 14-bit volume/value of operator 2
	opmod = opout[plan.op2in] >> 1;
	opout[2] = ops[1].computeVolume(ops[1].phase() + opmod, amOffset);
	opout[5] = opout[1] + opout[2];

	// compute the 14-bit volume/value of operator 3
	opmod = opout[plan.op3in] >> 1;
	opout[3] = ops[2].computeVolume(ops[2].phase() + opmod, amOffset);
	opout[6] = opout[1] + opout[3];
	opout[7] = opout[2] + opout[3];

	// compute the 14-bit volume/value of operator 4;
	// all algorithms consume OP4 output at a minimum
	opmod = opout[plan.op4in] >> 1;
	int32_t result = ops[3].computeVolume(ops[3].phase() + opmod, amOffset) >> OUTPUT_SHIFT;

	// optionally add OP1, OP2, OP3. computeVolume() cannot exceed the largest
	// power table entry, 8168, so even four unshifted carriers stay inside the
	// 16-bit range that the clamp here used to enforce.
	if (plan.carrierMask & 1) {
		result += opout[1] >> OUTPUT_SHIFT;
	}
	if (plan.carrierMask & 2) {
		result += opout[2] >> OUTPUT_SHIFT;
	}
	if (plan.carrierMask & 4) {
		result += opout[3] >> OUTPUT_SHIFT;
	}

	return result;
}


FmEngine::FmEngine(MSXMotherBoard& motherboard, std::string_view name)
	: irq(motherboard, strCat(name, ".IRQ"))
	, timers{Timer(motherboard.getScheduler(), 0),
	         Timer(motherboard.getScheduler(), 1)}
{
}

uint8_t FmEngine::setResetStatus(uint8_t set, uint8_t reset)
{
	statusReg = (statusReg | set) & ~reset;
	checkInterrupts();
	return statusReg;
}

void FmEngine::reset(EmuTime time)
{
	for (auto& timer : timers) {
		timer.cancel();
	}

	// reset all status bits
	setResetStatus(0, 0xff);
	irq.reset();

	// register type-specific initialization
	registers.reset();

	// explicitly write to the mode register since it has side-effects
	// QUESTION: old cores initialize this to 0x30 -- who is right?
	writeReg(OpnaRegisters::REG_MODE, 0, time);

	// reset the channels
	for (auto& chan : channels) {
		chan.reset();
	}
}

// Clock one channel across a stretch of samples with no prepare() in between.
// Write selects whether output is mixed.
template<bool WRITE, bool LFO>
static void synthesizeFmChannel(FmChannel& channel, OpnaRegisters& regs,
                                  float* buf [[maybe_unused]], unsigned num, uint32_t env,
                                  unsigned chNum)
{
	// Registers hold for the whole buffer, the AM shift among them. Without
	// the LFO walk the AM offset is constant as well, so it is read once too.
	FmChannel::OutputPlan plan;
	uint32_t lfoMaxCount = 0;
	uint32_t amShift = 0;
	uint32_t amOffset = 0;
	bool panLeft = false;
	bool panRight = false;
	if constexpr (LFO) {
		lfoMaxCount = regs.lfoMaxCount();
	}
	if constexpr (WRITE) {
		plan = channel.makeOutputPlan(regs, chNum);
		panLeft = (plan.outputMask & 1) != 0;
		panRight = (plan.outputMask & 2) != 0;
		amShift = regs.lfoAmShift(OpnaRegisters::channelOffset(chNum));
		if constexpr (!LFO) {
			amOffset = regs.lfoAmOffset(amShift);
		}
	}

	bool endCsm = true;
	auto tick = [&] {
		env = stepEgCounter(env);
		int32_t pm = 0;
		if constexpr (LFO) {
			pm = regs.clockLfo(lfoMaxCount);
			if constexpr (WRITE) {
				amOffset = regs.lfoAmOffset(amShift);
			}
		}
		channel.clock(env, pm);
		// CSM is a one-sample key-on. Release after that sample so a skipped
		// prepare() on later buffers cannot leave the operators stuck on.
		if (endCsm) {
			channel.endCsmPulse();
			endCsm = false;
		}
	};

	if constexpr (!WRITE) {
		for (unsigned index = 0; index < num; ++index) {
			tick();
		}
		return;
	}

	if (panLeft && panRight) {
		for (unsigned index = 0; index < num; ++index) {
			tick();
			float value = float(channel.output4Op(plan, amOffset));
			unsigned pos = index * 2;
			buf[pos + 0] += value;
			buf[pos + 1] += value;
		}
	} else if (panLeft) {
		for (unsigned index = 0; index < num; ++index) {
			tick();
			buf[index * 2] += float(channel.output4Op(plan, amOffset));
		}
	} else if (panRight) {
		for (unsigned index = 0; index < num; ++index) {
			tick();
			buf[index * 2 + 1] += float(channel.output4Op(plan, amOffset));
		}
	} else {
		for (unsigned index = 0; index < num; ++index) {
			tick();
			channel.output4Op(plan, amOffset);
		}
	}
}


void FmEngine::generate(std::span<float*, CHANNELS> buffers, unsigned num, uint32_t chanMask)
{
	static_assert(OPERATORS / CHANNELS == 4);

	// An empty buffer must not consume a pending key-on.
	if (num == 0) return;

	// Register writes and CSM key-on land before this call. A channel that is
	// quiet here cannot become active before the next generate(). The envelope
	// counter advances once per sample for ADPCM-A, even when no channel clocks.
	const uint32_t env0 = envCounter;
	const auto lfo0 = registers.saveLfo();
	const bool lfoEnabled = registers.lfoEnable();
	// Disabled LFO is a constant. One update serves every sample of the chunk.
	if (!lfoEnabled) {
		registers.holdDisabledLfo();
	}

	bool lfoWalked = false;
	for (unsigned chNum = 0; chNum < channels.size(); ++chNum) {
		auto& channel = channels[chNum];
		// prepare() applies a new key and rebuilds the operator cache. With no
		// register or key change, the envelope state already decides the path.
		bool audible = modified ? channel.prepare(registers, chNum) : channel.audible();
		if (channel.finished()) {
			buffers[chNum] = nullptr;
			// prepare() may have consumed CSM even on a channel that is already
			// silent at max release attenuation; still clear the one-shot.
			channel.endCsmPulse();
			channel.quiesceFeedback(num);
			continue;
		}
		bool enabled = bitfield(chanMask, chNum) != 0;
		bool mix = audible && enabled;
		// Phase modulation matters only while the note is still running. AM is
		// read only when the channel is mixed.
		const unsigned chOffs = OpnaRegisters::channelOffset(chNum);
		bool walkLfo = lfoEnabled && ((audible && registers.chLfoPmSens(chOffs) != 0)
			|| (mix && registers.chLfoAmSens(chOffs) != 0));
		if (walkLfo) {
			registers.restoreLfo(lfo0);
			lfoWalked = true;
		}
		// Closed pan still clocks and updates operator-1 feedback, but adds
		// nothing, so the mixer can skip the buffer.
		if (!mix || (!registers.chOutput0(chOffs) && !registers.chOutput1(chOffs))) {
			buffers[chNum] = nullptr;
		}
		if (mix && walkLfo) {
			synthesizeFmChannel<true, true>(channel, registers, buffers[chNum], num, env0, chNum);
		} else if (mix) {
			synthesizeFmChannel<true, false>(channel, registers, buffers[chNum], num, env0, chNum);
		} else if (walkLfo) {
			synthesizeFmChannel<false, true>(channel, registers, nullptr, num, env0, chNum);
		} else {
			synthesizeFmChannel<false, false>(channel, registers, nullptr, num, env0, chNum);
		}
	}

	envCounter = advanceEgCounter(env0, num);
	totalClocks = uint8_t(totalClocks + num);
	modified = false;
	if (lfoEnabled && !lfoWalked) {
		registers.restoreLfo(lfo0);
		const uint32_t lfoMaxCount = registers.lfoMaxCount();
		for (unsigned i = 0; i < num; ++i) {
			(void)registers.clockLfo(lfoMaxCount);
		}
	}
}

void FmEngine::writeReg(uint16_t regNum, uint8_t data, EmuTime time)
{
	// special case: writes to the mode register can impact IRQs;
	// schedule these writes to ensure ordering with timers
	// Consumed by the next generate(): rebuild caches and apply key changes.
	modified = true;

	if (regNum == OpnaRegisters::REG_MODE) {
		modeWrite(data, time);
		return;
	}

	// most writes are passive, consumed only when needed
	unsigned keyOnChannel;
	unsigned keyOnOpMask;
	if (registers.write(regNum, data, keyOnChannel, keyOnOpMask)) {
		// handle writes to the keyon register(s)
		if (keyOnChannel < channels.size()) {
			// normal channel on/off
			channels[keyOnChannel].keyOnOff(keyOnOpMask, KeyOnType::NORMAL);
		}
	}
}

uint8_t FmEngine::status() const
{
	return statusReg;
}

void FmEngine::updateTimer(unsigned tNum, bool enable, int32_t deltaClocks, EmuTime time)
{
	// if the timer is live, but not currently enabled, set the timer
	if (enable && !timerRunning[tNum]) {
		// period comes from the registers, and is different for each
		uint32_t period = (tNum == 0) ? (1024 - registers.timerAValue()) : 16 * (256 - registers.timerBValue());

		// caller can also specify a delta to account for other effects
		period += deltaClocks;

		// reset it
		scheduleTimer(tNum, period * OPERATORS * prescale, time);
		timerRunning[tNum] = true;
	} else if (!enable) {
		// if the timer is not live, ensure it is not enabled
		scheduleTimer(tNum, -1, time);
		timerRunning[tNum] = false;
	}
}

void FmEngine::setClockPrescale(uint32_t value, EmuTime time)
{
	if (value == prescale) return;
	prescale = value;
	// Wall-clock deadlines were computed with the old factor. Restart any
	// running timer for a full period at the new rate (remaining ticks are
	// not tracked separately).
	for (unsigned tNum = 0; tNum < timers.size(); ++tNum) {
		if (!timerRunning[tNum]) continue;
		timerRunning[tNum] = false;
		bool load = (tNum == 0) ? registers.loadTimerA() : registers.loadTimerB();
		updateTimer(tNum, load, 0, time);
	}
}

void FmEngine::scheduleTimer(unsigned timer, int32_t duration, EmuTime time)
{
	if (duration < 0) {
		timers[timer].cancel();
	} else {
		timers[timer].schedule(time + Clock<CLOCK>::duration(unsigned(duration)));
	}
}

FmEngine::Timer::Timer(Scheduler& scheduler_, uint8_t index_)
	: Schedulable(scheduler_)
	, index(index_)
{
}

void FmEngine::Timer::executeUntil(EmuTime time)
{
	// Same as OUTER(FmEngine, timers), plus one element when this is timers[1].
	auto addr = std::bit_cast<uintptr_t>(this) - offsetof(FmEngine, timers);
	if (index == 1) {
		addr -= sizeof(Timer);
	}
	auto& engine = *std::bit_cast<FmEngine*>(addr);
	engine.engineTimerExpired(index, time);
}

void FmEngine::engineTimerExpired(unsigned tNum, EmuTime time)
{
	assert(tNum == 0 || tNum == 1);

	// update status
	if (tNum == 0 && registers.enableTimerA()) {
		setResetStatus(STATUS_TIMER_A, 0);
	} else if (tNum == 1 && registers.enableTimerB()) {
		setResetStatus(STATUS_TIMER_B, 0);
	}

	// Timer A overflow in CSM mode keys channel 2. Flush the mixer first so
	// samples before this instant still use the old key state.
	if (tNum == 0 && registers.csm()) {
		OUTER(YM2608, fm).updateStream(time);
		modified = true;
		channels[2].keyOnOff(0xf, KeyOnType::CSM);
	}

	// reset
	timerRunning[tNum] = false;
	updateTimer(tNum, true, 0, time);
}

void FmEngine::checkInterrupts()
{
	irq.set((statusReg & irqMask) != 0);
}

void FmEngine::modeWrite(uint8_t data, EmuTime time)
{
	// actually write the mode register now
	uint32_t dummy1, dummy2;
	registers.write(OpnaRegisters::REG_MODE, data, dummy1, dummy2);

	// reset timer status
	uint8_t resetMask = 0;
	if (registers.resetTimerB()) {
		resetMask |= STATUS_TIMER_B;
	}
	if (registers.resetTimerA()) {
		resetMask |= STATUS_TIMER_A;
	}
	setResetStatus(0, resetMask);

	// load timers; note that timer B gets a small negative adjustment because
	// the *16 multiplier is free-running, so the first tick of the clock
	// is a bit shorter
	updateTimer(1, registers.loadTimerB(), -(totalClocks & 15), time);
	updateTimer(0, registers.loadTimerA(), 0, time);
}


void OpnaRegisters::holdDisabledLfo()
{
	lfoCounter = 0;
	lfoAm = 0x3f;
}

uint32_t OpnaRegisters::lfoMaxCount() const
{
	// this table is based on converting the frequencies in the applications
	// manual to clock dividers, based on the assumption of a 7-bit LFO value
	static constexpr std::array<uint8_t, 8> lfoMaxCounts = {109, 78, 72, 68, 63, 45, 9, 6};
	return lfoMaxCounts[lfoRate()];
}

OpnaRegisters::LfoState OpnaRegisters::saveLfo() const
{
	return {.counter = lfoCounter, .am = lfoAm};
}

void OpnaRegisters::restoreLfo(LfoState state)
{
	lfoCounter = state.counter;
	lfoAm = state.am;
}

uint32_t OpnaRegisters::lfoAmShift(unsigned chOffs) const
{
	// shift value for AM sensitivity is [7, 3, 1, 0],
	// mapping to values of [0, 1.4, 5.9, and 11.8dB]
	return (1 << (chLfoAmSens(chOffs) ^ 3)) - 1;
}

void OpnaRegisters::reset()
{
	std::ranges::fill(regData, 0);

	// enable output on both channels by default
	regData[0x0b4] = regData[0x0b5] = regData[0x0b6] = 0xc0;
	regData[0x1b4] = regData[0x1b5] = regData[0x1b6] = 0xc0;
}

bool OpnaRegisters::write(uint16_t index, uint8_t data, unsigned& channel, unsigned& opMask)
{
	assert(index < REGISTERS);

	// writes in the 0xa0-af/0x1a0-af region are handled as latched pairs
	// borrow unused registers 0xb8-bf as temporary holding locations
	if ((index & 0xf0) == 0xa0) {
		if (bitfield(index, 0, 2) == 3) return false;

		uint32_t latchIndex = 0xb8 | bitfield(index, 3);

		// writes to the upper half just latch (only low 6 bits matter)
		if (bitfield(index, 2)) {
			regData[latchIndex] = data & 0x3f;
		} else {
			// writes to the lower half also apply said latch
			regData[index] = data;
			regData[index | 4] = regData[latchIndex];
		}
		return false;
	} else if ((index & 0xf8) == 0xb8) {
		// registers 0xb8-0xbf are used internally
		return false;
	}

	// everything else is normal
	regData[index] = data;

	// handle writes to the key on index
	if (index == 0x28) {
		channel = bitfield(data, 0, 2);
		if (channel == 3) return false;
		channel += bitfield(data, 2, 1) * 3;
		opMask = bitfield(data, 4, 4);
		return true;
	}
	return false;
}

// Caller has checked that LFO is enabled.
int32_t OpnaRegisters::clockLfo(uint32_t maxCount)
{
	uint32_t subCount = uint8_t(lfoCounter++);

	// when we cross the divider count, add enough to zero it and cause an
	// increment at bit 8; the 7-bit value lives from bits 8-14
	if (subCount >= maxCount) {
		// note: to match the published values this should be 0x100 - subcount;
		// however, tests on the hardware and nuked bear out an off-by-one
		// error exists that causes the max LFO rate to be faster than published
		lfoCounter += 0x101 - subCount;
	}

	// AM value is 7 bits, staring at bit 8; grab the low 6 directly
	lfoAm = bitfield(lfoCounter, 8, 6);

	// first half of the AM period (bit 6 == 0) is inverted
	if (bitfield(lfoCounter, 8+6) == 0) {
		lfoAm ^= 0x3f;
	}

	// PM value is 5 bits, starting at bit 10; grab the low 3 directly
	int32_t pm = bitfield(lfoCounter, 10, 3);

	// PM is reflected based on bit 3
	if (bitfield(lfoCounter, 10+3)) {
		pm ^= 7;
	}

	// PM is negated based on bit 4
	return bitfield(lfoCounter, 10+4) ? -pm : pm;
}

uint32_t OpnaRegisters::lfoAmOffset(uint32_t amShift) const
{
	// QUESTION: max sensitivity should give 11.8dB range, but this value
	// is directly added to an x.8 attenuation value, which will only give
	// 126/256 or ~4.9dB range -- what am I missing? The calculation below
	// matches several other emulators, including the Nuked implementation.

	// raw LFO AM value on OPN is 0-3F, scale that up by a factor of 2
	// (giving 7 bits) before applying the final shift
	return (lfoAm << 1) >> amShift;
}

void OpnaRegisters::cacheOperatorData(unsigned chOffs, unsigned opOffs, OpDataCache& cache) const
{
	// get frequency from the channel
	uint32_t blockFreq = cache.blockFreq = chBlockFreq(chOffs);

	// if multi-frequency mode is enabled and this is channel 2,
	// fetch one of the special frequencies
	if (multiFreq() && chOffs == 2) {
		if (opOffs == 2) {
			blockFreq = cache.blockFreq = multiBlockFreq(1);
		} else if (opOffs == 10) {
			blockFreq = cache.blockFreq = multiBlockFreq(2);
		} else if (opOffs == 6) {
			blockFreq = cache.blockFreq = multiBlockFreq(0);
		}
	}

	// compute the keycode: blockFreq is:
	//
	//     BBBFFFFFFFFFFF
	//     ^^^^???
	//
	// the 5-bit keycode uses the top 4 bits plus a magic formula
	// for the final bit
	uint32_t keycode = bitfield(blockFreq, 10, 4) << 1;

	// lowest bit is determined by a mix of next lower FNUM bits
	// according to this equation from the YM2608 manual:
	//
	//   (F11 & (F10 | F9 | F8)) | (!F11 & F10 & F9 & F8)
	//
	// for speed, we just look it up in a 16-bit constant
	keycode |= bitfield(0xfe80, bitfield(blockFreq, 7, 4));

	// detune adjustment
	cache.detune = detuneAdjustment(opDetune(opOffs), keycode);

	// multiple value, as an x.1 value (0 means 0.5)
	cache.multiple = opMultiple(opOffs) * 2;
	if (cache.multiple == 0) {
		cache.multiple = 1;
	}

	// LFO PM sensitivity, read per sample otherwise
	cache.lfoPmSens = uint8_t(chLfoPmSens(chOffs));

	// phase step, or PHASE_STEP_DYNAMIC if PM is active; this depends on
	// blockFreq, detune, and multiple, so compute it after we've done those
	if (!lfoEnable() || cache.lfoPmSens == 0) {
		cache.phaseStep = computePhaseStep(cache, 0);
	} else {
		cache.phaseStep = OpDataCache::PHASE_STEP_DYNAMIC;
	}

	// total level, scaled by 8
	cache.totalLevel = opTotalLevel(opOffs) << 3;

	// 4-bit sustain level, but 15 means 31 so effectively 5 bits
	cache.egSustain = opSustainLevel(opOffs);
	cache.egSustain |= (cache.egSustain + 1) & 0x10;
	cache.egSustain <<= 5;

	// determine KSR adjustment for envelope rates
	uint32_t ksrVal = keycode >> (opKsr(opOffs) ^ 3);
	cache.egRate[std::to_underlying(EnvelopeState::ATTACK)] = effectiveRate(opAttackRate(opOffs) * 2, ksrVal);
	cache.egRate[std::to_underlying(EnvelopeState::DECAY)] = effectiveRate(opDecayRate(opOffs) * 2, ksrVal);
	cache.egRate[std::to_underlying(EnvelopeState::SUSTAIN)] = effectiveRate(opSustainRate(opOffs) * 2, ksrVal);
	cache.egRate[std::to_underlying(EnvelopeState::RELEASE)] = effectiveRate(opReleaseRate(opOffs) * 4 + 2, ksrVal);

	// SSG-EG shape and the LFO AM enable, read per sample otherwise
	cache.ssgEgMode = uint8_t(opSsgEgMode(opOffs));
	cache.ssgEgEnable = opSsgEgEnable(opOffs);
	cache.lfoAmEnable = opLfoAmEnable(opOffs);
}

uint32_t OpnaRegisters::computePhaseStep(const OpDataCache& cache, int32_t lfoRawPm)
{
	// OPN phase calculation has only a single detune parameter
	// and uses FNUMs instead of keycodes

	// extract frequency number (low 11 bits of blockFreq)
	uint32_t fnum = bitfield(cache.blockFreq, 0, 11) << 1;

	// if there's a non-zero PM sensitivity, compute the adjustment
	if (cache.lfoPmSens != 0) {
		// apply the phase adjustment based on the upper 7 bits
		// of FNUM and the PM depth parameters
		fnum += opnLfoPmPhaseAdjustment(bitfield(cache.blockFreq, 4, 7), cache.lfoPmSens, lfoRawPm);

		// keep fnum to 12 bits
		fnum &= 0xfff;
	}

	// apply block shift to compute phase step
	uint32_t block = bitfield(cache.blockFreq, 11, 3);
	uint32_t phaseStep = (fnum << block) >> 2;

	// apply detune based on the keycode
	phaseStep += cache.detune;

	// clamp to 17 bits in case detune overflows
	// QUESTION: is this specific to the YM2612/3438?
	phaseStep &= 0x1ffff;

	// apply frequency multiplier (which is cached as an x.1 value)
	return (phaseStep * cache.multiple) >> 1;
}


void AdpcmARegisters::reset()
{
	std::ranges::fill(regData, 0);

	// initialize the pans to on by default, and max instrument volume;
	// some neogeo homebrews (for example ffeast) rely on this
	regData[0x08] = regData[0x09] = regData[0x0a] = 0xdf;
	regData[0x0b] = regData[0x0c] = regData[0x0d] = 0xdf;
}


void AdpcmAChannel::reset()
{
	playing = false;
	curNibble = 0;
	curByte = 0;
	curAddress = 0;
	accumulator = 0;
	stepIndex = 0;
}

bool AdpcmAChannel::resting() const
{
	return !playing && accumulator == 0;
}

int16_t AdpcmAChannel::sample(const OutputPlan& plan) const
{
	// accumulator is a 12-bit value; shift up to sign-extend;
	// the downshift is incorporated into the plan's shift
	return int16_t(((int16_t(accumulator << 4) * plan.mul) >> plan.shift) & ~3);
}

void AdpcmAChannel::keyOnOff(bool on, uint32_t start)
{
	// QUESTION: repeated key ons restart the sample?
	playing = on;
	if (playing) {
		curAddress = start;
		curNibble = 0;
		curByte = 0;
		accumulator = 0;
		stepIndex = 0;
	}
}

void AdpcmAChannel::clock(uint32_t end)
{
	// if not playing, hold a zero sample
	if (!playing) {
		accumulator = 0;
		return;
	}

	// if we're about to read nibble 0, fetch the data
	uint8_t data;
	if (curNibble == 0) {
		// stop when we hit the end address; apparently only low 20 bits are used for
		// comparison on the YM2610: this affects sample playback in some games, for
		// example twinspri character select screen music will skip some samples if
		// this is not correct
		//
		// note also: end address is inclusive, so wait until we are about to fetch
		// the sample just after the end before stopping; this is needed for nitd's
		// jump sound, for example
		uint32_t endAddr = end + 1;
		if (((curAddress ^ endAddr) & 0xfffff) == 0) {
			playing = false;
			accumulator = 0;
			return;
		}

		curByte = YM2608_ADPCM_ROM[curAddress++ & 0x1fff];
		data = curByte >> 4;
		curNibble = 1;
	} else {
		// otherwise just extract from the previously-fetched byte
		data = curByte & 0xf;
		curNibble = 0;
	}

	// compute the ADPCM delta
	static constexpr std::array<uint16_t, 49> adpcmSteps = {
		 16,  17,   19,   21,   23,   25,   28,
		 31,  34,   37,   41,   45,   50,   55,
		 60,  66,   73,   80,   88,   97,  107,
		118, 130,  143,  157,  173,  190,  209,
		230, 253,  279,  307,  337,  371,  408,
		449, 494,  544,  598,  658,  724,  796,
		876, 963, 1060, 1166, 1282, 1411, 1552,
	};
	int32_t delta = (2 * bitfield(data, 0, 3) + 1) * adpcmSteps[stepIndex] / 8;
	if (bitfield(data, 3)) {
		delta = -delta;
	}

	// the 12-bit accumulator wraps on the ym2610 and ym2608 (like the msm5205)
	accumulator = int16_t((accumulator + delta) & 0xfff);

	// adjust ADPCM step
	static constexpr std::array<int8_t, 8> stepInc = {-1, -1, -1, -1, 2, 5, 7, 9};
	stepIndex = int8_t(std::clamp(stepIndex + stepInc[bitfield(data, 0, 3)], 0, 48));
}

bool AdpcmAChannel::silent(const AdpcmARegisters& regs, unsigned chNum) const
{
	// A stopped channel forces the accumulator to 0 on the next clock that
	// includes it. Until that clock, sample() still emits the held value.
	// Key-on only happens from a register write.
	if (!playing && accumulator == 0) return true;

	// Instrument level, total level and pan are registers. clock() does
	// not change them, and a write ends the current buffer first.
	return makeOutputPlan(regs, chNum).panMask == 0;
}

AdpcmAChannel::OutputPlan AdpcmAChannel::makeOutputPlan(
	const AdpcmARegisters& regs, unsigned chNum) const
{
	OutputPlan plan;

	// volume combines instrument and total levels
	int vol = (regs.chInstrumentLevel(chNum) ^ 0x1f) + (regs.totalLevel() ^ 0x3f);

	// convert into a shift and a multiplier
	// QUESTION: verify this from other sources
	plan.mul = 15 - (vol & 7);
	plan.shift = 4 + 1 + (vol >> 3);

	// a maximum combined volume adds nothing, and neither does a closed pan
	plan.panMask = 0;
	if (vol < 63) {
		if (regs.chPanLeft(chNum)) {
			plan.panMask |= 1;
		}
		if (regs.chPanRight(chNum)) {
			plan.panMask |= 2;
		}
	}
	return plan;
}


void AdpcmAEngine::reset()
{
	registers.reset();

	for (auto& chan : channels) {
		chan.reset();
	}
}

void AdpcmAEngine::generate(std::span<float*, CHANNELS> buffers, unsigned num, uint32_t envStart)
{
	for (unsigned chNum = 0; chNum < channels.size(); ++chNum) {
		auto& channel = channels[chNum];
		if (channel.resting()) continue;
		// Volume and pan are registers, so they are read once per buffer.
		// An empty pan mask means this channel adds nothing.
		const auto plan = channel.makeOutputPlan(registers, chNum);
		float* buf = (plan.panMask != 0) ? buffers[chNum] : nullptr;
		uint32_t env = envStart;
		// Channels 0-3 clock on every ADPCM tick. Channels 4-5 clock on
		// every other tick, when envelope bit 2 is clear.
		const bool low = chNum < 4;
		const uint32_t end = registers.chEnd(chNum);
		if (buf == nullptr) {
			for (unsigned i = 0; i < num; ++i) {
				env = stepEgCounter(env);
				if ((env & 3) == 0 && (low || (env & 4) == 0)) {
					channel.clock(end);
				}
			}
		} else {
			// Only clock() changes the accumulator, so the scaled sample is
			// recomputed there instead of once per sample. A closed pan side
			// keeps adding zero.
			const bool panLeft  = (plan.panMask & 1) != 0;
			const bool panRight = (plan.panMask & 2) != 0;
			float left  = 0.0f;
			float right = 0.0f;
			auto rescale = [&] {
				float value = float(channel.sample(plan));
				left  = panLeft  ? value : 0.0f;
				right = panRight ? value : 0.0f;
			};
			rescale();
			for (unsigned i = 0; i < num; ++i) {
				env = stepEgCounter(env);
				if ((env & 3) == 0 && (low || (env & 4) == 0)) {
					channel.clock(end);
					rescale();
				}
				unsigned pos = i * 2;
				buf[pos + 0] += left;
				buf[pos + 1] += right;
			}
		}
	}
}

void AdpcmAEngine::writeReg(uint32_t regNum, uint8_t data)
{
	// store the raw value to the register array;
	// most writes are passive, consumed only when needed
	registers.write(regNum, data);

	// actively handle writes to the control register
	if (regNum == 0x00) {
		for (unsigned chNum = 0; chNum < channels.size(); ++chNum) {
			if (bitfield(data, chNum)) {
				channels[chNum].keyOnOff(bitfield(~data, 7) != 0, registers.chStart(chNum));
			}
		}
	}
}

void AdpcmAEngine::setStartEnd(unsigned chNum, uint16_t start, uint16_t end)
{
	registers.write(chNum + 0x10, uint8_t(start));
	registers.write(chNum + 0x18, uint8_t(start >> 8));
	registers.write(chNum + 0x20, uint8_t(end));
	registers.write(chNum + 0x28, uint8_t(end >> 8));
}


void AdpcmBRegisters::reset()
{
	std::ranges::fill(regData, 0);

	// default limit to wide open
	regData[0x0c] = regData[0x0d] = 0xff;
}


AdpcmBChannel::AdpcmBChannel(Ram& ram_, AdpcmBRegisters& regs)
	: registers(regs), ram(ram_)
{
}

void AdpcmBChannel::reset()
{
	statusReg = STATUS_BRDY;
	curNibble = 0;
	curByte = 0;
	dummyRead = 0;
	position = 0;
	curAddress = 0;
	accumulator = 0;
	prevAccum = 0;
	adpcmStep = STEP_MIN;
	cpuWriteActive = false;
}

bool AdpcmBChannel::resting() const
{
	return (statusReg & STATUS_PLAYING) == 0 && accumulator == 0 && prevAccum == 0;
}

AdpcmBChannel::OutputPlan AdpcmBChannel::makeOutputPlan() const
{
	return {.level = registers.level(), .panMask = panMask()};
}

int32_t AdpcmBChannel::sample(const OutputPlan& plan) const
{
	// do a linear interpolation between samples
	int32_t result = prevAccum + int32_t((int64_t(accumulator - prevAccum) * int32_t(position)) >> 16);

	// apply volume (level) in a linear fashion and reduce
	return (result * int32_t(plan.level)) >> 9;
}

bool AdpcmBChannel::decoding() const
{
	return registers.execute() && !registers.record() && (statusReg & STATUS_PLAYING) != 0;
}

bool AdpcmBChannel::advance(uint32_t delta)
{
	uint32_t nextPos = position + delta;
	position = uint16_t(nextPos);
	if (nextPos < 0x10000) return true;
	return consumeNibble();
}

uint8_t AdpcmBChannel::panMask() const
{
	if (registers.level() == 0) return 0;
	uint8_t mask = 0;
	if (registers.panLeft()) {
		mask |= 1;
	}
	if (registers.panRight()) {
		mask |= 2;
	}
	return mask;
}

bool AdpcmBChannel::atLimit() const
{
	return (curAddress == (((registers.limit() + 1) << addressShift()) - 1));
}

bool AdpcmBChannel::atEnd() const
{
	return (curAddress == (((registers.end() + 1) << addressShift()) - 1));
}

// One sample after the fractional position has wrapped.
bool AdpcmBChannel::consumeNibble()
{
	// if we're about to process nibble 0, fetch sample
	if (curNibble == 0) {
		// playing from RAM/ROM
		if (registers.external()) {
			curByte = ram[curAddress & 0x3ffff];
		}
	}

	// extract the nibble from our current byte
	uint8_t data = uint8_t(curByte << (4 * curNibble)) >> 4;
	curNibble ^= 1;

	// we just processed the last nibble
	if (curNibble == 0) {
		// if playing from RAM/ROM, check the end/limit address or advance
		if (registers.external()) {
			// handle the sample end, either repeating or stopping
			if (atEnd()) {
				// if repeating, go back to the start
				if (registers.repeat()) {
					loadStart();
				} else {
					// otherwise, done; set the EOS bit
					accumulator = 0;
					prevAccum = 0;
					statusReg = (statusReg & ~STATUS_PLAYING) | STATUS_EOS;
					return false;
				}
			} else if (atLimit()) {
				// wrap at the limit address
				curAddress = 0;
			} else {
				// otherwise, advance the current address
				curAddress++;
				curAddress &= 0xffffff;
			}
		} else {
			// if CPU-driven, copy the next byte and request more
			curByte = registers.cpuData();
			statusReg |= STATUS_BRDY;
		}
	}

	// remember previous value for interpolation
	prevAccum = accumulator;

	// forecast to next forecast: 1/8, 3/8, 5/8, 7/8, 9/8, 11/8, 13/8, 15/8
	int32_t delta = (2 * bitfield(data, 0, 3) + 1) * adpcmStep / 8;
	if (bitfield(data, 3)) {
		delta = -delta;
	}

	// add and clamp to 16 bits
	accumulator = int16_t(std::clamp(int(accumulator) + delta, -32768, 32767));

	// scale the ADPCM step: 0.9, 0.9, 0.9, 0.9, 1.2, 1.6, 2.0, 2.4
	static constexpr std::array<uint8_t, 8> stepScale = {57, 57, 57, 57, 77, 102, 128, 153};
	adpcmStep = int16_t(std::clamp(int(adpcmStep) * stepScale[bitfield(data, 0, 3)] / 64,
	                                  int(STEP_MIN), int(STEP_MAX)));
	return true;
}

// Several clocks, batching position steps.
void AdpcmBChannel::clockN(unsigned num)
{
	if (num == 0) return;

	// Not decoding: a clock only clears PLAYING. One store covers the run.
	if (!decoding()) {
		statusReg &= ~STATUS_PLAYING;
		return;
	}

	const uint32_t delta = registers.deltaN();
	// Adding zero never reaches the next nibble.
	if (delta == 0) return;

	while (num != 0) {
		// Clocks until and including the next 16-bit overflow.
		uint32_t room = 0x10000u - position;
		uint32_t steps = (room + delta - 1) / delta;
		if (steps > num) {
			position = uint16_t(uint32_t(position) + uint64_t(num) * delta);
			return;
		}
		// The low 16 bits are the position after the overflowing add.
		position = uint16_t(uint32_t(position) + uint64_t(steps) * delta);
		num -= steps;
		// End-without-repeat stops here. Later clocks would only clear
		// PLAYING, which consumeNibble() already cleared.
		if (!consumeNibble()) return;
	}
}

void AdpcmBChannel::generate(float* buffer, unsigned num, const OutputPlan& plan)
{
	if (num == 0) return;

	const bool panLeft  = (plan.panMask & 1) != 0;
	const bool panRight = (plan.panMask & 2) != 0;

	// The decode state and delta-N are registers, so they are read once.
	// Without decoding the position never moves, and neither does it without
	// a step, so in both cases the held sample repeats for the whole run.
	uint32_t delta = 0;
	if (decoding()) {
		delta = registers.deltaN();
	} else {
		statusReg &= ~STATUS_PLAYING;
	}

	auto tick = [&] {
		// End-without-repeat zeroes both interpolator ends, so the rest of
		// the run holds that zero.
		if (delta != 0 && !advance(delta)) {
			delta = 0;
		}
		return float(sample(plan));
	};

	if (panLeft && panRight) {
		for (unsigned i = 0; i < num; ++i) {
			float value = tick();
			buffer[2 * i + 0] += value;
			buffer[2 * i + 1] += value;
		}
	} else if (panLeft) {
		for (unsigned i = 0; i < num; ++i) {
			buffer[2 * i + 0] += tick();
		}
	} else {
		for (unsigned i = 0; i < num; ++i) {
			buffer[2 * i + 1] += tick();
		}
	}
}

bool AdpcmBChannel::silent() const
{
	// Not advancing: a clock returns before it touches the accumulators.
	// Playback itself starts only from a register write. A held sample is
	// silent only when both ends of the interpolator are already zero.
	if (!decoding() && accumulator == 0 && prevAccum == 0) return true;

	// Level and pan are registers. clock() does not change them, and a
	// write ends the current buffer first.
	return panMask() == 0;
}

uint8_t AdpcmBChannel::peek(uint32_t regNum) const
{
	// Observe the next CPU read without consuming dummy reads, advancing RAM,
	// changing EOS/BRDY or invoking a potentially destructive host read.
	if (regNum == 0x08 && !registers.execute() && !registers.record() && registers.external()) {
		if (cpuWriteActive) return registers.cpuData();
		if (dummyRead == 0) return ram[curAddress & 0x3ffff];
	}
	return 0;
}

uint8_t AdpcmBChannel::read(uint32_t regNum)
{
	uint8_t result = 0;

	// register 8 reads over the bus under some conditions
	if (regNum == 0x08 && !registers.execute() && !registers.record() && registers.external()) {
		// A mode change alone does not terminate an unfinished RAM writer.
		// Until RESET, the CPU sees its last data-buffer byte (Makoto V4).
		if (cpuWriteActive) return registers.cpuData();

		// two dummy reads are consumed first
		if (dummyRead != 0) {
			loadStart();
			dummyRead--;
		} else {
			// read the data
			// read from outside of the chip
			result = ram[curAddress & 0x3ffff];

			// did we hit the end? if so, signal EOS
			if (atEnd()) {
				statusReg = STATUS_EOS | STATUS_BRDY;
			} else {
				// signal ready
				statusReg = STATUS_BRDY;
			}

			// The limit is inclusive: consume its last byte before wrapping.
			if (atLimit()) {
				curAddress = 0;
			} else {
				curAddress++;
			}
		}
	}
	return result;
}

void AdpcmBChannel::write(uint32_t regNum, uint8_t value)
{
	// register 0 can do a reset; also use writes here to reset the
	// dummy read counter
	if (regNum == 0x00) {
		if (registers.execute()) {
			loadStart();
		} else {
			statusReg &= ~STATUS_EOS;
		}
		if (registers.resetFlag()) {
			reset();
		}
		if (registers.external()) {
			dummyRead = 2;
		}
	} else if (regNum == 0x08) {
		// register 8 writes over the bus under some conditions
		// if writing from the CPU during execute, clear the ready flag
		if (registers.execute() && !registers.record() && !registers.external()) {
			statusReg &= ~STATUS_BRDY;
		} else if (!registers.execute() && registers.record() && registers.external()) {
			// if writing during "record", pass through as data
			// clear out dummy reads and set start address
			if (dummyRead != 0) {
				loadStart();
				dummyRead = 0;
			}

			// The end register describes an inclusive chunk. Keep the CPU
			// address one past its last byte once the transfer has stopped.
			uint32_t end = (registers.end() + 1) << addressShift();
			if (curAddress != end) {
				ram[curAddress++ & 0x3ffff] = value;
				cpuWriteActive = true;
			}

			if (curAddress == end) {
				cpuWriteActive = false;
				statusReg = STATUS_EOS | STATUS_BRDY;
			} else {
				statusReg = STATUS_BRDY;
			}
		}
	}
}

uint32_t AdpcmBChannel::addressShift() const
{
	// if ROM or 8-bit DRAM, shift is 5 bits
	if (registers.romRam()) return 5;
	if (registers.dram8Bit()) return 5;

	// otherwise, shift is 2 bits
	return 2;
}

void AdpcmBChannel::loadStart()
{
	statusReg = (statusReg & ~STATUS_EOS) | STATUS_PLAYING;
	curAddress = registers.external() ? (registers.start() << addressShift()) : 0;
	curNibble = 0;
	curByte = 0;
	position = 0;
	accumulator = 0;
	prevAccum = 0;
	adpcmStep = STEP_MIN;
	cpuWriteActive = false;
}


AdpcmBEngine::AdpcmBEngine(const DeviceConfig& config, std::string_view name)
	: ram(config, strCat(name, " ADPCM RAM"), "YM2608 ADPCM-B sample RAM", 0x40000)
	, channel(ram, registers)
{
	ram.clear(0); // hardware power-on contents are unknown.
}

void AdpcmBEngine::reset()
{
	registers.reset();
	channel.reset();
}

void AdpcmBEngine::generate(float* buffer, unsigned num)
{
	if (channel.resting()) return;
	// Level and pan are registers, so they are read once per buffer. An empty
	// pan mask means this channel adds nothing.
	const auto plan = channel.makeOutputPlan();
	if (buffer == nullptr || plan.panMask == 0) {
		channel.clockN(num);
	} else {
		channel.generate(buffer, num, plan);
	}
}

void AdpcmBEngine::writeReg(uint32_t regNum, uint8_t data)
{
	// store the raw value to the register array;
	// most writes are passive, consumed only when needed
	registers.write(regNum, data);

	// let the channel handle any special writes
	channel.write(regNum, data);
}


template<typename Archive>
void OpnaRegisters::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("lfoCounter", lfoCounter,
	             "lfoAm",      lfoAm,
	             "regData",    regData);
}

template<typename Archive>
void FmOperator::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("phaseAcc",       phaseAcc,
	             "envAttenuation", envAttenuation,
	             "envState",       envState,
	             "ssgInverted",    ssgInverted,
	             "keyState",       keyState,
	             "keyOnLive",      keyOnLive);
}

template<typename Archive>
void FmChannel::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("feedback",   feedback,
	             "feedbackIn", feedbackIn,
	             "ops",        ops);
}

template<typename Archive>
void FmEngine::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("envCounter",   envCounter,
	             "statusReg",    statusReg,
	             "prescale",     prescale,
	             "timerRunning", timerRunning,
	             "totalClocks",  totalClocks,
	             "registers",    registers,
	             "channels",     channels,
	             "irq",          irq,
	             "timers",       timers);
	// Operator caches and irqMask are not saved. The next generate() rebuilds
	// the caches; YM2608::serialize restores irqMask from irqEnable/flagControl.
	modified = true;
}

template<typename Archive>
void FmEngine::Timer::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<Schedulable>(*this);
}

template<typename Archive>
void AdpcmARegisters::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("regData", regData);
}

template<typename Archive>
void AdpcmAChannel::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("curAddress",  curAddress,
	             "accumulator", accumulator,
	             "stepIndex",   stepIndex,
	             "playing",     playing,
	             "curNibble",   curNibble,
	             "curByte",     curByte);
}

template<typename Archive>
void AdpcmAEngine::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("registers", registers,
	             "channels",  channels);
}

template<typename Archive>
void AdpcmBRegisters::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("regData", regData);
}

template<typename Archive>
void AdpcmBChannel::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("curAddress",     curAddress,
	             "position",       position,
	             "accumulator",    accumulator,
	             "prevAccum",      prevAccum,
	             "adpcmStep",      adpcmStep,
	             "statusReg",      statusReg,
	             "curNibble",      curNibble,
	             "curByte",        curByte,
	             "dummyRead",      dummyRead,
	             "cpuWriteActive", cpuWriteActive);
}

template<typename Archive>
void AdpcmBEngine::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("registers", registers,
	             "channel",   channel,
	             "ram",       ram);
}

} // namespace ym2608


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

	reset(time);
}

void YM2608::reset(EmuTime time)
{
	updateStream(time);

	// reset the engines
	fm.reset(time);
	adpcmA.reset();
	adpcmB.reset();

	// fm.reset() does not restore clock divider or IRQ mask (same as ymfm).
	fm.setClockPrescale(6, time);

	// configure ADPCM percussion sounds; these are present in an embedded ROM
	adpcmA.setStartEnd(0, 0x0000, 0x01bf); // bass drum
	adpcmA.setStartEnd(1, 0x01c0, 0x043f); // snare drum
	adpcmA.setStartEnd(2, 0x0440, 0x1b7f); // top cymbal
	adpcmA.setStartEnd(3, 0x1b80, 0x1cff); // high hat
	adpcmA.setStartEnd(4, 0x1d00, 0x1f7f); // tom tom
	adpcmA.setStartEnd(5, 0x1f80, 0x1fff); // rim shot

	// initialize our special interrupt states, then read the upper status
	// register, which updates the IRQs
	irqEnable = 0x1f;
	flagControl = 0x1c;
	fm.setIrqMask(irqEnable & ~flagControl & 0x1f);
	(void)readStatusHi(time);

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
	default:
		UNREACHABLE; return 0;
	}
}

uint8_t YM2608::peekPort(unsigned port, EmuTime time) const
{
	// Debugger reads deliberately bypass readStatusHi()'s IRQ update and the
	// ADPCM data port's dummy reads, address advancement and flag changes.
	auto busy = (time < busyEnd) ? STATUS_BUSY : 0;
	switch (port & 3) {
	case 0:
		return (fm.status() & (ym2608::FmEngine::STATUS_TIMER_A | ym2608::FmEngine::STATUS_TIMER_B)) | busy;
	case 1:
		if (addressLatch < 0x10) {
			return ssg.peekRegister(addressLatch, time);
		}
		return (addressLatch == 0xff) ? 1 : 0;
	case 2:
		return statusHi() | busy;
	case 3:
		return ((addressLatch & 0xff) < 0x10) ? adpcmB.peek(addressLatch & 0x0f) : 0;
	default:
		UNREACHABLE; return 0;
	}
}


uint8_t YM2608::readStatus(EmuTime time)
{
	uint8_t result = fm.status() & (ym2608::FmEngine::STATUS_TIMER_A | ym2608::FmEngine::STATUS_TIMER_B);
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
	syncAdpcmBStatus();

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
	if ((adpcmStatus & ym2608::AdpcmBChannel::STATUS_EOS) != 0) {
		status |= STATUS_ADPCM_B_EOS;
	}
	if ((adpcmStatus & ym2608::AdpcmBChannel::STATUS_BRDY) != 0) {
		status |= STATUS_ADPCM_B_BRDY;
	}
	if ((adpcmStatus & ym2608::AdpcmBChannel::STATUS_PLAYING) != 0) {
		status |= STATUS_ADPCM_B_PLAYING;
	}

	// turn off any bits that have been requested to be masked
	status &= ~(flagControl & 0x1f);

	return status;
}

void YM2608::syncAdpcmBStatus()
{
	// ymfm only pushed ADPCM flags into the IRQ line when status-hi was read.
	// Raise or clear them whenever the ADPCM-B engine may have changed them.
	uint8_t status = statusHi();
	fm.setResetStatus(status, ~status);
}

uint8_t YM2608::readDataHi()
{
	if ((addressLatch & 0xff) < 0x10) {
		uint8_t result = adpcmB.read(addressLatch & 0x0f);
		syncAdpcmBStatus();
		return result;
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
				fm.setClockPrescale(6, time);
			} else if (addressLatch == 0x2e && fm.clockPrescale() == 6) {
				fm.setClockPrescale(3, time);
			} else if (addressLatch == 0x2f) {
				fm.setClockPrescale(2, time);
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
	default:
		UNREACHABLE;
	}

	applyRates(time);
}

void YM2608::writeRegister(unsigned regNum, uint8_t data, EmuTime time)
{
	updateStream(time);

	if (regNum < 0x10) {
		// 00-0F: write to SSG
		ssg.writeRegister(regNum, data, time);
	} else if (regNum < 0x20) {
		// 10-1F: write to ADPCM-A
		adpcmA.writeReg(regNum & 0x0f, data);
	} else if (regNum == 0x29) {
		// 29: special IRQ mask register
		irqEnable = data;
		fm.setIrqMask(irqEnable & ~flagControl & 0x1f);
	} else if (0x100 <= regNum && regNum < 0x110) {
		// 100-10F: write to ADPCM-B
		adpcmB.writeReg(regNum & 0x0f, data);
		syncAdpcmBStatus();
	} else if (regNum == 0x110) {
		// 110: IRQ flag control
		if (data & 0x80) {
			fm.setResetStatus(0, 0xff);
		} else {
			flagControl = data;
			fm.setIrqMask(irqEnable & ~flagControl & 0x1f);
		}
	} else {
		// 20-28, 2A-FF, 111-1FF: write to FM
		fm.writeReg(regNum, data, time);
	}

	// mark busy for a bit
	busyEnd = time + Clock<CLOCK>::duration(32 * fm.clockPrescale());
}

uint8_t YM2608::peekRegister(unsigned regNum, EmuTime time) const
{
	assert(regNum < 0x200);
	if (regNum < 0x10) {
		return ssg.peekRegister(regNum, time);
	} else if (regNum < 0x20) {
		return adpcmA.readReg(regNum & 0x0f);
	} else if (regNum == 0x29) {
		return irqEnable;
	} else if (0x100 <= regNum && regNum < 0x110) {
		return adpcmB.readReg(regNum & 0x0f);
	} else if (regNum == 0x110) {
		return flagControl;
	} else {
		return fm.readReg(regNum);
	}
}

void YM2608::updateStream(EmuTime time)
{
	fmPart.updateStream(time);
}

void YM2608::applyRates(EmuTime time)
{
	unsigned prescale = fm.clockPrescale();
	unsigned fmRate = (CLOCK + 12 * prescale) / (24 * prescale);
	unsigned ssgRate = CLOCK / (prescale == 6 ? 32
	                          : prescale == 3 ? 16
	                                          :  8);
	fmPart.rate(fmRate);
	ssg.setClockFrequency(float(8 * ssgRate), time);
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
	const uint32_t env = fm.envelopeCounter();
	// Bit 7 of register 0x29 enables FM channels 3-5. generate() nulls every
	// channel that is outside this mask or that prepare() finds already quiet.
	const uint32_t fmMask = (irqEnable & 0x80) ? 0x3f : 0x07;
	fm.generate(buffers.first<6>(), num, fmMask);
	adpcmB.generate(buffers[6], num);
	adpcmA.generate(buffers.subspan(7, 6).first<6>(), num, env);
	syncAdpcmBStatus();
}


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

static constexpr auto envelopeInfo = std::to_array<enum_string<ym2608::EnvelopeState>>({
	{ "ATTACK",  ym2608::EnvelopeState::ATTACK },
	{ "DECAY",   ym2608::EnvelopeState::DECAY },
	{ "SUSTAIN", ym2608::EnvelopeState::SUSTAIN },
	{ "RELEASE", ym2608::EnvelopeState::RELEASE },
});
SERIALIZE_ENUM(ym2608::EnvelopeState, envelopeInfo);

template<typename Archive>
void YM2608::serialize(Archive& ar, unsigned /*version*/)
{
	if constexpr (!Archive::IS_LOADER) {
		updateStream(fm.getCurrentTime());
	}
	ar.serialize("addressLatch", addressLatch,
	             "irqEnable",    irqEnable,
	             "flagControl",  flagControl,
	             "fm",           fm,
	             "adpcmA",       adpcmA,
	             "adpcmB",       adpcmB,
	             "busyEnd",      busyEnd,
	             "ssg",          ssg);
	if constexpr (Archive::IS_LOADER) {
		fm.setIrqMask(irqEnable & ~flagControl & 0x1f);
		applyRates(fm.getCurrentTime());
	}
}
INSTANTIATE_SERIALIZE_METHODS(YM2608);

} // namespace openmsx
