// $Id$

#ifndef __VDPCMDENGINE_HH__
#define __VDPCMDENGINE_HH__

#include "openmsx.hh"
#include "VDP.hh"
#include "DisplayMode.hh"

namespace openmsx {

class VDPVRAM;

/** VDP command engine by Alex Wulms.
  * Implements command execution unit of V9938/58.
  */
class VDPCmdEngine
{
public:
	/** Constructor.
	  */
	VDPCmdEngine(VDP *vdp);

	/** Destructor
	  */
	~VDPCmdEngine();

	/** Reinitialise Renderer state.
	  * @param time The moment in time the reset occurs.
	  */
	void reset(const EmuTime &time);

	/** Synchronises the command engine with the VDP.
	  * Ideally this would be a private method, but the current
	  * design doesn't allow that.
	  * @param time The moment in emulated time to sync to.
	  */
	inline void sync(const EmuTime &time) {
		if (!CMD) {
			// no command in progress
			return;
		}
		commands[CMD]->execute(time);
	}

	/** Gets the command engine status (part of S#2).
	  * Bit 7 (TR) is set when the command engine is ready for
	  * a pixel transfer.
	  * Bit 4 (BD) is set when the boundary colour is detected.
	  * Bit 0 (CE) is set when a command is in progress.
	  */
	inline byte getStatus(const EmuTime &time) {
		if (commands[CMD]->willStatusChange(time)) {
			sync(time);
		}
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
		return COL;
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
	  * @param value The new value for the specified register.
	  * @param time The moment in emulated time this write occurs.
	  */
	void setCmdReg(byte index, byte value, const EmuTime &time);

	/** Informs the command engine of a VDP display mode change.
	  * @param mode The new display mode.
	  * @param time The moment in emulated time this change occurs.
	  */
	void updateDisplayMode(DisplayMode mode, const EmuTime &time);

private:
	// Types:
	enum LogOp {
		OP_IMP  = 0x0, OP_AND  = 0x1, OP_OR   = 0x2, OP_XOR  = 0x3,
		OP_NOT  = 0x4, OP_5    = 0x5, OP_6    = 0x6, OP_7    = 0x7,
		OP_TIMP = 0x8, OP_TAND = 0x9, OP_TOR  = 0xA, OP_TXOR = 0xB,
		OP_TNOT = 0xC, OP_D    = 0xD, OP_E    = 0xE, OP_F    = 0xF,
	};
	
	void executeCommand(const EmuTime &time);
	
	/** Report to stdout the VDP command specified in the registers.
	  */
	void reportVdpCommand();


	/** VDP command registers.
	  */
	word SX, SY, DX, DY, NX, NY;
	byte COL, ARG, CMD;
	LogOp LOG;
	

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

	/** The VDP this command engine is part of.
	  */
	VDP *vdp;

	/** Current screen mode.
	  * 0 -> SCREEN5, 1 -> SCREEN6, 2 -> SCREEN7, 3 -> SCREEN8,
	  * -1 -> other.
	  */
	int scrMode;

	class VDPCmd;
	VDPCmd* commands[16];


	/** This is an abstract base class the VDP commands
	  */
	class VDPCmd {
	public:
		VDPCmd(VDPCmdEngine *engine_, VDPVRAM *vram_)
			: engine(engine_), vram(vram_) {}

		/** Calculate addr of a pixel in VRAM.
		  */
		inline int vramAddr(int x, int y);

		/** Get a pixel on SCREEN 5 6 7 8
		  */
		inline byte point5(int sx, int sy);
		
		/** Get a pixel on SCREEN 5 6 7 8
		  */
		inline byte point6(int sx, int sy);
		
		/** Get a pixel on SCREEN 5 6 7 8
		  */
		inline byte point7(int sx, int sy);
		
		/** Get a pixel on SCREEN 5 6 7 8
		  */
		inline byte point8(int sx, int sy);

		/** Get a pixel on the screen.
		  */
		inline byte point(int sx, int sy);

		/** Low level method to set a pixel on a screen.
		  */
		inline void psetLowLevel(int addr, byte cl, byte m, LogOp op);

		/** Set a pixel on SCREEN 5
		  */
		inline void pset5(int dx, int dy, byte cl, LogOp op);
		
		/** Set a pixel on SCREEN 6
		  */
		inline void pset6(int dx, int dy, byte cl, LogOp op);
		
		/** Set a pixel on SCREEN 7
		  */
		inline void pset7(int dx, int dy, byte cl, LogOp op);
		
		/** Set a pixel on SCREEN 8
		  */
		inline void pset8(int dx, int dy, byte cl, LogOp op);

		/** Set a pixel on the screen.
		  */
		inline void pset(int dx, int dy, byte cl, LogOp op);

		/** Prepare execution of cmd
		  */
		virtual void start(const EmuTime &time) = 0;

		/** Perform a given V9938 graphical operation.
		  */
		virtual void execute(const EmuTime &time) = 0;

		/** Will this command change the status register before the
		  * specified time? It is allowed to return true if you're not
		  * sure the status register will change.
		  */
		virtual bool willStatusChange(const EmuTime &time) { return true; }

		/** Finshed executing graphical operation.
		  */
		void commandDone();

		/** Get timing value for a certain VDP command.
		  * @param timingValues Pointer to a table containing the timing
		  *   values for the VDP command in question.
		  */
		int getVdpTimingValue(const int *timingValues);

	protected:
		VDPCmdEngine *engine;
		VDPVRAM *vram;
		EmuTimeFreq<VDP::TICKS_PER_SECOND> currentTime;
		int opsCount;
	};
	friend class VDPCmd;

	/** Abort
	  */
	class AbortCmd : public VDPCmd {
	public:
		AbortCmd(VDPCmdEngine *engine, VDPVRAM *vram)
			: VDPCmd(engine, vram) {}
		virtual void start(const EmuTime &time);
		virtual void execute(const EmuTime &time);
	};
	friend class AbortCmd;

	/** Point
	  */
	class PointCmd : public VDPCmd {
	public:
		PointCmd(VDPCmdEngine *engine, VDPVRAM *vram)
			: VDPCmd(engine, vram) {}
		virtual void start(const EmuTime &time);
		virtual void execute(const EmuTime &time);
	};
	friend class PointCmd;

	/** Pset
	  */
	class PsetCmd : public VDPCmd {
	public:
		PsetCmd(VDPCmdEngine *engine, VDPVRAM *vram)
			: VDPCmd(engine, vram) {}
		virtual void start(const EmuTime &time);
		virtual void execute(const EmuTime &time);
	};
	friend class PsetCmd;

	/** Search a dot.
	  */
	class SrchCmd : public VDPCmd {
	public:
		SrchCmd(VDPCmdEngine *engine, VDPVRAM *vram)
			: VDPCmd(engine, vram) {}
		virtual void start(const EmuTime &time);
		virtual void execute(const EmuTime &time);

		int SX, SY;
		int TX, MX;
		int ANX;
		byte CL;
	};
	friend class SrchCmd;

	/** Draw a line.
	  */
	class LineCmd : public VDPCmd {
	public:
		LineCmd(VDPCmdEngine *engine, VDPVRAM *vram)
			: VDPCmd(engine, vram) {}
		virtual void start(const EmuTime &time);
		virtual void execute(const EmuTime &time);

		int DX, DY;
		int NX, NY;
		int TX, TY;
		int ASX, ADX;
		byte CL;
		LogOp LO;
	};
	friend class LineCmd;

	/** Logical move VDP -> VRAM.
	  */
	class LmmvCmd : public VDPCmd {
	public:
		LmmvCmd(VDPCmdEngine *engine, VDPVRAM *vram)
			: VDPCmd(engine, vram) {}
		virtual void start(const EmuTime &time);
		virtual void execute(const EmuTime &time);

		int DX, DY;
		int TX, TY;
		int NX, NY;
		int ADX, ANX;
		byte CL;
		LogOp LO;
	};
	friend class LmmvCmd;

	/** Logical move VRAM -> VRAM.
	  */
	class LmmmCmd : public VDPCmd {
	public:
		LmmmCmd(VDPCmdEngine *engine, VDPVRAM *vram)
			: VDPCmd(engine, vram) {}
		virtual void start(const EmuTime &time);
		virtual void execute(const EmuTime &time);

		int SX, SY;
		int DX, DY;
		int TX, TY;
		int NX, NY;
		int ASX, ADX, ANX;
		LogOp LO;
	};
	friend class LmmmCmd;

	/** Logical move VRAM -> CPU.
	  */
	class LmcmCmd : public VDPCmd {
	public:
		LmcmCmd(VDPCmdEngine *engine, VDPVRAM *vram)
			: VDPCmd(engine, vram) {}
		virtual void start(const EmuTime &time);
		virtual void execute(const EmuTime &time);

		int SX, SY;
		int DY, NX;
		int NY, MX;
		int TX, TY;
		int ASX, ANX;
	};
	friend class LmcmCmd;

	/** Logical move CPU -> VRAM.
	  */
	class LmmcCmd : public VDPCmd {
	public:
		LmmcCmd(VDPCmdEngine *engine, VDPVRAM *vram)
			: VDPCmd(engine, vram) {}
		virtual void start(const EmuTime &time);
		virtual void execute(const EmuTime &time);

		int DX, DY;
		int NX, NY;
		int TX, TY;
		int MX;
		int ADX, ANX;
		LogOp LO;
	};
	friend class LmmcCmd;

	/** High-speed move VDP -> VRAM.
	  */
	class HmmvCmd : public VDPCmd {
	public:
		HmmvCmd(VDPCmdEngine *engine, VDPVRAM *vram)
			: VDPCmd(engine, vram) {}
		virtual void start(const EmuTime &time);
		virtual void execute(const EmuTime &time);

		int DX, DY;
		int NX, NY;
		int TX, TY;
		int ADX, ANX;
		byte CL;
	};
	friend class HmmvCmd;

	/** High-speed move VRAM -> VRAM.
	  */
	class HmmmCmd : public VDPCmd {
	public:
		HmmmCmd(VDPCmdEngine *engine, VDPVRAM *vram)
			: VDPCmd(engine, vram) {}
		virtual void start(const EmuTime &time);
		virtual void execute(const EmuTime &time);

		int SX, SY;
		int DX, DY;
		int TX, TY;
		int NX, NY;
		int ASX, ADX, ANX;
	};
	friend class HmmmCmd;

	/** High-speed move VRAM -> VRAM (Y direction only).
	  */
	class YmmmCmd : public VDPCmd {
	public:
		YmmmCmd(VDPCmdEngine *engine, VDPVRAM *vram)
			: VDPCmd(engine, vram) {}
		virtual void start(const EmuTime &time);
		virtual void execute(const EmuTime &time);

		int SY;
		int NY;
		int DX, DY;
		int TX, TY;
		int ADX;
	};
	friend class YmmmCmd;

	/** High-speed move CPU -> VRAM.
	  */
	class HmmcCmd : public VDPCmd {
	public:
		HmmcCmd(VDPCmdEngine *engine, VDPVRAM *vram)
			: VDPCmd(engine, vram) {}
		virtual void start(const EmuTime &time);
		virtual void execute(const EmuTime &time);

		int DX, DY;
		int NX, NY;
		int TX, TY;
		int MX;
		int ADX, ANX;
	};
	friend class HmmcCmd;

};

} // namespace openmsx

#endif // __VDPCMDENGINE_HH__
