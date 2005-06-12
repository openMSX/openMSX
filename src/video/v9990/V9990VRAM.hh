// $Id$

#ifndef V9990VRAM_HH
#define V9990VRAM_HH

#include <vector>
#include "openmsx.hh"
#include "Debuggable.hh"
#include "V9990ModeEnum.hh"

namespace openmsx {

class EmuTime;
class V9990;
class V9990VRAMObserver;

typedef std::vector<V9990VRAMObserver*> V9990VRAMObservers;

/** Video RAM for the V9990.
  */
class V9990VRAM : private Debuggable
{
public:
	/** Construct V9990 VRAM.
	  * @param vdp The V9990 vdp this VRAM belongs to
	  * @param time  Moment in time to create the VRAM
	  */
	V9990VRAM(V9990* vdp, const EmuTime& time);

	/** Destruct V9990 VRAM
	  */
	virtual ~V9990VRAM();

	/** VRAM Size
	  */
	static const unsigned VRAM_SIZE = 512 * 1024; // 512kB

	/** Update VRAM state to specified moment in time.
	  * @param time Moment in emulated time to synchronise VRAM to
	  */
	inline void sync(const EmuTime& time) {
		if (&time); // avoid warning
		// not much to do, yet
	}

	byte readVRAM(unsigned address);
	void writeVRAM(unsigned address, byte val);
	byte readVRAMInterleave(unsigned address);
	void writeVRAMInterleave(unsigned address, byte val);
	inline byte readVRAMP1(unsigned address) { return data[address]; }

	/** Obtain a pointer to the data.
	  */
	inline byte* getData() {
		return data;
	}

	// Interface for V9990VRAMObservers
	/** Add an observer to a window of the VRAM
	  * @param observer  The observer
	  */
	void addObserver(V9990VRAMObserver& observer);

	/** Remove an observer
	  * @param observer  Observer to be removed
	  */
	void removeObserver(V9990VRAMObserver& observer);

private:
	// Debuggable:
	virtual unsigned getSize() const;
	virtual const std::string& getDescription() const;
	virtual byte read(unsigned address);
	virtual void write(unsigned address, byte value);

	/** Inform observers of an update
	  */
	void notifyObservers(unsigned address);

	/** V9990 VDP this VRAM belongs to.
	  */
	V9990* vdp;

	/** Pointer V9990 VRAM data.
	  */
	byte* data;

	/** Display mode.
	  */
	V9990DisplayMode displayMode;

	/** Observers
	  */
	V9990VRAMObservers observers;
}; // class V9990VRAM

} // namespace openmsx

#endif
