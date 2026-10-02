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

MakotoYM2608::MakotoYM2608(ymfm_interface& intf)
    : address(0), irqEnable(0x1f), flagControl(0x1c), fm(intf), ssg(intf), adpcmA(intf, 0), adpcmB(intf)
{
	updatePrescale(fm.clock_prescale());
}

void MakotoYM2608::reset()
{
	// reset the engines
	fm.reset();
	ssg.reset();
	adpcmA.reset();
	adpcmB.reset();

	// configure ADPCM percussion sounds; these are present in an embedded ROM
	adpcmA.set_start_end(0, 0x0000, 0x01bf); // bass drum
	adpcmA.set_start_end(1, 0x01c0, 0x043f); // snare drum
	adpcmA.set_start_end(2, 0x0440, 0x1b7f); // top cymbal
	adpcmA.set_start_end(3, 0x1b80, 0x1cff); // high hat
	adpcmA.set_start_end(4, 0x1d00, 0x1f7f); // tom tom
	adpcmA.set_start_end(5, 0x1f80, 0x1fff); // rim shot

	// initialize our special interrupt states, then read the upper status
	// register, which updates the IRQs
	irqEnable = 0x1f;
	flagControl = 0x1c;
	readStatusHi();
}

uint8_t MakotoYM2608::readStatus()
{
	uint8_t result = fm.status() & (fm_engine::STATUS_TIMERA | fm_engine::STATUS_TIMERB);
	if (fm.intf().ymfm_is_busy()) {
		result |= fm_engine::STATUS_BUSY;
	}
	return result;
}

uint8_t MakotoYM2608::readData()
{
	if (address < 0x10) {
		return ssg.read(address & 0x0f);
	} else {
		return address == 0xff ? 1 : 0;
	}
}

uint8_t MakotoYM2608::statusHi() const
{
	// fetch regular status
	uint8_t status = fm.status() & ~(STATUS_ADPCM_B_EOS | STATUS_ADPCM_B_BRDY | STATUS_ADPCM_B_PLAYING);

	// fetch ADPCM-B status, and merge in the bits
	uint8_t adpcmStatus = adpcmB.status();
	if ((adpcmStatus & adpcm_b_channel::STATUS_EOS) != 0) {
		status |= STATUS_ADPCM_B_EOS;
	}
	if ((adpcmStatus & adpcm_b_channel::STATUS_BRDY) != 0) {
		status |= STATUS_ADPCM_B_BRDY;
	}
	if ((adpcmStatus & adpcm_b_channel::STATUS_PLAYING) != 0) {
		status |= STATUS_ADPCM_B_PLAYING;
	}

	// turn off any bits that have been requested to be masked
	status &= ~(flagControl & 0x1f);

	return status;
}

uint8_t MakotoYM2608::readStatusHi()
{
	uint8_t status = statusHi();

	// update the status so that IRQs are propagated
	fm.set_reset_status(status, ~status);

	// merge in the busy flag
	if (fm.intf().ymfm_is_busy()) {
		status |= fm_engine::STATUS_BUSY;
	}
	return status;
}

uint8_t MakotoYM2608::readDataHi()
{
	if ((address & 0xff) < 0x10) {
		return adpcmB.read(address & 0x0f);
	} else {
		return 0;
	}
}

uint8_t MakotoYM2608::read(uint32_t offset)
{
	uint8_t result = 0;
	switch (offset & 3) {
	case 0: // status port, YM2203 compatible
		result = readStatus();
		break;

	case 1: // data port (only SSG)
		result = readData();
		break;

	case 2: // status port, extended
		result = readStatusHi();
		break;

	case 3: // ADPCM-B data
		result = readDataHi();
		break;
	}
	return result;
}

// Debugger reads deliberately bypass readStatusHi()'s IRQ update and the
// ADPCM data port's dummy reads, address advancement and flag changes.
uint8_t MakotoYM2608::peek(uint32_t offset, bool busy) const
{
	switch (offset & 3) {
	case 0:
		return (fm.status() & (fm_engine::STATUS_TIMERA | fm_engine::STATUS_TIMERB)) |
		       (busy ? fm_engine::STATUS_BUSY : 0);
	case 1:
		if (address < 0x10) {
			return ssg.peek(address);
		}
		return (address == 0xff) ? 1 : 0;
	case 2:
		return statusHi() | (busy ? fm_engine::STATUS_BUSY : 0);
	case 3:
		return ((address & 0xff) < 0x10) ? adpcmB.peek(address & 0x0f) : 0;
	}
	return 0;
}

// openMSX: normal port-write behavior without disturbing a pending CPU write.
void MakotoYM2608::writeRegister(uint16_t regnum, uint8_t data)
{
	uint16_t savedAddress = address;
	uint32_t port = (regnum & 0x100) ? 2 : 0;
	write(port, uint8_t(regnum));
	write(port + 1, data);
	address = savedAddress;
}

uint8_t MakotoYM2608::peekRegister(uint16_t regnum) const
{
	assert(regnum < 0x200);
	if (regnum < 0x10) {
		return ssg.regs().read(regnum);
	}
	if (regnum < 0x20) {
		return adpcmA.regs().read(regnum & 0x0f);
	}
	if (regnum == 0x29) {
		return irqEnable;
	}
	if (regnum >= 0x100 && regnum < 0x110) {
		return adpcmB.regs().read(regnum & 0x0f);
	}
	if (regnum == 0x110) {
		return flagControl;
	}
	return fm.regs().read(regnum);
}

void MakotoYM2608::writeAddress(uint8_t data)
{
	// just set the address
	address = data;

	// special case: update the prescale
	if (address >= 0x2d && address <= 0x2f) {
		// 2D-2F: prescaler select
		if (address == 0x2d) {
			updatePrescale(6);
		} else if (address == 0x2e && fm.clock_prescale() == 6)
			updatePrescale(3);
		else if (address == 0x2f)
			updatePrescale(2);
	}
}

void MakotoYM2608::writeData(uint8_t data)
{
	// ignore if paired with upper address
	if (bitfield(address, 8)) {
		return;
	}

	if (address < 0x10) {
		// 00-0F: write to SSG
		ssg.write(address & 0x0f, data);
	} else if (address < 0x20) {
		// 10-1F: write to ADPCM-A
		adpcmA.write(address & 0x0f, data);
	} else if (address == 0x29) {
		// 29: special IRQ mask register
		irqEnable = data;
		fm.set_irq_mask(irqEnable & ~flagControl & 0x1f);
	} else {
		// 20-28, 2A-FF: write to FM
		fm.write(address, data);
	}

	// mark busy for a bit
	fm.intf().ymfm_set_busy_end(32 * fm.clock_prescale());
}

void MakotoYM2608::writeAddressHi(uint8_t data)
{
	// just set the address
	address = 0x100 | data;
}

void MakotoYM2608::writeDataHi(uint8_t data)
{
	// ignore if paired with upper address
	if (!bitfield(address, 8)) {
		return;
	}

	if (address < 0x110) {
		// 100-10F: write to ADPCM-B
		adpcmB.write(address & 0x0f, data);
	} else if (address == 0x110) {
		// 110: IRQ flag control
		if (bitfield(data, 7))
			fm.set_reset_status(0, 0xff);
		else {
			flagControl = data;
			fm.set_irq_mask(irqEnable & ~flagControl & 0x1f);
		}
	} else {
		// 111-1FF: write to FM
		fm.write(address, data);
	}

	// mark busy for a bit
	fm.intf().ymfm_set_busy_end(32 * fm.clock_prescale());
}

void MakotoYM2608::write(uint32_t offset, uint8_t data)
{
	switch (offset & 3) {
	case 0: // address port
		writeAddress(data);
		break;

	case 1: // data port
		writeData(data);
		break;

	case 2: // upper address port
		writeAddressHi(data);
		break;

	case 3: // upper data port
		writeDataHi(data);
		break;
	}
}

void MakotoYM2608::updatePrescale(uint8_t prescale)
{
	fm.set_clock_prescale(prescale);
	ssg.prescale_changed();
}

template <bool Combined> void MakotoYM2608::generateFMImpl(std::span<float*> buffers, unsigned num)
{
	uint32_t orOutput = 0;
	const uint32_t fmMask = bitfield(irqEnable, 7) ? 0x3f : 0x07;
	for (unsigned i = 0; i < num; ++i) {
		const auto env = fm.clock(fm_engine::ALL_CHANNELS);
		if (bitfield(env, 0, 2) == 0) {
			adpcmA.clock(bitfield(env, 2) ? 0x0f : 0x3f);
		}
		adpcmB.clock();
		if constexpr (Combined) {
			// No channel tools: render each engine once into a local stereo sum.
			// This is not retained chip state, and is never internally clipped.
			fm_engine::output_data mixed;
			fm.output(mixed.clear(), 1, 32767, fmMask);
			adpcmB.output(mixed, 1);
			adpcmA.output(mixed, 0x3f);
			buffers[0][2 * i] += float(mixed.data[0]);
			buffers[0][2 * i + 1] += float(mixed.data[1]);
			orOutput |= uint32_t(mixed.data[0] | mixed.data[1]);
		} else {
			// Six FM voices, one ADPCM-B voice and six rhythm voices.
			// Write directly to the host buffers; no channel-output cache.
			for (unsigned c = 0; c < 13; ++c) {
				fm_engine::output_data voice;
				voice.clear();
				if (c < 6) {
					fm.output(voice, 1, 32767, fmMask & (1U << c));
				} else if (c == 6) {
					adpcmB.output(voice, 1);
				} else {
					adpcmA.output(voice, 1U << (c - 7));
				}
				buffers[c][2 * i] += float(voice.data[0]);
				buffers[c][2 * i + 1] += float(voice.data[1]);
			}
		}
	}
	if constexpr (Combined) {
		std::ranges::fill(buffers.subspan(1), nullptr);
		if (!orOutput) {
			buffers[0] = nullptr;
		}
	}
}

void MakotoYM2608::generateFM(std::span<float*> buffers, unsigned num)
{
	assert(buffers.size() == 13);
	if (std::ranges::all_of(buffers, [&](auto* b) { return b == buffers[0]; })) {
		generateFMImpl<true>(buffers, num);
	} else {
		generateFMImpl<false>(buffers, num);
	}
}

template <bool Combined> void MakotoYM2608::generateSSGImpl(std::span<float*> buffers, unsigned num)
{
	uint32_t orOutput = 0;
	for (unsigned i = 0; i < num; ++i) {
		ssg_engine::output_data s;
		ssg.clock();
		ssg.output(s);
		if constexpr (Combined) {
			const auto total = s.data[0] + s.data[1] + s.data[2];
			buffers[0][i] += float(total);
			orOutput |= uint32_t(total);
		} else {
			for (unsigned c = 0; c < 3; ++c)
				buffers[c][i] += float(s.data[c]);
		}
	}
	if constexpr (Combined) {
		std::ranges::fill(buffers.subspan(1), nullptr);
		if (!orOutput) {
			buffers[0] = nullptr;
		}
	}
}

void MakotoYM2608::generateSSG(std::span<float*> buffers, unsigned num)
{
	assert(buffers.size() == 3);
	if (std::ranges::all_of(buffers, [&](auto* b) { return b == buffers[0]; })) {
		generateSSGImpl<true>(buffers, num);
	} else {
		generateSSGImpl<false>(buffers, num);
	}
}

} // namespace openmsx
