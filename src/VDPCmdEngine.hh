// $Id$

#ifndef __VDPCMDENGINE_HH__
#define __VDPCMDENGINE_HH__

#include "openmsx.hh"
#include "EmuTime.hh"
#include "VDP.hh"

class VDPVRAM;

/** VDP command engine by Alex Wulms.
  * Implements command execution unit of V9938/58.
  */
class VDPCmdEngine
{
public:
	/** Constructor.
	  */
	VDPCmdEngine(VDP *vdp, const EmuTime &time);

	/** Synchronises the command engine with the VDP.
	  * Ideally this would be a private method, but the current
	  * design doesn't allow that.
	  * @param time The moment in emulated time to sync to.
	  */
	inline void sync(const EmuTime &time) {
		if (time > currentTime) {
			opsCount = currentTime.getTicksTill(time);
			currentTime = time;
			(this->*currEngine)();
		}
	}

	/** Gets the command engine status (part of S#2).
	  * Bit 7 (TR) is set when the command engine is ready for
	  * a pixel transfer.
	  * Bit 4 (BD) is set when the boundary colour is detected.
	  * Bit 0 (CE) is set when a command is in progress.
	  */
	inline byte getStatus(const EmuTime &time) {
		sync(time);
		return status;
	}

	/** Use this method to transfer pixel(s) from VDP to CPU.
	  * This method implements V9938 S#7.
	  * @param time The moment in emulated time this read occurs.
	  * @return Colour value of the pixel.
	  */
	inline byte readColour(const EmuTime &time) {
		sync(time);
		status &= 0x7F;
		// TODO: Find a way to use REG_COL from here.
		return cmdReg[0x0C /*REG_COL*/];
	}

	/** Gets the X coordinate of a border detected by SRCH.
	  * @param time The moment in emulated time this get occurs.
	  */
	inline int getBorderX(const EmuTime &time) {
		sync(time);
		return borderX;
	}

	/** Writes to a command register.
	  * @param index The register [0..14] to write to.
	  * @param index The new value for the specified register.
	  * @param time The moment in emulated time this write occurs.
	  */
	inline void setCmdReg(byte index, byte value, const EmuTime &time) {
		// TODO: fMSX sets the register after calling write,
		//       with write setting the register as well.
		//       Is there a difference? Which is correct?
		sync(time);
		cmdReg[index] = value;
		if (index == 12) {
			status &= 0x7F;
		} else if (index == 14) {
			executeCommand();
		}
	}

	/** Informs the command engine of a VDP display mode change.
	  * @param mode The new display mode: M5..M1.
	  * @param time The moment in emulated time this change occurs.
	  */
	void updateDisplayMode(int mode, const EmuTime &time);

private:

	// Types:

	typedef void (VDPCmdEngine::*EngineMethod)();

	// Methods:

	/** TODO: Get rid of this.
	  */
	inline byte getVRAM(int address);

	/** Calculate addr of a pixel in VRAM.
	  */
	inline int vramAddr(int x, int y);

	/** Get a pixel on SCREEN5.
	  */
	inline byte point5(int sx, int sy);

	/** Get a pixel on SCREEN6.
	  */
	inline byte point6(int sx, int sy);

	/** Get a pixel on SCREEN7.
	  */
	inline byte point7(int sx, int sy);

	/** Get a pixel on SCREEN8.
	  */
	inline byte point8(int sx, int sy);

	/** Get a pixel on the screen.
	  */
	inline byte point(int sx, int sy);

	/** Low level method to set a pixel on a screen.
	  */
	inline void psetLowLevel(int addr, byte cl, byte m, byte op);

	/** Set a pixel on SCREEN5.
	  */
	inline void pset5(int dx, int dy, byte cl, byte op);

	/** Set a pixel on SCREEN6.
	  */
	inline void pset6(int dx, int dy, byte cl, byte op);

	/** Set a pixel on SCREEN7.
	  */
	inline void pset7(int dx, int dy, byte cl, byte op);

	/** Set a pixel on SCREEN8.
	  */
	inline void pset8(int dx, int dy, byte cl, byte op);

	/** Set a pixel on the screen.
	  */
	inline void pset(int dx, int dy, byte cl, byte op);

	/** Perform a given V9938 graphical operation.
	  */
	void executeCommand();

	/** Finshed executing graphical operation.
	  */
	void commandDone();

	/** Get timing value for a certain VDP command.
	  * @param timingValues Pointer to a table containing the timing
	  *   values for the VDP command in question.
	  */
	int getVdpTimingValue(const int *timingValues);

	// Engine methods which implement the different commands.

	/** Do nothing.
	  */
	void dummyEngine();

	/** Search a dot.
	  */
	void srchEngine();

	/** Draw a line.
	  */
	void lineEngine();

	/** Logical move VDP -> VRAM.
	  */
	void lmmvEngine();

	/** Logical move VRAM -> VRAM.
	  */
	void lmmmEngine();

	/** Logical move VRAM -> CPU.
	  */
	void lmcmEngine();

	/** Logical move CPU -> VRAM.
	  */
	void lmmcEngine();

	/** High-speed move VDP -> VRAM.
	  */
	void hmmvEngine();

	/** High-speed move VRAM -> VRAM.
	  */
	void hmmmEngine();

	/** High-speed move VRAM -> VRAM (Y direction only).
	  */
	void ymmmEngine();

	/** High-speed move CPU -> VRAM.
	  */
	void hmmcEngine();

	/** Report to stdout the VDP command specified in the registers.
	  */
	void reportVdpCommand();

	// Fields:

	struct {
		int SX,SY;
		int DX,DY;
		int TX,TY;
		int NX,NY;
		int MX;
		int ASX,ADX,ANX;
		byte CL;
		byte LO;
		byte CM;
	} MMC;

	/** VDP command registers.
	  */
	byte cmdReg[15];

	/** The command engine status (part of S#2).
	  * Bit 7 (TR) is set when the command engine is ready for
	  * a pixel transfer.
	  * Bit 4 (BD) is set when the boundary colour is detected.
	  * Bit 0 (CE) is set when a command is in progress.
	  */
	byte status;

	/** The X coordinate of a border detected by SRCH.
	  */
	int borderX;

	/** Operation timing.
	  */
	int opsCount;

	/** Pointer to engine method that performs current command.
	  */
	EngineMethod currEngine;

	/** The VDP this command engine is part of.
	  */
	VDP *vdp;

	/** The VRAM this command engine operates on.
	  */
	VDPVRAM *vram;

	/** Current screen mode.
	  * 0 -> SCREEN5, 1 -> SCREEN6, 2 -> SCREEN7, 3 -> SCREEN8,
	  * -1 -> other.
	  */
	int scrMode;

	/** Current time: the moment up until when the engine is emulated.
	  */
	EmuTimeFreq<VDP::TICKS_PER_SECOND> currentTime;

};

#endif // __VDPCMDENGINE_HH__
