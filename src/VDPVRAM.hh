// $Id$

#ifndef __VDPVRAM_HH__
#define __VDPVRAM_HH__

#include "openmsx.hh"
#include "Renderer.hh"
#include "VDP.hh"
#include "VDPCmdEngine.hh"
#include "SpriteChecker.hh"
#include <cassert>

class EmuTime;

/** Manages VRAM contents and synchronises the various users of the VRAM.
  */
class VDPVRAM {
public:

	/** Identification of read windows.
	  */
	enum WindowId {
		COMMAND_READ,
		COMMAND_WRITE,
		RENDER_NAME,
		RENDER_COLOUR,
		RENDER_PATTERN,
		RENDER_BITMAP,
		SPRITES,
		NUM_WINDOWS
	};

	VDPVRAM(int size);
	~VDPVRAM();

	/** Update VRAM state to specified moment in time.
	  * @param time Moment in emulated time to update VRAM to.
	  * TODO: Replace this method by Window::sync().
	  */
	inline void sync(const EmuTime &time) {
		cmdEngine->sync(time);
	}

	/** Write a byte from the command engine.
	  * Synchronisation with reads by the command engine is skipped.
	  */
	inline void cmdWrite(int address, byte value, const EmuTime &time) {
		// Rewriting history is not allowed.
		//assert(time >= currentTime);

		spriteChecker->updateVRAM(address, value, time);
		// Problem: infinite loop (renderer <-> cmdEngine)
		// Workaround: only dirty check, no render update
		// Solution: break the chain somehow
		if (true || windows[RENDER_NAME].isInside(address)
		|| windows[RENDER_COLOUR].isInside(address)
		|| windows[RENDER_PATTERN].isInside(address)
		|| windows[RENDER_BITMAP].isInside(address)) {
			// TODO: Call separate routines for the different windows.
			//       Otherwise renderer has to do the range checks again.
			renderer->updateVRAM(address, value, time);
		}
		data[address] = value;
		currentTime = time;
	}

	/** Write a byte to VRAM through the CPU interface.
	  * Takes planar addressing into account if necessary.
	  * @param address The address to write.
	  * @param value The value to write.
	  * @param time The moment in emulated time this write occurs.
	  */
	inline void cpuWrite(int address, byte value, const EmuTime &time) {
		if (windows[COMMAND_READ].isInside(address)
		|| windows[COMMAND_WRITE].isInside(address)) {
			cmdEngine->sync(time);
		}
		if (planar) address = ((address << 16) | (address >> 1)) & 0x1FFFF;
		cmdWrite(address, value, time);
	}

	/** Reads a byte from the VRAM in its current state.
	  * Non-planar addressing is used no matter the display mode.
	  * This can speed up VRAM access when the caller knows it the current
	  * display mode is non-planar or when the caller has already performed
	  * the planar address remapping.
	  */
	inline byte readNP(int address) {
		assert(0 <= address && address < size);
		return data[address];
	}

	/** Reads a byte from the VRAM in its current state.
	  * Planar address remapping is performed in planar display modes.
	  */
	inline byte read(int address) {
		return readNP(
			  planar
			? ((address << 16) | (address >> 1)) & 0x1FFFF
			: address
			);
	}

	/** Read a byte from VRAM though the CPU interface.
	  * Takes planar addressing into account if necessary.
	  * @param address The address to read.
	  * @return The VRAM contents at the specified address.
	  */
	inline byte cpuRead(int address, const EmuTime &time) {
		// VRAM should never get ahead of CPU.
		//assert(time >= currentTime);

		if (windows[COMMAND_WRITE].isInside(address)) {
			cmdEngine->sync(time);
		}
		return read(address);
	}

	/** Reads an area of VRAM.
	  * Note: planar addressing is not applied.
	  * @param start Start address (inclusive) of the area to read.
	  * @param end End address (exclusive) of the area to read.
	  * @return A pointer to the specified area.
	  */
	inline const byte *readArea(int start, int end) {
		assert(0 <= start && start < size);
		assert(0 <= end && end <= size);
		assert(start <= end);
		return data + start;
	}

	/** Used by the VDP to signal display mode changes.
	  * VDPVRAM will inform the Renderer, command engine and the sprite
	  * checker of this change.
	  * @param mode The new display mode: M5..M1.
	  * @param time The moment in emulated time this change occurs.
	  */
	inline void updateDisplayMode(int mode, const EmuTime &time) {
		// Synchronise subsystems.
		renderer->updateDisplayMode(mode, time);
		cmdEngine->updateDisplayMode(mode, time);
		spriteChecker->updateDisplayMode(mode, time);

		// Commit change inside VDPVRAM.
		// TODO: Is the display mode check OK? Profile undefined modes.
		planar = (mode & 0x14) == 0x14;
	}

	/** Used by the VDP to signal display enabled changes.
	  * Both the regular border start/end and forced blanking by clearing
	  * the display enable bit are considered display enabled changes.
	  * @param enabled The new display enabled state.
	  * @param time The moment in emulated time this change occurs.
	  */
	inline void updateDisplayEnabled(bool enabled, const EmuTime &time) {
		// Synchronise subsystems.
		renderer->updateDisplayEnabled(enabled, time);
		cmdEngine->sync(time);
		spriteChecker->updateDisplayEnabled(enabled, time);
	}

	/** Used by the VDP to signal sprites enabled changes.
	  * @param enabled The new sprites enabled state.
	  * @param time The moment in emulated time this change occurs.
	  */
	inline void updateSpritesEnabled(bool enabled, const EmuTime &time) {
		// Synchronise subsystems.
		cmdEngine->sync(time);
		spriteChecker->updateSpritesEnabled(enabled, time);
	}

	inline void setRenderer(Renderer *renderer) {
		this->renderer = renderer;
	}

	/** Necessary becaus of circular dependencies.
	  */
	inline void setSpriteChecker(SpriteChecker *spriteChecker) {
		this->spriteChecker = spriteChecker;
	}

	/** Necessary becaus of circular dependencies.
	  */
	inline void setCmdEngine(VDPCmdEngine *cmdEngine) {
		this->cmdEngine = cmdEngine;
	}

	/** Specifies an address range in the VRAM.
	  * A VDP subsystem can use this to put a claim on a certain area.
	  * For example, the owner of a read window will be notified before
	  * writes to the corresponding area are commited.
	  */
	class Window {
	public:
		/** Create a new window.
		  * Initially, the window is disabled; use setRange to enable it.
		  */
		Window();

		/** Change the address range of this window.
		  * @param start New start address (inclusive).
		  * @param end New end address (exclusive).
		  */
		inline void setRange(int start, int end) {
			this->start = start;
			this->end = end;
		}

		/** Disable this window: no address will be considered inside.
		  */
		inline void disable() {
			this->end = 0;
		}

		/** Test whether an address is inside this window.
		  * @param address The address to test.
		  * @return true iff the address in side this window.
		  */
		inline bool isInside(int address) {
			return address < end && start <= address;
		}

		/** Does this window overlap the specified range?
		  * @param start Start address (inclusive) of range.
		  * @param end End address (exclusive) of range.
		  * @return truee iff range and window overlap.
		  * TODO: Use a Window as parameter instead?
		  */
		inline bool overlaps(int start, int end) {
			return start < this->end && this->start < end;
		}

	private:
		/** For access to setData.
		  */
		friend class VDPVRAM;

		/** Used by VDPVRAM to pass a pointer to the VRAM data.
		  */
		void setData(byte *data);

		/** Pointer to the entire VRAM data.
		  */
		byte *data;

		/** Start address (inclusive) of this window.
		  */
		int start;

		/** End address (exclusive) of this window.
		  * Set this to zero to disable the window.
		  */
		int end;
	};

	inline Window &getWindow(WindowId id) {
		return windows[id];
	}

private:

	/** Pointer to VRAM data block.
	  */
	byte *data;

	/** Size of VRAM in bytes.
	  */
	int size;

	bool planar;

	/** Windows through which various VDP subcomponents access the VRAM.
	  */
	Window windows[NUM_WINDOWS];

	Renderer *renderer;
	VDPCmdEngine *cmdEngine;
	SpriteChecker *spriteChecker;

	/** Current time: the moment up until when the VRAM is updated.
	  * TODO: Is this just for debugging or is it functional?
	  *       Maybe it should stay in either case, possibly between IFDEFs.
	  */
	EmuTimeFreq<VDP::TICKS_PER_SECOND> currentTime;

};

#endif //__VDPVRAM_HH__

