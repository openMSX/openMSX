// $Id$

#ifndef __VDPVRAM_HH__
#define __VDPVRAM_HH__

#include "openmsx.hh"
#include "Renderer.hh"
#include "VDPCmdEngine.hh"
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
		RENDER_NAME,
		RENDER_COLOUR,
		RENDER_PATTERN,
		RENDER_BITMAP,
		SPRITES,
		NUM_READ_WINDOWS
	};

	VDPVRAM(int size);
	~VDPVRAM();

	/** Write a byte from the command engine.
	  * Synchronisation with reads by the command engine is skipped.
	  */
	inline void cmdWrite(int address, byte value, const EmuTime &time) {
		if (true || readWindows[RENDER_NAME].isInside(address)
		|| readWindows[RENDER_COLOUR].isInside(address)
		|| readWindows[RENDER_PATTERN].isInside(address)
		|| readWindows[RENDER_BITMAP].isInside(address)) {
			// TODO: Call separate routines for the different windows.
			//       Otherwise renderer has to do the range checks again.
			renderer->updateVRAM(address, value, time);
		}
		// TODO: Inform sprites checker.
		data[address] = value;
	}

	/** Write a byte through the CPU interface.
	  */
	inline void cpuWrite(int address, byte value, const EmuTime &time) {
		if (readWindows[COMMAND_READ].isInside(address)
		|| writeWindow.isInside(address)) {
			cmdEngine->sync(time);
		}
		cmdWrite(address, value, time);
	}

	/** Command engine reads a byte from VRAM.
	  * Synchronisation with writes by the command engine is skipped.
	  */
	inline byte cmdRead(int address) {
		assert(0 <= address && address < size);
		return data[address];
	}

	/** TODO: Get rid of it, currently makes Renderers compile.
	  */
	inline byte getVRAM(int address) {
		return cmdRead(address);
	}

	/** Read a byte from VRAM.
	  */
	inline byte read(int address, const EmuTime &time) {
		if (writeWindow.isInside(address)) {
			cmdEngine->sync(time);
		}
		return cmdRead(address);
	}

	/** Reads an area of VRAM.
	  * @param start Start address (inclusive) of the area to read.
	  * @param end End address (exclusive) of the area to read.
	  * @param time Moment in emulated time the area's contents should
	  *   be updated to.
	  * @return A pointer to the specified area.
	  * TODO: Return expiry time for the area's contents.
	  */
	inline const byte *readArea(int start, int end, const EmuTime &time) {
		assert(0 <= start && start < size);
		assert(0 <= end && end <= size);
		assert(start <= end);
		if (writeWindow.overlaps(start, end)) {
			cmdEngine->sync(time);
		}
		return data + start;
	}

	inline void setRenderer(Renderer *renderer) {
		this->renderer = renderer;
	}

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

	inline Window &getReadWindow(WindowId id) {
		return readWindows[id];
	}

private:

	/** Pointer to VRAM data block.
	  */
	byte *data;
	/** Size of VRAM in bytes.
	  */
	int size;

	/** Windows through which various VDP subcomponents read the VRAM.
	  */
	Window readWindows[NUM_READ_WINDOWS];
	/** Window through which the command engine writes the VRAM.
	  */
	Window writeWindow;

	Renderer *renderer;
	VDPCmdEngine *cmdEngine;

};

// TODO: Nice abstraction, but virtual is a large overhead.
class VRAMListener {
public:

	/** Informs the listener of a change in VRAM contents.
	  * @param addr The address that will change.
	  * @param data The new value.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateVRAM(int addr, byte data, const EmuTime &time) = 0;

};

#endif //__VDPVRAM_HH__

