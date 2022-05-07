#include "YM2413OriginalNukeYKT.hh"
#include "ranges.hh"
#include "serialize.hh"
#include "xrange.hh"
#include <cassert>

namespace openmsx {
namespace YM2413OriginalNukeYKT {

YM2413::YM2413()
{
	reset();
}

void YM2413::reset()
{
	OPLL_Reset(&opll, opll_type_ym2413b);
	ranges::fill(regs, 0);
}

void YM2413::generateChannels(float* out_[9 + 5], uint32_t n)
{
	float* out[9 + 5];
	std::copy_n(out_, 9 + 5, out);

	auto f = [&] {
		int32_t buf[2];
		OPLL_Clock(&opll, buf);
		switch (opll.cycles) {
			case  0: *out[ 9]++ += buf[1] * 2; break;
			case  1: *out[10]++ += buf[1] * 2; break;
			case  2: *out[ 6]++ += buf[0];
				 *out[11]++ += buf[1] * 2; break;
			case  3: *out[ 7]++ += buf[0];
				 *out[12]++ += buf[1] * 2; break;
			case  4: *out[ 8]++ += buf[0];
				 *out[13]++ += buf[1] * 2; break;
			case  8: *out[ 0]++ += buf[0];     break;
			case  9: *out[ 1]++ += buf[0];     break;
			case 10: *out[ 2]++ += buf[0];     break;
			case 14: *out[ 3]++ += buf[0];     break;
			case 15: *out[ 4]++ += buf[0];     break;
			case 16: *out[ 5]++ += buf[0];     break;
		}
	};

	assert(n != 0);
	for (auto& write : writes) {
		if (write.port != uint8_t(-1)) {
			OPLL_Write(&opll, write.port, write.value);
			write.port = uint8_t(-1);
		}
		f();
	}
	repeat((n - 1) * 18, f);

	allowed_offset = std::max<int>(0, allowed_offset - 18); // see writePort()
}

void YM2413::writePort(bool port, uint8_t value, int cycle_offset)
{
	// see comments in YM2413NukeYKT.cc
	if (speedUpHack) [[unlikely]] {
		while (cycle_offset < allowed_offset) [[unlikely]] {
			float d = 0.0f;
			float* dummy[9 + 5] = {
				&d, &d, &d, &d, &d, &d, &d, &d, &d,
				&d, &d, &d, &d, &d,
			};
			generateChannels(dummy, 1);
		}
		allowed_offset = ((port ? 84 : 12) / 4) + cycle_offset;
	}

	writes[cycle_offset] = {port, value};

	// only needed for peekReg()
	if (port == 0) {
		latch = value & 63;
	} else {
		regs[latch] = value;
	}
}

void YM2413::pokeReg(uint8_t /*reg*/, uint8_t /*value*/)
{
	// not supported
}

uint8_t YM2413::peekReg(uint8_t reg) const
{
	return regs[reg & 63];
}

float YM2413::getAmplificationFactor() const
{
	return 1.0f / 256.0f;
}

void YM2413::setSpeed(double speed)
{
	speedUpHack = speed > 1.0;
}

void YM2413::serialize(Archive auto& /*ar*/, unsigned /*version*/)
{
	// Not implemented
}

} // namespace OriginalNuke

using YM2413OriginalNukeYKT::YM2413;
INSTANTIATE_SERIALIZE_METHODS(YM2413);
REGISTER_POLYMORPHIC_INITIALIZER(YM2413Core, YM2413, "YM2413-Original-NukeYKT");

} // namespace openmsx
