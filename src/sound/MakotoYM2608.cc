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
#include "MakotoYM2608.hh"
#include <algorithm>
#include <array>
namespace openmsx {
using namespace ymfm;

MakotoYM2608::MakotoYM2608(ymfm_interface &intf) :
	m_address(0),
	m_irq_enable(0x1f),
	m_flag_control(0x1c),
	m_fm(intf),
	m_ssg(intf),
	m_adpcm_a(intf, 0),
	m_adpcm_b(intf)
{
	update_prescale(m_fm.clock_prescale());
}


//-------------------------------------------------
//  reset - reset the system
//-------------------------------------------------

void MakotoYM2608::reset()
{
	// reset the engines
	m_fm.reset();
	m_ssg.reset();
	m_adpcm_a.reset();
	m_adpcm_b.reset();

	// configure ADPCM percussion sounds; these are present in an embedded ROM
	m_adpcm_a.set_start_end(0, 0x0000, 0x01bf); // bass drum
	m_adpcm_a.set_start_end(1, 0x01c0, 0x043f); // snare drum
	m_adpcm_a.set_start_end(2, 0x0440, 0x1b7f); // top cymbal
	m_adpcm_a.set_start_end(3, 0x1b80, 0x1cff); // high hat
	m_adpcm_a.set_start_end(4, 0x1d00, 0x1f7f); // tom tom
	m_adpcm_a.set_start_end(5, 0x1f80, 0x1fff); // rim shot

	// initialize our special interrupt states, then read the upper status
	// register, which updates the IRQs
	m_irq_enable = 0x1f;
	m_flag_control = 0x1c;
	read_status_hi();
}


//-------------------------------------------------
//  save_restore - save or restore the data
//-------------------------------------------------

void MakotoYM2608::save_restore(ymfm_saved_state &state)
{
	state.save_restore(m_address);
	state.save_restore(m_irq_enable);
	state.save_restore(m_flag_control);
	std::array<int32_t, 2> legacyFm{};
	for (auto& value : legacyFm) state.save_restore(value);

	m_fm.save_restore(state);
	m_ssg.save_restore(state);
	uint32_t legacyIndex = 0;
	std::array<int32_t, 3> legacySsg{};
	state.save_restore(legacyIndex);
	for (auto& value : legacySsg) state.save_restore(value);
	m_adpcm_a.save_restore(state);
	m_adpcm_b.save_restore(state);
	// Keep engine prescaler notification consistent after legacy-state restore.
	// No repeated-sample or SSG resampler configuration exists in this layer.
	if (!state.saving())
		update_prescale(m_fm.clock_prescale());
}


//-------------------------------------------------
//  read_status - read the status register
//-------------------------------------------------

uint8_t MakotoYM2608::read_status()
{
	uint8_t result = m_fm.status() & (fm_engine::STATUS_TIMERA | fm_engine::STATUS_TIMERB);
	if (m_fm.intf().ymfm_is_busy())
		result |= fm_engine::STATUS_BUSY;
	return result;
}


//-------------------------------------------------
//  read_data - read the data register
//-------------------------------------------------

uint8_t MakotoYM2608::read_data()
{
	uint8_t result = 0;
	if (m_address < 0x10)
	{
		// 00-0F: Read from SSG
		result = m_ssg.read(m_address & 0x0f);
	}
	else if (m_address == 0xff)
	{
		// FF: ID code
		result = 1;
	}
	return result;
}


//-------------------------------------------------
//  read_status_hi - read the extended status
//  register
//-------------------------------------------------

uint8_t MakotoYM2608::status_hi() const
{
	// fetch regular status
	uint8_t status = m_fm.status() & ~(STATUS_ADPCM_B_EOS | STATUS_ADPCM_B_BRDY | STATUS_ADPCM_B_PLAYING);

	// fetch ADPCM-B status, and merge in the bits
	uint8_t adpcm_status = m_adpcm_b.status();
	if ((adpcm_status & adpcm_b_channel::STATUS_EOS) != 0)
		status |= STATUS_ADPCM_B_EOS;
	if ((adpcm_status & adpcm_b_channel::STATUS_BRDY) != 0)
		status |= STATUS_ADPCM_B_BRDY;
	if ((adpcm_status & adpcm_b_channel::STATUS_PLAYING) != 0)
		status |= STATUS_ADPCM_B_PLAYING;

	// turn off any bits that have been requested to be masked
	status &= ~(m_flag_control & 0x1f);

	return status;
}

uint8_t MakotoYM2608::read_status_hi()
{
	uint8_t status = status_hi();

	// update the status so that IRQs are propagated
	m_fm.set_reset_status(status, ~status);

	// merge in the busy flag
	if (m_fm.intf().ymfm_is_busy())
		status |= fm_engine::STATUS_BUSY;
	return status;
}


//-------------------------------------------------
//  read_data_hi - read the upper data register
//-------------------------------------------------

uint8_t MakotoYM2608::read_data_hi()
{
	uint8_t result = 0;
	if ((m_address & 0xff) < 0x10)
	{
		// 00-0F: Read from ADPCM-B
		result = m_adpcm_b.read(m_address & 0x0f);
	}
	return result;
}


//-------------------------------------------------
//  read - handle a read from the device
//-------------------------------------------------

uint8_t MakotoYM2608::read(uint32_t offset)
{
	uint8_t result = 0;
	switch (offset & 3)
	{
		case 0: // status port, YM2203 compatible
			result = read_status();
			break;

		case 1: // data port (only SSG)
			result = read_data();
			break;

		case 2: // status port, extended
			result = read_status_hi();
			break;

		case 3: // ADPCM-B data
			result = read_data_hi();
			break;
	}
	return result;
}


// Debugger reads deliberately bypass read_status_hi()'s IRQ update and the
// ADPCM data port's dummy reads, address advancement and flag changes.
uint8_t MakotoYM2608::peek(uint32_t offset, bool busy) const
{
	switch (offset & 3)
	{
		case 0:
			return (m_fm.status() & (fm_engine::STATUS_TIMERA | fm_engine::STATUS_TIMERB)) | (busy ? fm_engine::STATUS_BUSY : 0);
		case 1:
			if (m_address < 0x10)
				return m_ssg.peek(m_address);
			return (m_address == 0xff) ? 1 : 0;
		case 2:
			return status_hi() | (busy ? fm_engine::STATUS_BUSY : 0);
		case 3:
			return ((m_address & 0xff) < 0x10) ? m_adpcm_b.peek(m_address & 0x0f) : 0;
	}
	return 0;
}

// openMSX: normal port-write behavior without disturbing a pending CPU write.
void MakotoYM2608::write_register(uint16_t regnum, uint8_t data)
{
	uint16_t saved_address = m_address;
	uint32_t port = (regnum & 0x100) ? 2 : 0;
	write(port, uint8_t(regnum));
	write(port + 1, data);
	m_address = saved_address;
}

uint8_t MakotoYM2608::peek_register(uint16_t regnum) const
{
	assert(regnum < 0x200);
	if (regnum < 0x10)
		return m_ssg.regs().read(regnum);
	if (regnum < 0x20)
		return m_adpcm_a.regs().read(regnum & 0x0f);
	if (regnum == 0x29)
		return m_irq_enable;
	if (regnum >= 0x100 && regnum < 0x110)
		return m_adpcm_b.regs().read(regnum & 0x0f);
	if (regnum == 0x110)
		return m_flag_control;
	return m_fm.regs().read(regnum);
}


//-------------------------------------------------
//  write_address - handle a write to the address
//  register
//-------------------------------------------------

void MakotoYM2608::write_address(uint8_t data)
{
	// just set the address
	m_address = data;

	// special case: update the prescale
	if (m_address >= 0x2d && m_address <= 0x2f)
	{
		// 2D-2F: prescaler select
		if (m_address == 0x2d)
			update_prescale(6);
		else if (m_address == 0x2e && m_fm.clock_prescale() == 6)
			update_prescale(3);
		else if (m_address == 0x2f)
			update_prescale(2);
	}
}


//-------------------------------------------------
//  write - handle a write to the data register
//-------------------------------------------------

void MakotoYM2608::write_data(uint8_t data)
{
	// ignore if paired with upper address
	if (bitfield(m_address, 8))
		return;

	if (m_address < 0x10)
	{
		// 00-0F: write to SSG
		m_ssg.write(m_address & 0x0f, data);
	}
	else if (m_address < 0x20)
	{
		// 10-1F: write to ADPCM-A
		m_adpcm_a.write(m_address & 0x0f, data);
	}
	else if (m_address == 0x29)
	{
		// 29: special IRQ mask register
		m_irq_enable = data;
		m_fm.set_irq_mask(m_irq_enable & ~m_flag_control & 0x1f);
	}
	else
	{
		// 20-28, 2A-FF: write to FM
		m_fm.write(m_address, data);
	}

	// mark busy for a bit
	m_fm.intf().ymfm_set_busy_end(32 * m_fm.clock_prescale());
}


//-------------------------------------------------
//  write_address_hi - handle a write to the upper
//  address register
//-------------------------------------------------

void MakotoYM2608::write_address_hi(uint8_t data)
{
	// just set the address
	m_address = 0x100 | data;
}


//-------------------------------------------------
//  write_data_hi - handle a write to the upper
//  data register
//-------------------------------------------------

void MakotoYM2608::write_data_hi(uint8_t data)
{
	// ignore if paired with upper address
	if (!bitfield(m_address, 8))
		return;

	if (m_address < 0x110)
	{
		// 100-10F: write to ADPCM-B
		m_adpcm_b.write(m_address & 0x0f, data);
	}
	else if (m_address == 0x110)
	{
		// 110: IRQ flag control
		if (bitfield(data, 7))
			m_fm.set_reset_status(0, 0xff);
		else
		{
			m_flag_control = data;
			m_fm.set_irq_mask(m_irq_enable & ~m_flag_control & 0x1f);
		}
	}
	else
	{
		// 111-1FF: write to FM
		m_fm.write(m_address, data);
	}

	// mark busy for a bit
	m_fm.intf().ymfm_set_busy_end(32 * m_fm.clock_prescale());
}


//-------------------------------------------------
//  write - handle a write to the register
//  interface
//-------------------------------------------------

void MakotoYM2608::write(uint32_t offset, uint8_t data)
{
	switch (offset & 3)
	{
		case 0: // address port
			write_address(data);
			break;

		case 1: // data port
			write_data(data);
			break;

		case 2: // upper address port
			write_address_hi(data);
			break;

		case 3: // upper data port
			write_data_hi(data);
			break;
	}
}



void MakotoYM2608::update_prescale(uint8_t prescale)
{
	m_fm.set_clock_prescale(prescale);
	m_ssg.prescale_changed();
}

template<bool Combined>
void MakotoYM2608::generateFMImpl(std::span<float*> buffers, unsigned num)
{
	uint32_t orOutput = 0;
	const uint32_t fmMask = bitfield(m_irq_enable, 7) ? 0x3f : 0x07;
	for (unsigned i = 0; i < num; ++i) {
		const auto env = m_fm.clock(fm_engine::ALL_CHANNELS);
		if (bitfield(env, 0, 2) == 0)
			m_adpcm_a.clock(bitfield(env, 2) ? 0x0f : 0x3f);
		m_adpcm_b.clock();
		if constexpr (Combined) {
			// No channel tools: render each engine once into a local stereo sum.
			// This is not retained chip state, and is never internally clipped.
			fm_engine::output_data mixed;
			m_fm.output(mixed.clear(), 1, 32767, fmMask);
			m_adpcm_b.output(mixed, 1);
			m_adpcm_a.output(mixed, 0x3f);
			buffers[0][2 * i] += float(mixed.data[0]);
			buffers[0][2 * i + 1] += float(mixed.data[1]);
			orOutput |= uint32_t(mixed.data[0] | mixed.data[1]);
		} else {
			// Six FM voices, one ADPCM-B voice and six rhythm voices.
			// Write directly to the host buffers; no channel-output cache.
			for (unsigned c = 0; c < 13; ++c) {
				fm_engine::output_data voice;
				voice.clear();
				if (c < 6) m_fm.output(voice, 1, 32767, fmMask & (1U << c));
				else if (c == 6) m_adpcm_b.output(voice, 1);
				else m_adpcm_a.output(voice, 1U << (c - 7));
				buffers[c][2 * i] += float(voice.data[0]);
				buffers[c][2 * i + 1] += float(voice.data[1]);
			}
		}
	}
	if constexpr (Combined) {
		std::ranges::fill(buffers.subspan(1), nullptr);
		if (!orOutput) buffers[0] = nullptr;
	}
}

void MakotoYM2608::generateFM(std::span<float*> buffers, unsigned num)
{
	assert(buffers.size() == 13);
	if (std::ranges::all_of(buffers, [&](auto* b) { return b == buffers[0]; }))
		generateFMImpl<true>(buffers, num);
	else
		generateFMImpl<false>(buffers, num);
}

template<bool Combined>
void MakotoYM2608::generateSSGImpl(std::span<float*> buffers, unsigned num)
{
	uint32_t orOutput = 0;
	for (unsigned i = 0; i < num; ++i) {
		ssg_engine::output_data s;
		m_ssg.clock();
		m_ssg.output(s);
		if constexpr (Combined) {
			const auto total = s.data[0] + s.data[1] + s.data[2];
			buffers[0][i] += float(total);
			orOutput |= uint32_t(total);
		} else {
			for (unsigned c = 0; c < 3; ++c) buffers[c][i] += float(s.data[c]);
		}
	}
	if constexpr (Combined) {
		std::ranges::fill(buffers.subspan(1), nullptr);
		if (!orOutput) buffers[0] = nullptr;
	}
}

void MakotoYM2608::generateSSG(std::span<float*> buffers, unsigned num)
{
	assert(buffers.size() == 3);
	if (std::ranges::all_of(buffers, [&](auto* b) { return b == buffers[0]; }))
		generateSSGImpl<true>(buffers, num);
	else
		generateSSGImpl<false>(buffers, num);
}

} // namespace openmsx
