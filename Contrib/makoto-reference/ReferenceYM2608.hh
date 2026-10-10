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

// Preserved pre-extraction control layer for differential tests only.
#pragma once
#include "3rdparty/ymfm/ymfm_opn.h"
namespace ymfm {
// ======================> ym2608_reference

class ym2608_reference
{
	static constexpr uint8_t STATUS_ADPCM_B_EOS = 0x04;
	static constexpr uint8_t STATUS_ADPCM_B_BRDY = 0x08;
	static constexpr uint8_t STATUS_ADPCM_B_ZERO = 0x10;
	static constexpr uint8_t STATUS_ADPCM_B_PLAYING = 0x20;

public:
	using fm_engine = fm_engine_base<opna_registers>;
	static constexpr uint32_t FM_OUTPUTS = fm_engine::OUTPUTS;
	static constexpr uint32_t SSG_OUTPUTS = 1;
	static constexpr uint32_t OUTPUTS = FM_OUTPUTS + SSG_OUTPUTS;
	using output_data = ymfm_output<OUTPUTS>;

	// constructor
	ym2608_reference(ymfm_interface &intf);

	// configuration
	void ssg_override(ssg_override &intf) { m_ssg.override(intf); }
	void set_fidelity(opn_fidelity fidelity) { m_fidelity = fidelity; update_prescale(m_fm.clock_prescale()); }

	// reset
	void reset();

	// save/restore
	void save_restore(ymfm_saved_state &state);

	// pass-through helpers
	uint32_t sample_rate(uint32_t input_clock) const
	{
		switch (m_fidelity)
		{
			case OPN_FIDELITY_MIN:	return input_clock / 48;
			case OPN_FIDELITY_MED:	return input_clock / 24;
			default:
			case OPN_FIDELITY_MAX:	return input_clock / 8;
		}
	}
	uint32_t ssg_effective_clock(uint32_t input_clock) const { uint32_t scale = m_fm.clock_prescale() * 2 / 3; return input_clock / scale; }
	void invalidate_caches() { m_fm.invalidate_caches(); }

	// read access
	uint8_t read_status();
	uint8_t read_data();
	uint8_t read_status_hi();
	uint8_t read_data_hi();
	uint8_t read(uint32_t offset);

	// Debugger access: no audio clocks, IRQ updates or sample read side effects.
	uint8_t peek(uint32_t offset);
	// Effective core registers, not a history of writes (FM pairs are latched).
	uint8_t peek_register(uint16_t regnum) const;

	// write access
	void write_address(uint8_t data);
	void write_data(uint8_t data);
	void write_address_hi(uint8_t data);
	void write_data_hi(uint8_t data);
	void write(uint32_t offset, uint8_t data);

	// openMSX: optional 16-channel output cache, owned/serialized by the host.
	// FM 0-5, SSG 6-8, ADPCM-B 9, rhythm 10-15; stereo pairs.
	void set_channel_output(int32_t *output) { m_channel_output = output; }
	// openMSX: debugger writes use normal register semantics, preserving the
	// program's address latch. Address-only prescaler writes still take effect.
	void write_register(uint16_t regnum, uint8_t data);
	bool channel_output_changed() const { return m_channel_output_changed; }
	const ssg_engine::output_data &ssg_output() const { return m_ssg_resampler.last_output(); }

	// generate one sample of sound
	void generate(output_data *output, uint32_t numsamples = 1);

protected:
	// Status without BUSY or IRQ propagation.
	uint8_t status_hi() const;

	// internal helpers
	void update_prescale(uint8_t prescale);
	void clock_fm_and_adpcm();

	// internal state
	bool m_channel_output_changed = false; // cache notification, not chip state
	int32_t *m_channel_output = nullptr; // optional host output cache (not chip state)
	opn_fidelity m_fidelity;            // configured fidelity
	uint16_t m_address;                 // address register
	uint8_t m_fm_samples_per_output;    // how many samples to repeat
	uint8_t m_irq_enable;               // IRQ enable register
	uint8_t m_flag_control;             // flag control register
	fm_engine::output_data m_last_fm;   // last FM output
	fm_engine m_fm;                     // core FM engine
	ssg_engine m_ssg;                   // SSG engine
	ssg_resampler<output_data, 2, true> m_ssg_resampler; // SSG resampler helper
	adpcm_a_engine m_adpcm_a;           // ADPCM-A engine
	adpcm_b_engine m_adpcm_b;           // ADPCM-B engine
};


}
