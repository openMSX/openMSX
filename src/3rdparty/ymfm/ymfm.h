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
#include <algorithm>
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


//-------------------------------------------------
//  clamp - clamp between the minimum and maximum
//  values provided
//-------------------------------------------------

inline int32_t clamp(int32_t value, int32_t minval, int32_t maxval)
{
	if (value < minval)
		return minval;
	if (value > maxval)
		return maxval;
	return value;
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

// ======================> ymfm_engine_callbacks

// this class represents functions in the engine that the ymfm_interface
// needs to be able to call; it is represented here as a separate interface
// that is independent of the actual engine implementation
class ymfm_engine_callbacks
{
public:
	virtual ~ymfm_engine_callbacks() = default;

	// timer callback; called by the interface when a timer fires
	virtual void engine_timer_expired(uint32_t tnum) = 0;

	// check interrupts; called by the interface after synchronization
	virtual void engine_check_interrupts() = 0;

	// mode register write; called by the interface after synchronization
	virtual void engine_mode_write(uint8_t data) = 0;
};


// ======================> ymfm_interface

// this class represents the interface between the fm_engine and the outside
// world; it provides hooks for timers, synchronization, and I/O
class ymfm_interface
{
	// the engine is our friend
	friend class fm_engine_base;

public:
	virtual ~ymfm_interface() = default;

	// the following functions must be implemented by any derived classes; the
	// default implementations are sufficient for some minimal operation, but will
	// likely need to be overridden to integrate with the outside world; they are
	// all prefixed with ymfm_ to reduce the likelihood of namespace collisions

	//
	// timing and synchronizaton
	//

	// the chip implementation calls this when a write happens to the mode
	// register, which could affect timers and interrupts; our responsibility
	// is to ensure the system is up to date before calling the engine's
	// engine_mode_write() method
	virtual void ymfm_sync_mode_write(uint8_t data) { m_engine->engine_mode_write(data); }

	// the chip implementation calls this when the chip's status has changed,
	// which may affect the interrupt state; our responsibility is to ensure
	// the system is up to date before calling the engine's
	// engine_check_interrupts() method
	virtual void ymfm_sync_check_interrupts() { m_engine->engine_check_interrupts(); }

	// the chip implementation calls this when one of the two internal timers
	// has changed state; our responsibility is to arrange to call the engine's
	// engine_timer_expired() method after the provided number of clocks; if
	// duration_in_clocks is negative, we should cancel any outstanding timers
	virtual void ymfm_set_timer(uint32_t /*tnum*/, int32_t /*duration_in_clocks*/) { }

	// the chip implementation calls this to indicate that the chip should be
	// considered in a busy state until the given number of clocks has passed;
	// our responsibility is to compute and remember the ending time based on
	// the chip's clock for later checking
	virtual void ymfm_set_busy_end(uint32_t /*clocks*/) { }

	// the chip implementation calls this to see if the chip is still currently
	// is a busy state, as specified by a previous call to ymfm_set_busy_end();
	// our responsibility is to compare the current time against the previously
	// noted busy end time and return true if we haven't yet passed it
	virtual bool ymfm_is_busy() { return false; }

	//
	// I/O functions
	//

	// the chip implementation calls this when the state of the IRQ signal has
	// changed due to a status change; our responsibility is to respond as
	// needed to the change in IRQ state, signaling any consumers
	virtual void ymfm_update_irq(bool /*asserted*/) { }

	// the chip implementation calls this whenever data is read from outside
	// of the chip; our responsibility is to provide the data requested
	virtual uint8_t ymfm_external_read(access_class /*type*/, uint32_t /*address*/) { return 0; }

	// Debugger-only external reads. Hosts may override this to inspect memory or
	// I/O without side effects; never fall back to the ordinary read callback.
	virtual uint8_t ymfm_external_peek(access_class /*type*/, uint32_t /*address*/) { return 0xff; }

	// the chip implementation calls this whenever data is written outside
	// of the chip; our responsibility is to pass the written data on to any consumers
	virtual void ymfm_external_write(access_class /*type*/, uint32_t /*address*/, uint8_t /*data*/) { }

protected:
	// pointer to engine callbacks -- this is set directly by the engine at
	// construction time
	ymfm_engine_callbacks *m_engine;
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
