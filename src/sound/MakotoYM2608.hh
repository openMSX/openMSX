// BSD 3-Clause License
//
// Copyright (c) 2021, Aaron Giles
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Native YM2608 control layer adapted for openMSX; synthesis remains in YMFM.
#ifndef MAKOTOYM2608_HH
#define MAKOTOYM2608_HH

#include "3rdparty/ymfm/ymfm_opn.h"
#include <span>

namespace openmsx {
class MakotoYM2608 final
{
	static constexpr uint8_t STATUS_ADPCM_B_EOS = 0x04;
	static constexpr uint8_t STATUS_ADPCM_B_BRDY = 0x08;
	static constexpr uint8_t STATUS_ADPCM_B_PLAYING = 0x20;
	using fm_engine = ymfm::fm_engine_base<ymfm::opna_registers>;

public:
	explicit MakotoYM2608(ymfm::ymfm_interface& intf);
	void reset();
	template <typename Archive> void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.serialize("address", address, "irqEnable", irqEnable, "flagControl", flagControl);
		serializeEngine(ar, "fm", fm);
		serializeEngine(ar, "ssg", ssg);
		serializeEngine(ar, "adpcmA", adpcmA);
		serializeEngine(ar, "adpcmB", adpcmB);
		if constexpr (Archive::IS_LOADER) {
			updatePrescale(fm.clock_prescale());
		}
	}
	void invalidateCaches() { fm.invalidate_caches(); }
	[[nodiscard]] unsigned prescale() const { return fm.clock_prescale(); }
	[[nodiscard]] unsigned fmRate() const { return (8000000 + 12 * prescale()) / (24 * prescale()); }
	[[nodiscard]] unsigned ssgRate() const
	{
		return 8000000 / (prescale() == 6 ? 32 : prescale() == 3 ? 16 : 8);
	}
	uint8_t read(uint32_t offset);
	[[nodiscard]] uint8_t peek(uint32_t offset, bool busy) const;
	[[nodiscard]] uint8_t peekRegister(uint16_t regnum) const;
	void write(uint32_t offset, uint8_t data);
	void writeRegister(uint16_t regnum, uint8_t data);
	void generateFM(std::span<float*> buffers, unsigned num);
	void generateSSG(std::span<float*> buffers, unsigned num);

private:
	template <typename Archive, typename Engine>
	static void serializeEngine(Archive& ar, const char* name, Engine& engine)
	{
		// Obtain the pinned engine's exact byte count. The archive's blob reader
		// rejects a mismatched length instead of YMFM silently zero-filling it.
		std::vector<uint8_t> data;
		ymfm::ymfm_saved_state saved(data, true);
		engine.save_restore(saved);
		ar.serialize_blob(name, std::span<uint8_t>(data), false);
		if constexpr (Archive::IS_LOADER) {
			ymfm::ymfm_saved_state restored(data, false);
			engine.save_restore(restored);
		}
	}
	uint8_t readStatus();
	uint8_t readStatusHi();
	uint8_t readData();
	uint8_t readDataHi();
	[[nodiscard]] uint8_t statusHi() const;
	void writeAddress(uint8_t data);
	void writeAddressHi(uint8_t data);
	void writeData(uint8_t data);
	void writeDataHi(uint8_t data);
	void updatePrescale(uint8_t prescale);
	template <bool Combined> void generateFMImpl(std::span<float*> buffers, unsigned num);
	template <bool Combined> void generateSSGImpl(std::span<float*> buffers, unsigned num);
	uint16_t address;
	uint8_t irqEnable;
	uint8_t flagControl;
	fm_engine fm;
	ymfm::ssg_engine ssg;
	ymfm::adpcm_a_engine adpcmA;
	ymfm::adpcm_b_engine adpcmB;
};
} // namespace openmsx
#endif
