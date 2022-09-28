/* This is a wrapper around the original (unmodified) NukeYKT code. This is
 * only useful for debugging because our modified/optimized NukeYKT code should
 * generate identical output.
 */
#ifndef YM2413ORIGINAL_HPP
#define YM2413ORIGINAL_HPP

#include "YM2413Core.hh"
#include "opll.hh"
#include <array>

namespace openmsx::YM2413OriginalNukeYKT {

class YM2413 final : public YM2413Core
{
public:
	YM2413();
	void reset() override;
	void writePort(bool port, uint8_t value, int cycle_offset) override;
	void pokeReg(uint8_t reg, uint8_t value) override;
	[[nodiscard]] uint8_t peekReg(uint8_t reg) const override;
	void generateChannels(float* out[9 + 5], uint32_t n) override;
	[[nodiscard]] float getAmplificationFactor() const override;
	void setSpeed(double speed) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	opll_t opll;
	struct Write {
		uint8_t port = uint8_t(-1);
		uint8_t value;
	};
	std::array<Write, 18> writes;

	// only used for peekReg();
	std::array<uint8_t, 64> regs;
	uint8_t latch;

	int allowed_offset = 0;
	bool speedUpHack = false;
};

} // namespace openmsx::OriginalNuke

#endif
