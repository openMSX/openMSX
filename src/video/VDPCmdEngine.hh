// $Id$

#ifndef __VDPCMDENGINE_HH__
#define __VDPCMDENGINE_HH__

#include "openmsx.hh"
#include "EmuTime.hh"
#include "VDP.hh"
#include "DisplayMode.hh"

class VDPVRAM;

/** VDP command engine by Alex Wulms.
  * Implements command execution unit of V9938/58.
  */
class VDPCmdEngine
{
public:
	enum Reg {
		REG_SXL = 0x00, // VDP R#32: source X low
		REG_SXH = 0x01, // VDP R#32: source X high
		REG_SYL = 0x02, // VDP R#34: source Y low
		REG_SYH = 0x03, // VDP R#35: source Y high
		REG_DXL = 0x04, // VDP R#36: destination X low
		REG_DXH = 0x05, // VDP R#37: destination X high
		REG_DYL = 0x06, // VDP R#38: destination Y low
		REG_DYH = 0x07, // VDP R#39: destination Y high
		REG_NXL = 0x08, // VDP R#40: number X low
		REG_NXH = 0x09, // VDP R#41: number X high
		REG_NYL = 0x0A, // VDP R#42: number Y low
		REG_NYH = 0x0B, // VDP R#43: number Y high
		REG_COL = 0x0C, // VDP R#44: colour
		REG_ARG = 0x0D, // VDP R#45: argument
		REG_CMD = 0x0E, // VDP R#46: command
		NUM_REGS = 15
	};

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
		if (!(status & 0x01)) {
			// no command in progress
			return;
		}
		commands[(cmdReg[REG_CMD] & 0xF0) >> 4]->execute(time);
	}

	/** Gets the command engine status (part of S#2).
	  * Bit 7 (TR) is set when the command engine is ready for
	  * a pixel transfer.
	  * Bit 4 (BD) is set when the boundary colour is detected.
	  * Bit 0 (CE) is set when a command is in progress.
	  */
	inline byte getStatus(const EmuTime &time) {
		byte cmd = (cmdReg[REG_CMD] & 0xF0) >> 4;
		if (commands[cmd]->willStatusChange(time)) {
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
		return cmdReg[REG_COL];
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
	inline void setCmdReg(byte index, byte value, const EmuTime &time) {
		sync(time);
		cmdReg[index] = value;
		if (index == REG_COL) {
			status &= 0x7F;
		} else if (index == REG_CMD) {
			executeCommand(time);
		}
	}

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
	enum Cmd {
		CM_ABRT  = 0x0, CM_1     = 0x1, CM_2     = 0x2, CM_3     = 0x3,
		CM_POINT = 0x4, CM_PSET  = 0x5, CM_SRCH  = 0x6, CM_LINE  = 0x7,
		CM_LMMV  = 0x8, CM_LMMM  = 0x9, CM_LMCM  = 0xA, CM_LMMC  = 0xB,
		CM_HMMV  = 0xC, CM_HMMM  = 0xD, CM_YMMM  = 0xE, CM_HMMC  = 0xF,
	};

	void executeCommand(const EmuTime &time);
	
	/** Report to stdout the VDP command specified in the registers.
	  */
	void reportVdpCommand();


	/** VDP command registers.
	  */
	byte cmdReg[NUM_REGS];

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
		byte &cmdReg(Reg reg) { return engine->cmdReg[reg]; } 
		
		VDPCmdEngine *engine;
		VDPVRAM *vram;
		EmuTimeFreq<VDP::TICKS_PER_SECOND> currentTime;
		int opsCount;
	};


	/** Abort
	  */
	class AbortCmd : public VDPCmd {
	public:
		AbortCmd(VDPCmdEngine *engine, VDPVRAM *vram)
			: VDPCmd(engine, vram) {}
		virtual void start(const EmuTime &time);
		virtual void execute(const EmuTime &time);
	};

	/** Point
	  */
	class PointCmd : public VDPCmd {
	public:
		PointCmd(VDPCmdEngine *engine, VDPVRAM *vram)
			: VDPCmd(engine, vram) {}
		virtual void start(const EmuTime &time);
		virtual void execute(const EmuTime &time);
	};

	/** Pset
	  */
	class PsetCmd : public VDPCmd {
	public:
		PsetCmd(VDPCmdEngine *engine, VDPVRAM *vram)
			: VDPCmd(engine, vram) {}
		virtual void start(const EmuTime &time);
		virtual void execute(const EmuTime &time);
	};

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

};

#endif // __VDPCMDENGINE_HH__
