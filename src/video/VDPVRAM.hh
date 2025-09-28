#ifndef VDPVRAM_HH
#define VDPVRAM_HH

#include "VDP.hh"
#include "VDPCmdEngine.hh"
#include "VRAMObserver.hh"

#include "Ram.hh"
#include "SimpleDebuggable.hh"

#include "Math.hh"

#include <cassert>
#include <cstdint>

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
Maybe it's not necessary to know exactly which windows overlap with cmdWrite,
only to know whether there are any. If not, sync can be skipped.

Is it possible to read multiple bytes at the same time?
In other words, get a pointer to an array instead of reading single bytes.
Yes, but only the first 64 bytes are guaranteed to be correct, because that
is the granularity of the color table.
But since whatever is reading the VRAM knows what it is operating on, it
can decide for itself how many bytes to read.

*/

class DummyVRAMObserver final : public VRAMObserver
{
public:
	void updateVRAM(unsigned /*offset*/, EmuTime /*time*/) override {}
	void updateWindow(bool /*enabled*/, EmuTime /*time*/) override {}
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
	VRAMWindow(VRAMWindow&&) = delete;
	VRAMWindow& operator=(const VRAMWindow&) = delete;
	VRAMWindow& operator=(VRAMWindow&&) = delete;

	/** Gets the mask for this window.
	  * Should only be called if the window is enabled.
	  * TODO: Only used by dirty checking. Maybe a new dirty checking
	  *       approach can obsolete this method?
	  */
	[[nodiscard]] unsigned getMask() const {
		assert(isEnabled());
		return effectiveBaseMask;
	}

	/** Sets the mask and enables this window.
	  * @param newBaseMask The table base register,
	  *     with the unused bits all ones.
	  * @param newIndexMask The table index mask,
	  *     with the unused bits all ones.
	  * @param newSizeMask
	  * @param time The moment in emulated time this change occurs.
	  * TODO: In planar mode, the index bits are rotated one to the right.
	  *       Solution: have the caller pass index mask instead of the
	  *       number of bits.
	  *       For many tables the number of index bits depends on the
	  *       display mode anyway.
	  */
	void setMask(unsigned newBaseMask, unsigned newIndexMask,
	                    unsigned newSizeMask, EmuTime time) {
		origBaseMask = newBaseMask;
		newBaseMask &= newSizeMask;
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

	/** Same as above, but 'sizeMask' doesn't change.
	 * This is a useful shortcut, because 'sizeMask' rarely changes.
	 */
	void setMask(unsigned newBaseMask, unsigned newIndexMask,
	                    EmuTime time) {
		setMask(newBaseMask, newIndexMask, sizeMask, time);
	}

	/** Disable this window: no address will be considered inside.
	  * @param time The moment in emulated time this change occurs.
	  */
	void disable(EmuTime time) {
		observer->updateWindow(false, time);
		baseAddr = unsigned(-1);
	}

	/** Is the given index range continuous in VRAM (iow there's no mirroring)
	  * Only if the range is continuous it's allowed to call getReadArea().
	  */
	[[nodiscard]] bool isContinuous(unsigned index, unsigned size) const {
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
	[[nodiscard]] bool isContinuous(unsigned mask) const {
		assert(isEnabled());
		assert((mask & ~indexMask)        == mask);
		return (mask & effectiveBaseMask) == mask;
	}

	/** Gets a span of a contiguous part of the VRAM. The region is
	  * [index, index + size) inside the current window.
	  * @param index Index in window
	  */
	template<size_t size>
	[[nodiscard]] std::span<const uint8_t, size> getReadArea(unsigned index) const {
		assert(isContinuous(index, size));
		return std::span<const uint8_t, size>{
				&data[effectiveBaseMask & (indexMask | index)],
				size};
	}

	/** Similar to getReadArea(), but now with planar addressing mode.
	  * This means the region is split in two: one region for the even bytes
	  * (span0) and another for the odd bytes (span1).
	  * @param index Index in window
	  * @return pair{span0, span1}
	  *    span0: The block of even numbered bytes.
	  *    span1: The block of odd  numbered bytes.
	  */
	template<size_t size>
	[[nodiscard]] std::pair<std::span<const uint8_t, size / 2>, std::span<const uint8_t, size / 2>>
			getReadAreaPlanar(unsigned index) const {
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
		const uint8_t* ptr0 = &data[addr | 0x00000];
		const uint8_t* ptr1 = &data[addr | 0x10000];
		return {std::span<const uint8_t, size / 2>{ptr0, size / 2},
		        std::span<const uint8_t, size / 2>{ptr1, size / 2}};
	}

	/** Reads a byte from VRAM in its current state.
	  * @param index Index in table, with unused bits set to 1.
	  */
	[[nodiscard]] uint8_t readNP(unsigned index) const {
		assert(isEnabled());
		return data[effectiveBaseMask & index];
	}

	/** Similar to readNP, but now with planar addressing.
	  * @param index Index in table, with unused bits set to 1.
	  */
	[[nodiscard]] uint8_t readPlanar(unsigned index) const {
		assert(isEnabled());
		index = ((index & 1) << 16) | ((index & 0x1FFFE) >> 1);
		unsigned addr = effectiveBaseMask & index;
		return data[addr];
	}

	/** Is there an observer registered for this window?
	  */
	[[nodiscard]] bool hasObserver() const {
		return observer != &dummyObserver;
	}

	/** Register an observer on this VRAM window.
	  * It will be called when changes occur within the window.
	  * There can be only one observer per window at any given time.
	  * @param newObserver The observer to register.
	  */
	void setObserver(VRAMObserver* newObserver) {
		observer = newObserver;
	}

	/** Unregister the observer of this VRAM window.
	  */
	void resetObserver() {
		observer = &dummyObserver;
	}

	/** Test whether an address is inside this window.
	  * "Inside" is defined as: there is at least one index in this window,
	  * which is mapped to the given address.
	  * TODO: Might be replaced by notify().
	  * @param address The address to test.
	  * @return true iff the address is inside this window.
	  */
	[[nodiscard]] bool isInside(unsigned address) const {
		return (address & combiMask) == baseAddr;
	}

	/** Notifies the observer of this window of a VRAM change,
	  * if the changes address is inside this window.
	  * @param address The address to test.
	  * @param time The moment in emulated time the change occurs.
	  */
	void notify(unsigned address, EmuTime time) {
		if (isInside(address)) {
			observer->updateVRAM(address - baseAddr, time);
		}
	}

	/** Inform VRAMWindow of changed sizeMask.
	  * For the moment this only happens when switching the VR bit in VDP
	  * register 8 (in VR=0 mode only 32kB VRAM is addressable).
	  */
	void setSizeMask(unsigned newSizeMask, EmuTime time) {
		if (isEnabled()) {
			setMask(origBaseMask, indexMask, newSizeMask, time);
		}
		// only apply sizeMask after observers have been notified
		sizeMask = newSizeMask;
	}

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	[[nodiscard]] bool isEnabled() const {
		return baseAddr != unsigned(-1);
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
	uint8_t* data;

	/** Observer associated with this VRAM window.
	  * It will be called when changes occur within the window.
	  * If there is no observer, this variable is &dummyObserver.
	  */
	VRAMObserver* observer = &dummyObserver;

	/** Base mask as passed to the setMask() method.
	 */
	unsigned origBaseMask = 0;

	/** Effective mask of this window.
	  * This is always equal to 'origBaseMask & sizeMask'.
	  */
	unsigned effectiveBaseMask = 0;

	/** Index mask of this window.
	  */
	unsigned indexMask = 0;

	/** Lowest address in this window.
	  * Or -1 when this window is disabled.
	  */
	unsigned baseAddr = unsigned(-1); // disable window

	/** Combination of effectiveBaseMask and index mask used for "inside" checks.
	  */
	unsigned combiMask = 0;

	/** Mask to handle vram mirroring
	  * Note: this only handles mirroring for power-of-2 sizes
	  *       mirroring of extended VRAM is handled in a different way
	  */
	unsigned sizeMask;

	static inline DummyVRAMObserver dummyObserver;
};

/** Manages VRAM contents and synchronizes the various users of the VRAM.
  * VDPVRAM does not apply planar remapping to addresses, this is the
  * responsibility of the caller.
  */
class VDPVRAM
{
public:
	VDPVRAM(const VDPVRAM&) = delete;
	VDPVRAM(VDPVRAM&&) = delete;
	VDPVRAM& operator=(const VDPVRAM&) = delete;
	VDPVRAM& operator=(VDPVRAM&&) = delete;

	VDPVRAM(VDP& vdp, unsigned size, EmuTime time);

	/** Initialize VRAM content to power-up state.
	 */
	void clear();

	/** Update VRAM state to specified moment in time.
	  * @param time Moment in emulated time to update VRAM to.
	  * TODO: Replace this method by VRAMWindow::sync().
	  */
	void sync(EmuTime time) {
		assert(vdp.isInsideFrame(time));
		cmdEngine->sync(time);
	}

	/** Write a byte from the command engine.
	  * Synchronisation with reads by the command engine is skipped.
	  * TODO: Replace by "cmdSync ; VRAMWindow::write".
	  *       Note: "cmdSync", because it checks against read windows, unlike
	  *       the other sync which checks against the cmd write window.
	  */
	void cmdWrite(unsigned address, uint8_t value, EmuTime time) {
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
	void cpuWrite(unsigned address, uint8_t value, EmuTime time) {
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
	[[nodiscard]] uint8_t cpuRead(unsigned address, EmuTime time) {
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
	void updateDisplayMode(DisplayMode mode, bool cmdBit, EmuTime time);

	/** Used by the VDP to signal display enabled changes.
	  * Both the regular border start/end and forced blanking by clearing
	  * the display enable bit are considered display enabled changes.
	  * @param enabled The new display enabled state.
	  * @param time The moment in emulated time this change occurs.
	  */
	void updateDisplayEnabled(bool enabled, EmuTime time);

	/** Used by the VDP to signal sprites enabled changes.
	  * @param enabled The new sprites enabled state.
	  * @param time The moment in emulated time this change occurs.
	  */
	void updateSpritesEnabled(bool enabled, EmuTime time);

	/** Change between VR=0 and VR=1 mode.
	  * @param mode false->VR=0 true->VR=1
	  * @param time The moment in emulated time this change occurs.
	  */
	void updateVRMode(bool mode, EmuTime time);

	void setRenderer(Renderer* renderer, EmuTime time);

	/** Returns the size of VRAM in bytes
	  */
	[[nodiscard]] unsigned getSize() const {
		return actualSize;
	}

	/** Necessary because of circular dependencies.
	  */
	void setSpriteChecker(SpriteChecker* newSpriteChecker) {
		spriteChecker = newSpriteChecker;
	}

	/** Necessary because of circular dependencies.
	  */
	void setCmdEngine(VDPCmdEngine* newCmdEngine) {
		cmdEngine = newCmdEngine;
	}

	/** TMS99x8 VRAM can be mapped in two ways.
	  * See implementation for more details.
	  */
	void change4k8kMapping(bool mapping8k);

	/** Only used by debugger
	 */
	[[nodiscard]] std::span<const uint8_t> getData() const {
		return {data.data(), data.size()};
	}

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	/* Common code of cmdWrite() and cpuWrite()
	 */
	void writeCommon(unsigned address, uint8_t value, EmuTime time) {
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

	void setSizeMask(EmuTime time);

private:
	/** VDP this VRAM belongs to.
	  */
	VDP& vdp;

	/** VRAM data block.
	  */
	Ram data;

	/** Debuggable with mode dependent view on the vram
	  *   Screen7/8 are not interleaved in this mode.
	  *   This debuggable is also at least 128kB in size (it possibly
	  *   contains unmapped regions).
	  */
	class LogicalVRAMDebuggable final : public SimpleDebuggable {
	public:
		explicit LogicalVRAMDebuggable(const VDP& vdp);
		[[nodiscard]] uint8_t read(unsigned address, EmuTime time) override;
		void write(unsigned address, uint8_t value, EmuTime time) override;
	private:
		unsigned transform(unsigned address);
	} logicalVRAMDebug;

	/** Physical view on the VRAM.
	  *   Screen 7/8 are interleaved in this mode. The size of this
	  *   debuggable is the same as the actual VRAM size.
	  */
	struct PhysicalVRAMDebuggable final : SimpleDebuggable {
		PhysicalVRAMDebuggable(const VDP& vdp, unsigned actualSize);
		[[nodiscard]] uint8_t read(unsigned address, EmuTime time) override;
		void write(unsigned address, uint8_t value, EmuTime time) override;
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
	EmuTime vramTime = EmuTime::zero();
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
