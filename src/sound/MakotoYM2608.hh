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
	// Retain the public 1134-byte state format; unused stream fields are padding.
	void save_restore(ymfm::ymfm_saved_state& state);
	void invalidate_caches() { m_fm.invalidate_caches(); }
	[[nodiscard]] unsigned prescale() const { return m_fm.clock_prescale(); }
	[[nodiscard]] unsigned fmRate() const { return (8000000 + 12 * prescale()) / (24 * prescale()); }
	[[nodiscard]] unsigned ssgRate() const { return 8000000 / (prescale() == 6 ? 32 : prescale() == 3 ? 16 : 8); }
	uint8_t read(uint32_t offset);
	[[nodiscard]] uint8_t peek(uint32_t offset, bool busy) const;
	[[nodiscard]] uint8_t peek_register(uint16_t regnum) const;
	void write(uint32_t offset, uint8_t data);
	void write_register(uint16_t regnum, uint8_t data);
	void generateFM(std::span<float*> buffers, unsigned num);
	void generateSSG(std::span<float*> buffers, unsigned num);
private:
	uint8_t read_status();
	uint8_t read_status_hi();
	uint8_t read_data();
	uint8_t read_data_hi();
	[[nodiscard]] uint8_t status_hi() const;
	void write_address(uint8_t data);
	void write_address_hi(uint8_t data);
	void write_data(uint8_t data);
	void write_data_hi(uint8_t data);
	void update_prescale(uint8_t prescale);
	template<bool Combined> void generateFMImpl(std::span<float*> buffers, unsigned num);
	template<bool Combined> void generateSSGImpl(std::span<float*> buffers, unsigned num);
	uint16_t m_address;
	uint8_t m_irq_enable;
	uint8_t m_flag_control;
	fm_engine m_fm;
	ymfm::ssg_engine m_ssg;
	ymfm::adpcm_a_engine m_adpcm_a;
	ymfm::adpcm_b_engine m_adpcm_b;
};
} // namespace openmsx
#endif
