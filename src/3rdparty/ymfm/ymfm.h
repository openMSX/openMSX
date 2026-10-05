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

#ifndef YMFM_H
#define YMFM_H

#pragma once

#include <cassert>
#include <cstdint>
#include <array>

namespace ymfm
{

//*********************************************************
//  GLOBAL HELPERS
//*********************************************************

//-------------------------------------------------
//  bitfield - extract a bitfield from the given
//  value, starting at bit 'start' for a length of
//  'length' bits
//-------------------------------------------------

constexpr uint32_t bitfield(uint32_t value, int start, int length = 1)
{
	return (value >> start) & ((1 << length) - 1);
}

// Envelope counter shared by the FM engine and the ADPCM-A clock grid. OPNA
// divides the envelope by 3, which this counter models by skipping every value
// whose low two bits are 3, so those bits cycle 0, 1, 2.
constexpr uint32_t step_eg_counter(uint32_t counter)
{
	++counter;
	if ((counter & 3) == 3)
		++counter;
	return counter;
}

// Advance the same counter by steps samples in closed form. The low two bits
// never hold 3, so a zero step count adds nothing.
constexpr uint32_t advance_eg_counter(uint32_t counter, uint32_t steps)
{
	uint32_t low = counter & 3;
	assert(low < 3);
	return counter + steps + (steps + low) / 3;
}


//*********************************************************
//  HELPER CLASSES
//*********************************************************

// various envelope states; value 0 was the OPLL depress state, so the
// numbering starts at 1 and still indexes opdata_cache::eg_rate directly
enum envelope_state : uint32_t
{
	EG_ATTACK = 1,
	EG_DECAY = 2,
	EG_SUSTAIN = 3,
	EG_RELEASE = 4,
	EG_STATES = 5
};

// external I/O access classes; OPNA reaches memory for its two ADPCM engines
enum access_class : uint32_t
{
	ACCESS_ADPCM_A,
	ACCESS_ADPCM_B
};



//*********************************************************
//  INTERFACE CLASSES
//*********************************************************

// ======================> ymfm_interface

// Host hooks for timers, IRQ, and external ADPCM memory. openMSX advances
// time with updateStream() before any engine call that needs a coherent
// clock, so there is no separate sync step here.
class ymfm_interface
{
public:
	virtual ~ymfm_interface() = default;

	// Arrange to call fm_engine_base::engine_timer_expired() after the given
	// number of clocks; a negative duration cancels that timer.
	virtual void ymfm_set_timer(uint32_t /*tnum*/, int32_t /*duration_in_clocks*/) { }

	// IRQ line changed
	virtual void ymfm_update_irq(bool /*asserted*/) { }

	// ADPCM memory
	virtual uint8_t ymfm_external_read(access_class /*type*/, uint32_t /*address*/) { return 0; }

	// Debugger-only external reads. Hosts may override this to inspect memory
	// without side effects; never fall back to the ordinary read callback.
	virtual uint8_t ymfm_external_peek(access_class /*type*/, uint32_t /*address*/) { return 0xff; }

	virtual void ymfm_external_write(access_class /*type*/, uint32_t /*address*/, uint8_t /*data*/) { }
};

}


#include "serialize_core.hh"

namespace openmsx {
	static constexpr auto envelopeInfo = std::to_array<enum_string<ymfm::envelope_state>>({
		{ "ATTACK",  ymfm::EG_ATTACK },
		{ "DECAY",   ymfm::EG_DECAY },
		{ "SUSTAIN", ymfm::EG_SUSTAIN },
		{ "RELEASE", ymfm::EG_RELEASE },
	});
	SERIALIZE_ENUM(ymfm::envelope_state, envelopeInfo);
}

#endif // YMFM_H
