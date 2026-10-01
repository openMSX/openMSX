#ifndef MAKOTONATIVECHIP_HH
#define MAKOTONATIVECHIP_HH

#include "ReferenceYM2608.hh"
#include <array>

namespace openmsx {
// Native-stream approach proposed by Wouter Vermaelen and prototyped by
// madscient (openMSX_Y8960 d84efe46). Keep the pinned core and its RAM fixes.
class MakotoNativeChip final : public ymfm::ym2608_reference
{
public:
	using ym2608_reference::ym2608_reference;
	[[nodiscard]] unsigned prescale() const { return m_fm.clock_prescale(); }
	[[nodiscard]] unsigned fmRate() const { return (8000000 + 12 * prescale()) / (24 * prescale()); }
	[[nodiscard]] unsigned ssgRate() const { return 8000000 / (prescale() == 6 ? 32 : prescale() == 3 ? 16 : 8); }
	[[nodiscard]] std::array<int32_t, 2> clockFM() {
		clock_fm_and_adpcm();
		return {m_last_fm.data[0], m_last_fm.data[1]};
	}
	[[nodiscard]] std::array<int32_t, 3> clockSSG() {
		ymfm::ssg_engine::output_data output;
		m_ssg.clock();
		m_ssg.output(output);
		return {output.data[0], output.data[1], output.data[2]};
	}
};
} // namespace openmsx
#endif
