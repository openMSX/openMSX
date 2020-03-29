#include "YM2413OriginalNukeYKT.hh"
#include "serialize.hh"
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
	for (int i = 0; i < 64; ++i) {
		regs[i] = 0;
	}
}

void YM2413::generateChannels(float* out_[9 + 5], uint32_t n)
{
	float* out[9 + 5];
	for (int i = 0; i < 9 + 5; ++i) out[i] = out_[i];

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
	for (uint32_t i = 0; i < 18; ++i) {
		auto& write = writes[i];
		if (write.port != uint8_t(-1)) {
			OPLL_Write(&opll, write.port, write.value);
			write.port = uint8_t(-1);
		}
		f();
	}
	n = (n - 1) * 18;
	for (uint32_t i = 0; i < n; ++i) {
		f();
	}
}

void YM2413::writePort(bool port, uint8_t value, int cycle_offset)
{
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
