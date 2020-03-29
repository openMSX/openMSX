/* This is a wrapper around the original (unmodified) NukeYKT code. This is
 * only useful for debugging because our modified/optimized NukeYKT code should
 * generate identical output.
 */
#ifndef YM2413ORIGINAL_HPP
#define YM2413ORIGINAL_HPP

#include "YM2413Core.hh"
#include "opll.hh"

namespace openmsx {
namespace YM2413OriginalNukeYKT {

class YM2413 : public YM2413Core
{
public:
	YM2413();
	void reset() override;
	void writePort(bool port, uint8_t value, int cycle_offset) override;
	void pokeReg(uint8_t reg, uint8_t value) override;
	uint8_t peekReg(uint8_t reg) const override;
	void generateChannels(float* out[9 + 5], uint32_t n) override;
	float getAmplificationFactor() const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	opll_t opll;
	struct Write {
		uint8_t port = uint8_t(-1);
		uint8_t value;
	} writes[18];

	// only used for peekReg();
	uint8_t regs[64];
	uint8_t latch;
};

} // namespace OriginalNuke
} // namespace openmsx

#endif
