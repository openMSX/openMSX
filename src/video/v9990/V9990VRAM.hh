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
	  * @param vdp The V9990 vdp this VRAM belongs to
	  * @param time  Moment in time to create the VRAM
	  */ 
	V9990VRAM(V9990* vdp, const EmuTime& time);

	/** Destruct V9990 VRAM
	  */
	virtual ~V9990VRAM();

	/** Update VRAM state to specified moment in time
	  * @param time Moment in emulated time to synchronise VRAM to
	  */
	inline void sync(const EmuTime& time) {
		// not much to do, yet
	}

	/*inline*/ byte readVRAM(unsigned address);
	/*inline*/ void writeVRAM(unsigned address, byte val);

	/** Obtain a pointer to the data
	  */
	inline byte *getData(void) {
		return data;
	}

private:
	// Debuggable:
	virtual unsigned getSize() const;
	virtual const string& getDescription() const;
	virtual byte read(unsigned address);
	virtual void write(unsigned address, byte value);

	/** V9990 VDP this VRAM belongs to
	  */
	V9990 *vdp;
	
	/** Pointer V9990 VRAM data.
	  */
	byte *data;

	/** Display mode
	  */
	V9990DisplayMode displayMode;

}; // class V9990VRAM

} // namespace openmsx

#endif

