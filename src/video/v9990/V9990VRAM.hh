// $Id$

/** V9990 VRAM
  *
  * Video RAM for the V9990
  */

#ifndef __V9990VRAM_HH__
#define __V9990VRAM_HH__

#include "openmsx.hh"
#include "Debuggable.hh"
#include "V9990.hh"

using std::string;

namespace openmsx {

class EmuTime;
class V9990;

class V9990VRAM : private Debuggable
{
public:
	/** Construct V9990 VRAM
	  * @param v9990 The V9990
	  * @param time  Moment in time to create the V9990
	  */ 
	V9990VRAM(V9990* v9990, const EmuTime& time);

	/** Destruct V9990 VRAM
	  */
	virtual ~V9990VRAM();

	/** Update VRAM state to specified moment in time
	  * @param time Moment in emulated time to synchronise VRAM to
	  */
	inline void sync(const EmuTime& time) {
		// not much to do, yet
	}

private:
	// Debuggable:
	virtual unsigned getSize() const;
	virtual const string& getDescription() const;
	virtual byte read(unsigned address);
	virtual void write(unsigned address, byte value);

	/** V9990 this VRAM belongs to
	  */
	V9990 *v9990;
	
	/** Pointer V9990 VRAM data.
	  */
	byte *vram;

	byte readVRAM(unsigned address, EmuTime& time);
	void writeVRAM(unsigned address, byte val, EmuTime& time);
}; // class V9990VRAM

} // namespace openmsx

#endif

