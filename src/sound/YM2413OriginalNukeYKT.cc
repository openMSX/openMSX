#include "YM2413OriginalNukeYKT.hh"

#include "narrow.hh"
#include "ranges.hh"
#include "serialize.hh"
#include "xrange.hh"

#include <algorithm>
#include <array>
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
	std::ranges::fill(regs, 0);
}

void YM2413::generateChannels(std::span<float*, 9 + 5> out_, uint32_t n)
{
	std::array<float*, 9 + 5> out;
	copy_to_range(out_, out);

	auto f = [&] {
		std::array<int32_t, 2> buf;
		OPLL_Clock(&opll, buf.data());
		switch (opll.cycles) {
			case  0: *out[ 9]++ += narrow_cast<float>(buf[1] * 2); break;
			case  1: *out[10]++ += narrow_cast<float>(buf[1] * 2); break;
			case  2: *out[ 6]++ += narrow_cast<float>(buf[0]);
				 *out[11]++ += narrow_cast<float>(buf[1] * 2); break;
			case  3: *out[ 7]++ += narrow_cast<float>(buf[0]);
				 *out[12]++ += narrow_cast<float>(buf[1] * 2); break;
			case  4: *out[ 8]++ += narrow_cast<float>(buf[0]);
				 *out[13]++ += narrow_cast<float>(buf[1] * 2); break;
			case  8: *out[ 0]++ += narrow_cast<float>(buf[0]);     break;
			case  9: *out[ 1]++ += narrow_cast<float>(buf[0]);     break;
			case 10: *out[ 2]++ += narrow_cast<float>(buf[0]);     break;
			case 14: *out[ 3]++ += narrow_cast<float>(buf[0]);     break;
			case 15: *out[ 4]++ += narrow_cast<float>(buf[0]);     break;
			case 16: *out[ 5]++ += narrow_cast<float>(buf[0]);     break;
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
			std::array<float*, 9 + 5> dummy;
			std::ranges::fill(dummy, &d);
			generateChannels(dummy, 1);
		}
		allowed_offset = ((port ? 84 : 12) / 4) + cycle_offset;
	}

	writes[cycle_offset] = {.port = port, .value = value};

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

template<typename Archive>
void YM2413::serialize(Archive& /*ar*/, unsigned /*version*/)
{
	// Not implemented
}

} // namespace OriginalNuke

using YM2413OriginalNukeYKT::YM2413;
INSTANTIATE_SERIALIZE_METHODS(YM2413);
REGISTER_POLYMORPHIC_INITIALIZER(YM2413Core, YM2413, "YM2413-Original-NukeYKT");

} // namespace openmsx
