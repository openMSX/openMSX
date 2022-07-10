#ifndef VDPVRAM_HH
#define VDPVRAM_HH

#include "VRAMObserver.hh"
#include "VDP.hh"
#include "VDPCmdEngine.hh"
#include "SimpleDebuggable.hh"
#include "Ram.hh"
#include "Math.hh"
#include "openmsx.hh"
#include <cassert>

namespace openmsx {

class DisplayMode;
class SpriteChecker;
class Renderer;

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

Each window reflects the state of the VRAM at a specified moment in time.

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

The only two subsystems with write access are CPU and command engine.
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
So "source window" and "destination window" would be better names.

Interesting observation:
Each window is at the same moment in time as the command engine (C):
- if a window doesn't overlap with the command destination window, it is
  stable from a moment before C until the CPU time T
- if a window overlaps with the command destination window, it cannot be
  before C (incorrect) or after C (uncertainty)
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
is the granularity of the color table.
But since whatever is reading the VRAM knows what it is operating on, it
can decide for itself how many bytes to read.

*/

class DummyVRAMOBserver final : public VRAMObserver
{
public:
	void updateVRAM(unsigned /*offset*/, EmuTime::param /*time*/) override {}
	void updateWindow(bool /*enabled*/, EmuTime::param /*time*/) override {}
};

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
class VRAMWindow
{
public:
	VRAMWindow(const VRAMWindow&) = delete;
	VRAMWindow& operator=(const VRAMWindow&) = delete;

	/** Gets the mask for this window.
	  * Should only be called if the window is enabled.
	  * TODO: Only used by dirty checking. Maybe a new dirty checking
	  *       approach can obsolete this method?
	  */
	[[nodiscard]] inline int getMask() const {
		assert(isEnabled());
		return effectiveBaseMask;
	}

	/** Sets the mask and enables this window.
	  * @param newBaseMask The table base register,
	  *     with the unused bits all ones.
	  * @param newIndexMask The table index mask,
	  *     with the unused bits all ones.
	  * @param time The moment in emulated time this change occurs.
	  * TODO: In planar mode, the index bits are rotated one to the right.
	  *       Solution: have the caller pass index mask instead of the
	  *       number of bits.
	  *       For many tables the number of index bits depends on the
	  *       display mode anyway.
	  */
	inline void setMask(int newBaseMask, int newIndexMask,
	                    EmuTime::param time) {
		origBaseMask = newBaseMask;
		newBaseMask &= sizeMask;
		if (isEnabled() &&
		    (newBaseMask  == effectiveBaseMask) &&
		    (newIndexMask == indexMask)) {
			return;
		}
		observer->updateWindow(true, time);
		effectiveBaseMask = newBaseMask;
		indexMask         = newIndexMask;
		baseAddr  =  effectiveBaseMask & indexMask; // this enables window
		combiMask = ~effectiveBaseMask | indexMask;
	}

	/** Disable this window: no address will be considered inside.
	  * @param time The moment in emulated time this change occurs.
	  */
	inline void disable(EmuTime::param time) {
		observer->updateWindow(false, time);
		baseAddr = -1;
	}

	/** Is the given index range continuous in VRAM (iow there's no mirroring)
	  * Only if the range is continuous it's allowed to call getReadArea().
	  */
	[[nodiscard]] inline bool isContinuous(unsigned index, unsigned size) const {
		assert(isEnabled());
		unsigned endIndex = index + size - 1;
		unsigned areaBits = Math::floodRight(index ^ endIndex);
		if ((areaBits & effectiveBaseMask) != areaBits) return false;
		if ((areaBits & ~indexMask)        != areaBits) return false;
		return true;
	}

	/** Alternative version to check whether a region is continuous in
	  * VRAM. This tests whether all addresses in the range
	  *    0bCCCCCCCCCCXXXXXXXX (with C constant and X varying)
	  * are continuous. The input must be a value 1-less-than-a-power-of-2
	  * (so a binary value containing zeros on the left ones on the right)
	  * 1-bits in the parameter correspond with 'X' in the pattern above.
	  * Or IOW it tests an aligned-power-of-2-sized region.
	  */
	[[nodiscard]] inline bool isContinuous(unsigned mask) const {
		assert(isEnabled());
		assert((mask & ~indexMask)        == mask);
		return (mask & effectiveBaseMask) == mask;
	}

	/** Gets a pointer to a contiguous part of the VRAM. The region is
	  * [index, index + size) inside the current window.
	  * @param index Index in table
	  * @param size Size of the block. This is only used to assert that
	  *             requested block is not too large.
	  */
	[[nodiscard]] inline const byte* getReadArea(unsigned index, unsigned size) const {
		assert(isContinuous(index, size)); (void)size;
		return &data[effectiveBaseMask & (indexMask | index)];
	}

	/** Similar to getReadArea(), but now with planar addressing mode.
	  * This means the region is split in two: one region for the even bytes
	  * (ptr0) and another for the odd bytes (ptr1).
	  * @param index Index in table
	  * @param size Size of the block. This is only used to assert that
	  *             requested block is not too large.
	  * @return pair{ptr0, ptr1}
	  *    ptr0: Pointer to the block of even numbered bytes.
	  *    ptr1: Pointer to the block of odd  numbered bytes.
	  */
	[[nodiscard]] inline std::pair<const byte*, const byte*> getReadAreaPlanar(
			unsigned index, unsigned size) const {
		assert((index & 1) == 0);
		assert((size & 1) == 0);
		unsigned endIndex = index + size - 1;
		unsigned areaBits = Math::floodRight(index ^ endIndex);
		areaBits = ((areaBits << 16) | (areaBits >> 1)) & 0x1FFFF & sizeMask;
		(void)areaBits;
		assert((areaBits & effectiveBaseMask) == areaBits);
		assert((areaBits & ~indexMask)        == areaBits);
		assert(isEnabled());
		unsigned addr = effectiveBaseMask & (indexMask | (index >> 1));
		const byte* ptr0 = &data[addr | 0x00000];
		const byte* ptr1 = &data[addr | 0x10000];
		return {ptr0, ptr1};
	}

	/** Reads a byte from VRAM in its current state.
	  * @param index Index in table, with unused bits set to 1.
	  */
	[[nodiscard]] inline byte readNP(unsigned index) const {
		assert(isEnabled());
		return data[effectiveBaseMask & index];
	}

	/** Similar to readNP, but now with planar addressing.
	  * @param index Index in table, with unused bits set to 1.
	  */
	[[nodiscard]] inline byte readPlanar(unsigned index) const {
		assert(isEnabled());
		index = ((index & 1) << 16) | ((index & 0x1FFFE) >> 1);
		unsigned addr = effectiveBaseMask & index;
		return data[addr];
	}

	/** Is there an observer registered for this window?
	  */
	[[nodiscard]] inline bool hasObserver() const {
		return observer != &dummyObserver;
	}

	/** Register an observer on this VRAM window.
	  * It will be called when changes occur within the window.
	  * There can be only one observer per window at any given time.
	  * @param newObserver The observer to register.
	  */
	inline void setObserver(VRAMObserver* newObserver) {
		observer = newObserver;
	}

	/** Unregister the observer of this VRAM window.
	  */
	inline void resetObserver() {
		observer = &dummyObserver;
	}

	/** Test whether an address is inside this window.
	  * "Inside" is defined as: there is at least one index in this window,
	  * which is mapped to the given address.
	  * TODO: Might be replaced by notify().
	  * @param address The address to test.
	  * @return true iff the address is inside this window.
	  */
	[[nodiscard]] inline bool isInside(unsigned address) const {
		return (address & combiMask) == unsigned(baseAddr);
	}

	/** Notifies the observer of this window of a VRAM change,
	  * if the changes address is inside this window.
	  * @param address The address to test.
	  * @param time The moment in emulated time the change occurs.
	  */
	inline void notify(unsigned address, EmuTime::param time) {
		if (isInside(address)) {
			observer->updateVRAM(address - baseAddr, time);
		}
	}

	/** Inform VRAMWindow of changed sizeMask.
	  * For the moment this only happens when switching the VR bit in VDP
	  * register 8 (in VR=0 mode only 32kB VRAM is addressable).
	  */
	void setSizeMask(unsigned newSizeMask, EmuTime::param time) {
		sizeMask = newSizeMask;
		if (isEnabled()) {
			setMask(origBaseMask, indexMask, time);
		}
	}

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	[[nodiscard]] inline bool isEnabled() const {
		return baseAddr != -1;
	}

private:
	/** Only VDPVRAM may construct VRAMWindow objects.
	  */
	friend class VDPVRAM;

	/** Create a new window.
	  * Initially, the window is disabled; use setRange to enable it.
	  */
	explicit VRAMWindow(Ram& vram);

	/** Pointer to the entire VRAM data.
	  */
	byte* data;

	/** Observer associated with this VRAM window.
	  * It will be called when changes occur within the window.
	  * If there is no observer, this variable is &dummyObserver.
	  */
	VRAMObserver* observer;

	/** Base mask as passed to the setMask() method.
	 */
	int origBaseMask;

	/** Effective mask of this window.
	  * This is always equal to 'origBaseMask & sizeMask'.
	  */
	int effectiveBaseMask;

	/** Index mask of this window.
	  */
	int indexMask;

	/** Lowest address in this window.
	  * Or -1 when this window is disabled.
	  */
	int baseAddr;

	/** Combination of effectiveBaseMask and index mask used for "inside" checks.
	  */
	int combiMask;

	/** Mask to handle vram mirroring
	  * Note: this only handles mirroring for power-of-2 sizes
	  *       mirroring of extended VRAM is handled in a different way
	  */
	int sizeMask;

	static inline DummyVRAMOBserver dummyObserver;
};

/** Manages VRAM contents and synchronizes the various users of the VRAM.
  * VDPVRAM does not apply planar remapping to addresses, this is the
  * responsibility of the caller.
  */
class VDPVRAM
{
public:
	VDPVRAM(const VDPVRAM&) = delete;
	VDPVRAM& operator=(const VDPVRAM&) = delete;

	VDPVRAM(VDP& vdp, unsigned size, EmuTime::param time);

	/** Initialize VRAM content to power-up state.
	 */
	void clear();

	/** Update VRAM state to specified moment in time.
	  * @param time Moment in emulated time to update VRAM to.
	  * TODO: Replace this method by VRAMWindow::sync().
	  */
	inline void sync(EmuTime::param time) {
		assert(vdp.isInsideFrame(time));
		cmdEngine->sync(time);
	}

	/** Write a byte from the command engine.
	  * Synchronisation with reads by the command engine is skipped.
	  * TODO: Replace by "cmdSync ; VRAMWindow::write".
	  *       Note: "cmdSync", because it checks against read windows, unlike
	  *       the other sync which checks against the cmd write window.
	  */
	inline void cmdWrite(unsigned address, byte value, EmuTime::param time) {
		#ifdef DEBUG
		// Rewriting history is not allowed.
		assert(time >= vramTime);
		#endif
		assert(vdp.isInsideFrame(time));

		// handle mirroring and non-present ram chips
		address &= sizeMask;
		if (address >= actualSize) [[unlikely]] {
			// 192kb vram is mirroring is handled elsewhere
			assert(address < 0x30000);
			// only happens in case of 16kb vram while you write
			// to range [0x4000,0x8000)
			return;
		}

		writeCommon(address, value, time);
	}

	/** Write a byte to VRAM through the CPU interface.
	  * @param address The address to write.
	  * @param value The value to write.
	  * @param time The moment in emulated time this write occurs.
	  */
	inline void cpuWrite(unsigned address, byte value, EmuTime::param time) {
		#ifdef DEBUG
		// Rewriting history is not allowed.
		assert(time >= vramTime);
		#endif
		assert(vdp.isInsideFrame(time));

		// handle mirroring and non-present ram chips
		address &= sizeMask;
		if (address >= actualSize) [[unlikely]] {
			// 192kb vram is mirroring is handled elsewhere
			assert(address < 0x30000);
			// only happens in case of 16kb vram while you write
			// to range [0x4000,0x8000)
			return;
		}

		// We should still sync with cmdEngine, even if the VRAM already
		// contains the value we're about to write (e.g. it's possible
		// syncing with cmdEngine changes that value, and this write
		// restores it again). This fixes bug:
		//   [2844043] Hinotori - Firebird small graphics corruption
		if (cmdReadWindow .isInside(address) ||
		    cmdWriteWindow.isInside(address)) {
			cmdEngine->sync(time);
		}
		writeCommon(address, value, time);

		cmdEngine->stealAccessSlot(time);
	}

	/** Read a byte from VRAM though the CPU interface.
	  * @param address The address to read.
	  * @param time The moment in emulated time this read occurs.
	  * @return The VRAM contents at the specified address.
	  */
	[[nodiscard]] inline byte cpuRead(unsigned address, EmuTime::param time) {
		#ifdef DEBUG
		// VRAM should never get ahead of CPU.
		assert(time >= vramTime);
		#endif
		assert(vdp.isInsideFrame(time));

		address &= sizeMask;
		if (cmdWriteWindow.isInside(address)) {
			cmdEngine->sync(time);
		}
		cmdEngine->stealAccessSlot(time);

		#ifdef DEBUG
		vramTime = time;
		#endif
		return data[address];
	}

	/** Used by the VDP to signal display mode changes.
	  * VDPVRAM will inform the Renderer, command engine and the sprite
	  * checker of this change.
	  * TODO: Does this belong in VDPVRAM?
	  * @param mode The new display mode.
	  * @param cmdBit Are VDP commands allowed in non-bitmap mode.
	  * @param time The moment in emulated time this change occurs.
	  */
	void updateDisplayMode(DisplayMode mode, bool cmdBit, EmuTime::param time);

	/** Used by the VDP to signal display enabled changes.
	  * Both the regular border start/end and forced blanking by clearing
	  * the display enable bit are considered display enabled changes.
	  * @param enabled The new display enabled state.
	  * @param time The moment in emulated time this change occurs.
	  */
	void updateDisplayEnabled(bool enabled, EmuTime::param time);

	/** Used by the VDP to signal sprites enabled changes.
	  * @param enabled The new sprites enabled state.
	  * @param time The moment in emulated time this change occurs.
	  */
	void updateSpritesEnabled(bool enabled, EmuTime::param time);

	/** Change between VR=0 and VR=1 mode.
	  * @param mode false->VR=0 true->VR=1
	  * @param time The moment in emulated time this change occurs.
	  */
	void updateVRMode(bool mode, EmuTime::param time);

	void setRenderer(Renderer* renderer, EmuTime::param time);

	/** Returns the size of VRAM in bytes
	  */
	[[nodiscard]] unsigned getSize() const {
		return actualSize;
	}

	/** Necessary because of circular dependencies.
	  */
	inline void setSpriteChecker(SpriteChecker* newSpriteChecker) {
		spriteChecker = newSpriteChecker;
	}

	/** Necessary because of circular dependencies.
	  */
	inline void setCmdEngine(VDPCmdEngine* newCmdEngine) {
		cmdEngine = newCmdEngine;
	}

	/** TMS99x8 VRAM can be mapped in two ways.
	  * See implementation for more details.
	  */
	void change4k8kMapping(bool mapping8k);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	/* Common code of cmdWrite() and cpuWrite()
	 */
	inline void writeCommon(unsigned address, byte value, EmuTime::param time) {
		#ifdef DEBUG
		assert(time >= vramTime);
		vramTime = time;
		#endif

		// Check that VRAM will actually be changed.
		// A lot of costly syncs can be saved if the same value is written.
		// For example Penguin Adventure always uploads the whole frame,
		// even if it is the same as the previous frame.
		if (data[address] == value) return;

		// Subsystem synchronisation should happen before the commit,
		// to be able to draw backlog using old state.
		bitmapVisibleWindow.notify(address, time);
		spriteAttribTable.notify(address, time);
		spritePatternTable.notify(address, time);

		data[address] = value;

		// Cache dirty marking should happen after the commit,
		// otherwise the cache could be re-validated based on old state.

		// these two seem to be unused
		// bitmapCacheWindow.notify(address, time);
		// nameTable.notify(address, time);
		assert(!bitmapCacheWindow.hasObserver());
		assert(!nameTable.hasObserver());

		// in the past GLRasterizer observed these two, now there are none
		assert(!colorTable.hasObserver());
		assert(!patternTable.hasObserver());

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

	void setSizeMask(EmuTime::param time);

private:
	/** VDP this VRAM belongs to.
	  */
	VDP& vdp;

	/** VRAM data block.
	  */
	Ram data;

	/** Debuggable with mode dependend view on the vram
	  *   Screen7/8 are not interleaved in this mode.
	  *   This debuggable is also at least 128kB in size (it possibly
	  *   contains unmapped regions).
	  */
	class LogicalVRAMDebuggable final : public SimpleDebuggable {
	public:
		explicit LogicalVRAMDebuggable(VDP& vdp);
		[[nodiscard]] byte read(unsigned address, EmuTime::param time) override;
		void write(unsigned address, byte value, EmuTime::param time) override;
	private:
		unsigned transform(unsigned address);
	} logicalVRAMDebug;

	/** Physical view on the VRAM.
	  *   Screen 7/8 are interleaved in this mode. The size of this
	  *   debuggable is the same as the actual VRAM size.
	  */
	struct PhysicalVRAMDebuggable final : SimpleDebuggable {
		PhysicalVRAMDebuggable(VDP& vdp, unsigned actualSize);
		[[nodiscard]] byte read(unsigned address, EmuTime::param time) override;
		void write(unsigned address, byte value, EmuTime::param time) override;
	} physicalVRAMDebug;

	// TODO: Renderer field can be removed, if updateDisplayMode
	//       and updateDisplayEnabled are moved back to VDP.
	//       Is that a good idea?
	Renderer* renderer;

	VDPCmdEngine* cmdEngine;
	SpriteChecker* spriteChecker;

	/** The last time a CmdEngine write or a CPU read/write occurred.
	  * This is only used in a debug build to check if read/writes come
	  * in the correct order.
	  */
	#ifdef DEBUG
	EmuTime vramTime;
	#endif

	/** Mask to handle vram mirroring
	  * Note: this only handles mirroring at power-of-2 sizes
	  *       mirroring of extended VRAM is handled in a different way
	  */
	unsigned sizeMask;

	/** Actual size of VRAM. Normally this is in sync with sizeMask, but
	  * for 16kb VRAM sizeMask is 32kb-1 while actualSize is only 16kb.
	  */
	const unsigned actualSize;

	/** Corresponds to the VR bit (bit 3 in VDP register 8).
	  */
	bool vrMode;

public:
	VRAMWindow cmdReadWindow;
	VRAMWindow cmdWriteWindow;
	VRAMWindow nameTable;
	VRAMWindow colorTable;
	VRAMWindow patternTable;
	VRAMWindow bitmapVisibleWindow;
	VRAMWindow bitmapCacheWindow;
	VRAMWindow spriteAttribTable;
	VRAMWindow spritePatternTable;
};

} // namespace openmsx

#endif
