// $Id$

#ifndef __VDPVRAM_HH__
#define __VDPVRAM_HH__

#include "VRAMObserver.hh"
#include "openmsx.hh"
#include "Renderer.hh"
#include "VDP.hh"
#include "VDPCmdEngine.hh"
#include <cassert>
#include "Command.hh"
#include "DisplayMode.hh"

namespace openmsx {

class EmuTime;
class SpriteChecker;

/*
Note: The way VRAM is accessed depends a lot on who is doing the accessing.

For example, the ranges:
- Table access is done using masks.
- Command engine work areas are rectangles.
- CPU access always spans full memory.

Maybe define an interface with multiple subclasses?
Or is that too much of a performance hit?
If accessed through the interface, a virtual method call is made.
But invoking the objects directly should be inlined.

Timing:

Each window reflects the state of the VRAM as a specified moment in time.

Because the CPU has full-range write access, it is incorrect for any window
to be ahead in time compared to the CPU. Because multi-cycle operations are
implemented as atomic, it is currently possible that a window which starts
an operation slightly before CPU time ends up slightly after CPU time.
Solutions:
- break up operations in 1-cycle suboperations
  (very hard to reverse engineer accurately)
- do not start an operation until its end time is after CPU time
  (requires minor rewrite of command engine)
- make the code that uses the timestamps resilient to after-CPU times
  (current implementation; investigate if this is correct)

Window ranges are not at fixed. But they can only be changed by the CPU, so
they are fixed until CPU time, which subsystems will never go beyond anyway.

Two only two subsystems with write access are CPU and command engine.
The command engine can only start executing a new command if instructed so
by the CPU. Therefore it is known which area the command engine can write
in until CPU time:
- empty, if the command engine is not executing a command
- the command's reach, if the command engine is executing a command
Currently the command's reach is not computed: full VRAM is used.
Taking the Y coordinate into account would speed things up a lot, because
usually commands execute on invisible pages, so the number of area overlaps
between renderer and command engine would be reduced significantly.
Also sprite tables are usually not written by commands.

Reading through a window is done as follows:
A subsystem reads the VRAM because it is updating itself to a certain moment
in time T.
1. the subsystems syncs the window to T
2. VDPVRAM checks overlap of the window with the command write area
   no overlap -> go to step 6
3. VDPVRAM syncs the command engine to T
4. the command engine calls VDPVRAM to write each byte it changes in VRAM,
   call the times this happens C1, C2, C3...
5. at the n-th write, VDPVRAM updates any subsystem with the written address
   in its window to Cn, this can include the original subsystem
6. the window has reached T
   now the subsystem can update itself to T
Using this approach instead of syncing on read makes sure there is no
re-entrance on the subsystem update methods.

Note: command engine reads through write window when doing logic-ops.

Interesting observation:
Each window is at the same moment in time as the command engine (C):
- if a window doesn't overlap with the command write window, it is stable
  from a moment before C until the CPU time T
- if a window overlaps with the command window, it cannot be before C
  (incorrect) or after C (uncertainty)
Since there is only one time for the entire VRAM, the VRAM itself can be said
to be at C. This is a justification for having the sync method in VDPVRAM
instead of in Window.

Writing through a window is done as follows:
- CPU write: sync with all non-CPU windows, including command engine write
- command engine write: sync with non-CPU and non-command engine windows
Syncing with a window is only necessary if the write falls into that window.

If all non-CPU windows are disjunct, then all subsystems function
independently (at least until CPU time), no need for syncs.
So what is interesting, is which windows overlap.
Since windows change position infrequently, it may be beneficial to
precalculate overlaps.
Not necessarily though, because even if two windows overlap, a single write
may not be inside the other window. So precalculated overlaps only speeds up
in the case there is no overlap.
Maybe it's not necessary to know exactly which windows overlap with cmdwrite,
only to know whether there are any. If not, sync can be skipped.

Is it possible to read multiple bytes at the same time?
In other words, get a pointer to an array instead of reading single bytes.
Yes, but only the first 64 bytes are guaranteed to be correct, because that
is the granularity of the colour table.
But since whatever is reading the VRAM knows what it is operating on, it
can decide for itself how many bytes to read.

*/

/** Specifies an address range in the VRAM.
  * A VDP subsystem can use this to put a claim on a certain area.
  * For example, the owner of a read window will be notified before
  * writes to the corresponding area are commited.
  * The address range is specified by a mask and is not necessarily
  * continuous. See "doc/vram-addressing.txt" for details.
  * TODO: Rename to "Table"? That's the term the VDP data book uses.
  *       Maybe have two classes: "Table" for tables, using a mask,
  *       and "Window" for the command engine, using an interval.
  */
class VRAMWindow {
private:
	inline bool isEnabled() {
		return baseAddr != -1;
	}
public:
	/** Gets the mask for this window.
	  * Should only be called if the window is enabled.
	  * TODO: Only used by dirty checking. Maybe a new dirty checking
	  *       approach can obsolete this method?
	  */
	inline int getMask() {
		assert(isEnabled());
		return baseMask;
	}

	/** Sets the mask and enables this window.
	  * @param baseMask The table base register,
	  * 	with the unused bits all ones.
	  * @param indexMask The table index mask,
	  * 	with the unused bits all ones.
	  * @param time The moment in emulated time this change occurs.
	  * TODO: In planar mode, the index bits are rotated one to the right.
	  *       Solution: have the caller pass index mask instead of #bits.
	  *       For many tables the number of index bits depends on the
	  *       display mode anyway.
	  */
	inline void setMask(int baseMask, int indexMask, const EmuTime &time) {
		int newBaseAddr = baseMask & indexMask;
		int newCombiMask = ~baseMask | indexMask;
		if (baseAddr == newBaseAddr && combiMask == newCombiMask
		&& this->baseMask == baseMask) return;
		if (observer) {
			observer->updateWindow(true, time);
		}
		this->baseMask = baseMask;
		baseAddr = newBaseAddr;
		combiMask = newCombiMask;
	}

	/** Disable this window: no address will be considered inside.
	  * @param time The moment in emulated time this change occurs.
	  */
	inline void disable(const EmuTime &time) {
		if (observer) {
			observer->updateWindow(false, time);
		}
		baseAddr = -1;
	}

	/** Gets a pointer to part of the VRAM in its current state.
	  * The byte pointer to corresponds to the index given,
	  * how many subsequent bytes correspond to subsequent indices
	  * depends on the mask. It is the responsibility of the caller
	  * to take this into account and to make sure no reads outside
	  * the VRAM will occur.
	  * @param index Index in table, with unused bits set to 1.
	  * TODO:
	  * Apply indexMask here, instead of at caller?
	  * Seems we have to know an indexMask anyway, for inside checks.
	  * I have forgotten to set the unused index so many times,
	  * it is really a pitfall.
	  * Because the method is inlined, an optimising compiler can
	  * probably avoid performance loss on constant expressions.
	  */
	inline const byte *readArea(int index) {
		// Reads are only allowed if window is enabled.
		assert(isEnabled());
		int addr = baseMask & index;
		assert(0 <= addr && addr < (1 << 17));
		return &data[addr];
	}

	/** Reads a byte from VRAM in its current state.
	  * @param index Index in table, with unused bits set to 1.
	  * TODO: Rename to "read", since all access is nonplanar (NP) now.
	  */
	inline byte readNP(int index) {
		return *readArea(index);
	}

	/** Register an observer on this VRAM window.
	  * It will be called when changes occur within the window.
	  * There can be only one observer per window at any given time.
	  * @param observer The observer to register.
	  */
	inline void setObserver(VRAMObserver *observer) {
		this->observer = observer;
	}

	/** Unregister the observer of this VRAM window.
	  */
	inline void resetObserver() {
		this->observer = NULL;
	}

	/** Test whether an address is inside this window.
	  * "Inside" is defined as: there is at least one index in this window,
	  * which is mapped to the given address.
	  * TODO: Might be replaced by notify().
	  * @param address The address to test.
	  * @return true iff the address is inside this window.
	  */
	inline bool isInside(int address) {
		return (address & combiMask) == baseAddr;
	}

	/** Notifies the observer of this window of a VRAM change,
	  * if the changes address is inside this window.
	  * @param address The address to test.
	  * @param time The moment in emulated time the change occurs.
	  */
	inline void notify(int address, const EmuTime &time) {
		if (observer && isInside(address)) {
			observer->updateVRAM(address - baseAddr, time);
		}
	}

private:
	/** For access to setData.
	  */
	friend class VDPVRAM;

	/** Create a new window.
	  * Initially, the window is disabled; use setRange to enable it.
	  */
	VRAMWindow();

	/** Used by VDPVRAM to pass a pointer to the VRAM data.
	  */
	void setData(byte *data) {
		this->data = data;
	}

	/** Pointer to the entire VRAM data.
	  */
	byte *data;

	/** Mask of this window.
	  */
	int baseMask;

	/** Lowest address in this window.
	  */
	int baseAddr;

	/** Combination of baseMask and index mask used for "inside" checks.
	  */
	int combiMask;

	/** Observer associated with this VRAM window.
	  * It will be called when changes occur within the window.
	  * If there is no observer, this variable contains NULL.
	  */
	VRAMObserver *observer;

};

/** Manages VRAM contents and synchronises the various users of the VRAM.
  * VDPVRAM does not apply planar remapping to addresses, this is the
  * responsibility of the caller.
  */
class VDPVRAM {
public:

	VRAMWindow cmdReadWindow;
	VRAMWindow cmdWriteWindow;
	VRAMWindow nameTable;
	VRAMWindow colourTable;
	VRAMWindow patternTable;
	VRAMWindow bitmapVisibleWindow;
	VRAMWindow bitmapCacheWindow;
	VRAMWindow spriteAttribTable;
	VRAMWindow spritePatternTable;

	VDPVRAM(VDP *vdp, int size);
	~VDPVRAM();

	/** Update VRAM state to specified moment in time.
	  * @param time Moment in emulated time to update VRAM to.
	  * TODO: Replace this method by VRAMWindow::sync().
	  */
	inline void sync(const EmuTime &time) {
		assert(vdp->isInsideFrame(time));
		cmdEngine->sync(time);
	}

	/** Write a byte from the command engine.
	  * Synchronisation with reads by the command engine is skipped.
	  * TODO: Replace by "cmdSync ; VRAMWindow::write".
	  *       Note: "cmdSync", because it checks against read windows, unlike
	  *       the other sync which checks against the cmd write window.
	  */
	inline void cmdWrite(int address, byte value, const EmuTime &time) {
		// Rewriting history is not allowed.
		assert(time >= currentTime);

		assert(vdp->isInsideFrame(time));

		/*
		// TODO: Pass index instead of address?
		if (true || nameTable.isInside(address)
		|| colourTable.isInside(address)
		|| patternTable.isInside(address) ) {
			renderer->updateVRAM(address, value, time);
		}
		*/
		// Subsystem synchronisation should happen before the commit,
		// to be able to draw backlog using old state.
		bitmapVisibleWindow.notify(address, time);
		spriteAttribTable.notify(address, time);
		spritePatternTable.notify(address, time);

		data[address] = value;
		currentTime = time;

		// Cache dirty marking should happen after the commit,
		// otherwise the cache could be re-validated based on old state.
		bitmapCacheWindow.notify(address, time);
		nameTable.notify(address, time);
		colourTable.notify(address, time);
		patternTable.notify(address, time);

		/* TODO:
		There seems to be a significant difference between subsystem sync
		and cache admin. One example is the code above, the other is
		updateWindow, where subsystem sync is interested in windows that
		were enabled before (new state doesn't matter), while cache admin
		is interested in windows that become enabled (old state doesn't
		matter).
		Does this mean it makes sense to have separate VRAMWindow like
		classes for each category?
		Note: In the future, sprites may switch category, or fall in both.
		*/
	}

	/** Write a byte to VRAM through the CPU interface.
	  * @param address The address to write.
	  * @param value The value to write.
	  * @param time The moment in emulated time this write occurs.
	  */
	inline void cpuWrite(int address, byte value, const EmuTime &time) {
		assert(time >= currentTime);
		assert(vdp->isInsideFrame(time));
		if (cmdReadWindow.isInside(address)
		|| cmdWriteWindow.isInside(address)) {
			cmdEngine->sync(time);
		}
		cmdWrite(address, value, time);
	}

	/** Read a byte from VRAM though the CPU interface.
	  * @param address The address to read.
	  * @param time The moment in emulated time this read occurs.
	  * @return The VRAM contents at the specified address.
	  */
	inline byte cpuRead(int address, const EmuTime &time) {
		// VRAM should never get ahead of CPU.
		assert(time >= currentTime);

		assert(vdp->isInsideFrame(time));

		if (cmdWriteWindow.isInside(address)) {
			cmdEngine->sync(time);
		}
		assert(0 <= address && address < size);
		return data[address];
	}

	/** Used by the VDP to signal display mode changes.
	  * VDPVRAM will inform the Renderer, command engine and the sprite
	  * checker of this change.
	  * TODO: Does this belong in VDPVRAM?
	  * @param mode The new display mode.
	  * @param time The moment in emulated time this change occurs.
	  */
	void updateDisplayMode(DisplayMode mode, const EmuTime &time);

	/** Used by the VDP to signal display enabled changes.
	  * Both the regular border start/end and forced blanking by clearing
	  * the display enable bit are considered display enabled changes.
	  * @param enabled The new display enabled state.
	  * @param time The moment in emulated time this change occurs.
	  */
	void updateDisplayEnabled(bool enabled, const EmuTime &time);

	/** Used by the VDP to signal sprites enabled changes.
	  * @param enabled The new sprites enabled state.
	  * @param time The moment in emulated time this change occurs.
	  */
	void updateSpritesEnabled(bool enabled, const EmuTime &time);

	void setRenderer(Renderer *renderer, const EmuTime &time);

	/** Necessary because of circular dependencies.
	  */
	inline void setSpriteChecker(SpriteChecker *spriteChecker) {
		this->spriteChecker = spriteChecker;
	}

	/** Necessary because of circular dependencies.
	  */
	inline void setCmdEngine(VDPCmdEngine *cmdEngine) {
		this->cmdEngine = cmdEngine;
	}

private:

	/** VDP this VRAM belongs to.
	  */
	VDP *vdp;

	/** Pointer to VRAM data block.
	  */
	byte *data;

	/** Size of VRAM in bytes.
	  */
	int size;

	// TODO: Renderer field can be removed, if updateDisplayMode
	//       and updateDisplayEnabled are moved back to VDP.
	//       Is that a good idea?
	Renderer *renderer;

	VDPCmdEngine *cmdEngine;
	SpriteChecker *spriteChecker;

	/** Current time: the moment up until when the VRAM is updated.
	  * TODO: Is this just for debugging or is it functional?
	  *       Maybe it should stay in either case, possibly between IFDEFs.
	  */
	EmuTimeFreq<VDP::TICKS_PER_SECOND> currentTime;

	class DumpVRAMCmd : public Command {
	public:
		DumpVRAMCmd(VDPVRAM *vram_);
		virtual string execute(const vector<string> &tokens)
			throw (CommandException);
		virtual string help(const vector<string> &tokens) const;
	private:
		VDPVRAM *vram;
	} dumpVRAMCmd;
	friend class DumpVRAMCmd;
};

} // namespace openmsx

#endif //__VDPVRAM_HH__

