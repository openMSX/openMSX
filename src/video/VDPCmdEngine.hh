// $Id$

#ifndef __VDPCMDENGINE_HH__
#define __VDPCMDENGINE_HH__

#include "openmsx.hh"
#include "VDP.hh"
#include "DisplayMode.hh"
#include "Settings.hh"

namespace openmsx {

class VDPVRAM;

/** VDP command engine by Alex Wulms.
  * Implements command execution unit of V9938/58.
  */
class VDPCmdEngine : private SettingListener
{
public:
	/** Constructor.
	  */
	VDPCmdEngine(VDP *vdp);

	/** Destructor
	  */
	virtual ~VDPCmdEngine();

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
		if (time >= statusChangeTime) {
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
		//status &= 0x7F; // dont reset TR
		// Note: Real VDP does reset TR, but for such a short time
		//       that the MSX won't notice it.
		transfer = true;
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

	virtual void update(const SettingLeafNode *setting);
	
	void executeCommand(const EmuTime &time);
	
	/** Finshed executing graphical operation.
	  */
	void commandDone(const EmuTime& time);

	/** Get the current command timing, depends on vdp settings (sprites, display).
	  */
	inline int getTiming() {
		return brokenTiming ? 0 : vdp->getAccessTiming(); 
	}

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

	/** Used in LMCM LMMC HMMC cmds, true when CPU has read or written
	  * next byte.
	  */
	bool transfer;

	/** The X coordinate of a border detected by SRCH.
	  */
	int borderX;

	/** The VDP this command engine is part of.
	  */
	VDP *vdp;
	VDPVRAM* vram;

	/** Current screen mode.
	  * 0 -> SCREEN5, 1 -> SCREEN6, 2 -> SCREEN7, 3 -> SCREEN8,
	  * -1 -> other.
	  */
	int scrMode;

	/** Real command timing or instantaneous (broken) timing
	  */
	bool brokenTiming;

	class VDPCmd;
	VDPCmd* commands[16];

	/** Lower bound for the time when the status register will change, IOW
	  * the status register will not change before this time.
	  * Can also be EmuTime::zero -> status can change any moment
	  * or EmuTime::infinity -> this command doesn't change the status
	  */
	EmuTime statusChangeTime;

	/** This is an abstract base class the VDP commands
	  */
	class VDPCmd {
	public:
		VDPCmd(VDPCmdEngine* engine, VDPVRAM* vram);
		virtual ~VDPCmd();

		/** Prepare execution of cmd
		  */
		virtual void start(const EmuTime &time) = 0;

		/** Perform a given V9938 graphical operation.
		  */
		virtual void execute(const EmuTime &time) = 0;

	protected:
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

		/** Clipping methods
		  */
		inline word clipNX_1(word DX, word NX, byte ppbs = 0);
		inline word clipNX_2(word SX, word DX, word NX, byte ppbs = 0);
		inline word clipNY_1(word DY, word NY);
		inline word clipNY_2(word SY, word DY, word NY);

		VDPCmdEngine *engine;
		VDPVRAM *vram;

		/** Time at which the next operation cycle starts.
		  * A cycle consists of reading source VRAM (if applicable),
		  * reading destination VRAM (if applicable),
		  * writing destination VRAM and updating coordinates.
		  * For perfect timing each phase within a cycle should be timed
		  * explicitly, but for now we use an average execution time per
		  * cycle.
		  */
		EmuTimeFreq<VDP::TICKS_PER_SECOND> currentTime;

		word ASX, ADX, ANX;
	};
	friend class VDPCmd;

	/** Abort
	  */
	class AbortCmd : public VDPCmd {
	public:
		AbortCmd(VDPCmdEngine *engine, VDPVRAM *vram);
		virtual void start(const EmuTime &time);
		virtual void execute(const EmuTime &time);
	};
	friend class AbortCmd;

	/** Point
	  */
	class PointCmd : public VDPCmd {
	public:
		PointCmd(VDPCmdEngine *engine, VDPVRAM *vram);
		virtual void start(const EmuTime &time);
		virtual void execute(const EmuTime &time);
	};
	friend class PointCmd;

	/** Pset
	  */
	class PsetCmd : public VDPCmd {
	public:
		PsetCmd(VDPCmdEngine *engine, VDPVRAM *vram);
		virtual void start(const EmuTime &time);
		virtual void execute(const EmuTime &time);
	};
	friend class PsetCmd;

	/** Search a dot.
	  */
	class SrchCmd : public VDPCmd {
	public:
		SrchCmd(VDPCmdEngine *engine, VDPVRAM *vram);
		virtual void start(const EmuTime &time);
		virtual void execute(const EmuTime &time);
	};
	friend class SrchCmd;

	/** Draw a line.
	  */
	class LineCmd : public VDPCmd {
	public:
		LineCmd(VDPCmdEngine *engine, VDPVRAM *vram);
		virtual void start(const EmuTime &time);
		virtual void execute(const EmuTime &time);
	};
	friend class LineCmd;
	
	/** Abstract base class for block commands.
	  */
	class BlockCmd : public VDPCmd {
	public:
		BlockCmd(VDPCmdEngine *engine, VDPVRAM *vram, const int* timing);
	protected:
		void calcFinishTime(word NX, word NY);

		const int* timing;
	};
	friend class BlockCmd;

	/** Logical move VDP -> VRAM.
	  */
	class LmmvCmd : public BlockCmd {
	public:
		LmmvCmd(VDPCmdEngine *engine, VDPVRAM *vram);
		virtual void start(const EmuTime &time);
		virtual void execute(const EmuTime &time);
	};
	friend class LmmvCmd;

	/** Logical move VRAM -> VRAM.
	  */
	class LmmmCmd : public BlockCmd {
	public:
		LmmmCmd(VDPCmdEngine *engine, VDPVRAM *vram);
		virtual void start(const EmuTime &time);
		virtual void execute(const EmuTime &time);
	};
	friend class LmmmCmd;

	/** Logical move VRAM -> CPU.
	  */
	class LmcmCmd : public BlockCmd {
	public:
		LmcmCmd(VDPCmdEngine *engine, VDPVRAM *vram);
		virtual void start(const EmuTime &time);
		virtual void execute(const EmuTime &time);
	};
	friend class LmcmCmd;

	/** Logical move CPU -> VRAM.
	  */
	class LmmcCmd : public BlockCmd {
	public:
		LmmcCmd(VDPCmdEngine *engine, VDPVRAM *vram);
		virtual void start(const EmuTime &time);
		virtual void execute(const EmuTime &time);
	};
	friend class LmmcCmd;

	/** High-speed move VDP -> VRAM.
	  */
	class HmmvCmd : public BlockCmd {
	public:
		HmmvCmd(VDPCmdEngine *engine, VDPVRAM *vram);
		virtual void start(const EmuTime &time);
		virtual void execute(const EmuTime &time);
	};
	friend class HmmvCmd;

	/** High-speed move VRAM -> VRAM.
	  */
	class HmmmCmd : public BlockCmd {
	public:
		HmmmCmd(VDPCmdEngine *engine, VDPVRAM *vram);
		virtual void start(const EmuTime &time);
		virtual void execute(const EmuTime &time);
	};
	friend class HmmmCmd;

	/** High-speed move VRAM -> VRAM (Y direction only).
	  */
	class YmmmCmd : public BlockCmd {
	public:
		YmmmCmd(VDPCmdEngine *engine, VDPVRAM *vram);
		virtual void start(const EmuTime &time);
		virtual void execute(const EmuTime &time);
	};
	friend class YmmmCmd;

	/** High-speed move CPU -> VRAM.
	  */
	class HmmcCmd : public BlockCmd {
	public:
		HmmcCmd(VDPCmdEngine *engine, VDPVRAM *vram);
		virtual void start(const EmuTime &time);
		virtual void execute(const EmuTime &time);
	};
	friend class HmmcCmd;

	
	/** Report the VDP command specified in the registers.
	  */
	void reportVdpCommand();

	/** Only call reportVdpCommand() when this setting is turned on
	  */
	BooleanSetting cmdTraceSetting;
};

} // namespace openmsx

#endif // __VDPCMDENGINE_HH__
