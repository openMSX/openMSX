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

// Test reference; never linked into the emulator.
#include "ReferenceYM2608.hh"
#include "ymfm_fm.ipp"
namespace ymfm {
//*********************************************************
//  YM2608
//*********************************************************

//-------------------------------------------------
//  ym2608_reference - constructor
//-------------------------------------------------

ym2608_reference::ym2608_reference(ymfm_interface &intf) :
	m_fidelity(OPN_FIDELITY_MAX),
	m_address(0),
	m_irq_enable(0x1f),
	m_flag_control(0x1c),
	m_fm(intf),
	m_ssg(intf),
	m_ssg_resampler(m_ssg),
	m_adpcm_a(intf, 0),
	m_adpcm_b(intf)
{
	m_last_fm.clear();
	update_prescale(m_fm.clock_prescale());
}


//-------------------------------------------------
//  reset - reset the system
//-------------------------------------------------

void ym2608_reference::reset()
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

void ym2608_reference::save_restore(ymfm_saved_state &state)
{
	state.save_restore(m_address);
	state.save_restore(m_irq_enable);
	state.save_restore(m_flag_control);
	state.save_restore(m_last_fm.data);

	m_fm.save_restore(state);
	m_ssg.save_restore(state);
	m_ssg_resampler.save_restore(state);
	m_adpcm_a.save_restore(state);
	m_adpcm_b.save_restore(state);
	// Rebuild derived sampling configuration from the restored prescaler.
	// The FM repeat count and SSG resampler function are not serialized.
	if (!state.saving())
		update_prescale(m_fm.clock_prescale());
}


//-------------------------------------------------
//  read_status - read the status register
//-------------------------------------------------

uint8_t ym2608_reference::read_status()
{
	uint8_t result = m_fm.status() & (fm_engine::STATUS_TIMERA | fm_engine::STATUS_TIMERB);
	if (m_fm.intf().ymfm_is_busy())
		result |= fm_engine::STATUS_BUSY;
	return result;
}


//-------------------------------------------------
//  read_data - read the data register
//-------------------------------------------------

uint8_t ym2608_reference::read_data()
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

uint8_t ym2608_reference::status_hi() const
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

uint8_t ym2608_reference::read_status_hi()
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

uint8_t ym2608_reference::read_data_hi()
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

uint8_t ym2608_reference::read(uint32_t offset)
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
uint8_t ym2608_reference::peek(uint32_t offset)
{
	switch (offset & 3)
	{
		case 0:
			return read_status(); // already side-effect-free
		case 1:
			if (m_address < 0x10)
				return m_ssg.peek(m_address);
			return (m_address == 0xff) ? 1 : 0;
		case 2:
			return status_hi() | (m_fm.intf().ymfm_is_busy() ? fm_engine::STATUS_BUSY : 0);
		case 3:
			return ((m_address & 0xff) < 0x10) ? m_adpcm_b.peek(m_address & 0x0f) : 0;
	}
	return 0;
}

// openMSX: normal port-write behavior without disturbing a pending CPU write.
void ym2608_reference::write_register(uint16_t regnum, uint8_t data)
{
	uint16_t saved_address = m_address;
	uint32_t port = (regnum & 0x100) ? 2 : 0;
	write(port, uint8_t(regnum));
	write(port + 1, data);
	m_address = saved_address;
}

uint8_t ym2608_reference::peek_register(uint16_t regnum) const
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

void ym2608_reference::write_address(uint8_t data)
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

void ym2608_reference::write_data(uint8_t data)
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

void ym2608_reference::write_address_hi(uint8_t data)
{
	// just set the address
	m_address = 0x100 | data;
}


//-------------------------------------------------
//  write_data_hi - handle a write to the upper
//  data register
//-------------------------------------------------

void ym2608_reference::write_data_hi(uint8_t data)
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

void ym2608_reference::write(uint32_t offset, uint8_t data)
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


//-------------------------------------------------
//  generate - generate one sample of sound
//-------------------------------------------------

void ym2608_reference::generate(output_data *output, uint32_t numsamples)
{
	m_channel_output_changed = false;
	// FM output is just repeated the prescale number of times; note that
	// 0 is a special 1.5 case
	if (m_fm_samples_per_output != 0)
	{
		for (uint32_t samp = 0; samp < numsamples; samp++, output++)
		{
			if ((m_ssg_resampler.sampindex() + samp) % m_fm_samples_per_output == 0)
				clock_fm_and_adpcm();
			output->data[0] = m_last_fm.data[0];
			output->data[1] = m_last_fm.data[1];
		}
	}
	else
	{
		for (uint32_t samp = 0; samp < numsamples; samp++, output++)
		{
			uint32_t step = (m_ssg_resampler.sampindex() + samp) % 3;
			if (step == 0)
				clock_fm_and_adpcm();
			output->data[0] = m_last_fm.data[0];
			output->data[1] = m_last_fm.data[1];
			if (step == 1)
			{
				clock_fm_and_adpcm();
				output->data[0] = (output->data[0] + m_last_fm.data[0]) / 2;
				output->data[1] = (output->data[1] + m_last_fm.data[1]) / 2;
			}
		}
	}

	// resample the SSG as configured
	m_ssg_resampler.resample(output - numsamples, numsamples);
}


//-------------------------------------------------
//  update_prescale - update the prescale value,
//  recomputing derived values
//-------------------------------------------------

void ym2608_reference::update_prescale(uint8_t prescale)
{
	// tell the FM engine
	m_fm.set_clock_prescale(prescale);
	m_ssg.prescale_changed();

	// Fidelity:   ---- minimum ----    ---- medium -----    ---- maximum-----
	//              rate = clock/48      rate = clock/24      rate = clock/8
	// Prescale    FM rate  SSG rate    FM rate  SSG rate    FM rate  SSG rate
	//     6          3:1     2:3          6:1     4:3         18:1     4:1
	//     3        1.5:1     1:3          3:1     2:3          9:1     2:1
	//     2          1:1     1:6          2:1     1:3          6:1     1:1

	// compute the number of FM samples per output sample, and select the
	// resampler function
	if (m_fidelity == OPN_FIDELITY_MIN)
	{
		switch (prescale)
		{
			default:
			case 6:	m_fm_samples_per_output = 3;	m_ssg_resampler.configure(2, 3);	break;
			case 3: m_fm_samples_per_output = 0;	m_ssg_resampler.configure(1, 3);	break;
			case 2: m_fm_samples_per_output = 1;	m_ssg_resampler.configure(1, 6);	break;
		}
	}
	else if (m_fidelity == OPN_FIDELITY_MED)
	{
		switch (prescale)
		{
			default:
			case 6:	m_fm_samples_per_output = 6;	m_ssg_resampler.configure(4, 3);	break;
			case 3: m_fm_samples_per_output = 3;	m_ssg_resampler.configure(2, 3);	break;
			case 2: m_fm_samples_per_output = 2;	m_ssg_resampler.configure(1, 3);	break;
		}
	}
	else
	{
		switch (prescale)
		{
			default:
			case 6:	m_fm_samples_per_output = 18;	m_ssg_resampler.configure(4, 1);	break;
			case 3: m_fm_samples_per_output = 9;	m_ssg_resampler.configure(2, 1);	break;
			case 2: m_fm_samples_per_output = 6;	m_ssg_resampler.configure(1, 1);	break;
		}
	}

	// if overriding the SSG, override the configuration with the nop
	// resampler to at least keep the sample index moving forward
	if (m_ssg.overridden())
		m_ssg_resampler.configure(0, 0);
}


//-------------------------------------------------
//  clock_fm_and_adpcm - clock FM and ADPCM state
//-------------------------------------------------

void ym2608_reference::clock_fm_and_adpcm()
{
	// top bit of the IRQ enable flags controls 3-channel vs 6-channel mode
	uint32_t fmmask = bitfield(m_irq_enable, 7) ? 0x3f : 0x07;

	// clock the system
	uint32_t env_counter = m_fm.clock(fm_engine::ALL_CHANNELS);

	// clock the ADPCM-A engine on every envelope cycle
	// (channels 4 and 5 clock every 2 envelope clocks)
	if (bitfield(env_counter, 0, 2) == 0)
		m_adpcm_a.clock(bitfield(env_counter, 2) ? 0x0f : 0x3f);

	// clock the ADPCM-B engine every cycle
	m_adpcm_b.clock();

	// openMSX: render each voice once when the host requests channel output.
	// Keep the original mixed path available for differential testing.
	if (m_channel_output != nullptr)
	{
		m_channel_output_changed = true;
		m_last_fm.clear();
		for (unsigned channel = 0; channel < 16; channel++)
		{
			fm_engine::output_data voice;
			voice.clear();
			if (channel < 6)
				m_fm.output(voice, 1, 32767, fmmask & (1U << channel));
			else if (channel == 9)
				m_adpcm_b.output(voice, 1);
			else if (channel >= 10)
				m_adpcm_a.output(voice, 1U << (channel - 10));
			for (unsigned side = 0; side < 2; side++)
			{
				m_channel_output[2 * channel + side] = voice.data[side];
				m_last_fm.data[side] += voice.data[side];
			}
		}
	}
	else
	{
		// OPNA is 13-bit with no intermediate clipping.
		m_fm.output(m_last_fm.clear(), 1, 32767, fmmask);
		m_adpcm_a.output(m_last_fm, 0x3f);
		m_adpcm_b.output(m_last_fm, 1);
	}
	// openMSX mixes floating-point voices; defer clipping to the host output.
}


}
