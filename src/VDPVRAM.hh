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
		if (readWindows[RENDER_NAME].isInside(address)
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
		assert(0 <= end && end < size);
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

	class Window {
	public:
		Window();
		inline void setRange(int start, int end) {
			enabled = true;
			this->start = start;
			this->end = end;
		}
		inline void disable() {
			enabled = false;
		}
		/*
		// TODO: If this method is never used, maybe it's faster to
		//       put start at infinity to disable a window.
		inline bool isEnabled() {
			return enabled;
		}
		*/
		inline bool isInside(int address) {
			return enabled && start <= address && address < end;
		}
		inline bool overlaps(int start, int end) {
			return enabled && this->start < end && start < this->end;
		}
		/** TODO: Move to constructor or make available only to VDPVRAM.
		  */
		void setData(byte *data);
	private:
		byte *data;
		bool enabled;
		int start;
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

#endif //__VDPVRAM_HH__

