#ifndef VDPCMDENGINE_HH
#define VDPCMDENGINE_HH

#include "VDP.hh"
#include "VDPAccessSlots.hh"

#include "BooleanSetting.hh"
#include "Probe.hh"
#include "TclCallback.hh"
#include "openmsx.hh"
#include "serialize_meta.hh"

namespace openmsx {

class VDPVRAM;
class DisplayMode;
class CommandController;


/** VDP command engine by Alex Wulms.
  * Implements command execution unit of V9938/58.
  */
class VDPCmdEngine
{
public:
	// bits in ARG register
	static constexpr byte MXD = 0x20;
	static constexpr byte MXS = 0x10;
	static constexpr byte DIY = 0x08;
	static constexpr byte DIX = 0x04;
	static constexpr byte EQ  = 0x02;
	static constexpr byte MAJ = 0x01;

public:
	VDPCmdEngine(VDP& vdp, CommandController& commandController);

	/** Reinitialize Renderer state.
	  * @param time The moment in time the reset occurs.
	  */
	void reset(EmuTime::param time);

	/** Synchronizes the command engine with the VDP.
	  * Ideally this would be a private method, but the current
	  * design doesn't allow that.
	  * @param time The moment in emulated time to sync to.
	  */
	inline void sync(EmuTime::param time) {
		if (CMD) sync2(time);
	}
	void sync2(EmuTime::param time);

	/** Steal a VRAM access slot from the CmdEngine.
	 * Used when the CPU reads/writes VRAM.
	 * @param time The moment in time the CPU read/write is performed.
	 */
	void stealAccessSlot(EmuTime::param time) {
		if (CMD && engineTime <= time) {
			// take the next available slot
			engineTime = getNextAccessSlot(time, VDPAccessSlots::Delta::D1);
			assert(engineTime > time);
		}
	}

	/** Gets the command engine status (part of S#2).
	  * Bit 7 (TR) is set when the command engine is ready for
	  * a pixel transfer.
	  * Bit 4 (BD) is set when the boundary color is detected.
	  * Bit 0 (CE) is set when a command is in progress.
	  */
	[[nodiscard]] inline byte getStatus(EmuTime::param time) {
		if (time >= statusChangeTime) {
			sync(time);
		}
		return status;
	}

	/** Use this method to transfer pixel(s) from VDP to CPU.
	  * This method implements V9938 S#7.
	  * @param time The moment in emulated time this read occurs.
	  * @return Color value of the pixel.
	  */
	[[nodiscard]] inline byte readColor(EmuTime::param time) {
		sync(time);
		return COL;
	}
	inline void resetColor() {
		// Note: Real VDP always resets TR, but for such a short time
		//       that the MSX won't notice it.
		// TODO: What happens on non-transfer commands?
		if (!CMD) status &= 0x7F;
		transfer = true;
	}

	/** Gets the X coordinate of a border detected by SRCH (intended behaviour,
          * as documented in the V9938 technical data book). However, real VDP
          * simply returns the current value of the ASX 'temporary source X' counter,
          * regardless of the command that is being executed or was executed most
          * recently
	  * @param time The moment in emulated time this get occurs.
	  */
	[[nodiscard]] inline unsigned getBorderX(EmuTime::param time) {
		sync(time);
		return ASX;
	}

	/** Writes to a command register.
	  * @param index The register [0..14] to write to.
	  * @param value The new value for the specified register.
	  * @param time The moment in emulated time this write occurs.
	  */
	void setCmdReg(byte index, byte value, EmuTime::param time);

	/** Read the content of a command register. This method is meant to
	  * be used by the debugger, there is no strict guarantee that the
	  * returned value is the correct value at _exactly_ this moment in
	  * time (IOW this method does not sync the complete CmdEngine)
	  * @param index The register [0..14] to read from.
	  */
	[[nodiscard]] byte peekCmdReg(byte index) const;

	/** Informs the command engine of a VDP display mode change.
	  * @param mode The new display mode.
	  * @param cmdBit Are VDP commands allowed in non-bitmap mode.
	  * @param time The moment in emulated time this change occurs.
	  */
	void updateDisplayMode(DisplayMode mode, bool cmdBit, EmuTime::param time);

	// For debugging only
	bool commandInProgress(EmuTime::param time) {
		sync(time);
		return status & 1;
	}
	/** Get the register-values for the last executed (or still in progress)
	 * command. For debugging purposes only.
	 */
	auto getLastCommand() const {
		return std::tuple{
			lastSX, lastSY, lastDX, lastDY, lastNX, lastNY,
			lastCOL, lastARG, lastCMD};
	}
	/** Get the (source and destination) X/Y coordinates of the currently
	  * executing command. For debugging purposes only.
	  */
	auto getInprogressPosition() const {
		return (status & 1) ? std::tuple{int(ASX), int(SY), int(ADX), int(DY)}
		                    : std::tuple{-1, -1, -1, -1};
	}

	/** Interface for logical operations.
	  */
	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void executeCommand(EmuTime::param time);

	void setStatusChangeTime(EmuTime::param t);
	void calcFinishTime(unsigned NX, unsigned NY, unsigned ticksPerPixel);

	                        void startAbrt(EmuTime::param time);
	                        void startPoint(EmuTime::param time);
	                        void startPset(EmuTime::param time);
	                        void startSrch(EmuTime::param time);
	                        void startLine(EmuTime::param time);
	template<typename Mode> void startLmmv(EmuTime::param time);
	template<typename Mode> void startLmmm(EmuTime::param time);
	template<typename Mode> void startLmcm(EmuTime::param time);
	template<typename Mode> void startLmmc(EmuTime::param time);
	template<typename Mode> void startHmmv(EmuTime::param time);
	template<typename Mode> void startHmmm(EmuTime::param time);
	template<typename Mode> void startYmmm(EmuTime::param time);
	template<typename Mode> void startHmmc(EmuTime::param time);

	template<typename Mode>                 void executePoint(EmuTime::param limit);
	template<typename Mode, typename LogOp> void executePset(EmuTime::param limit);
	template<typename Mode>                 void executeSrch(EmuTime::param limit);
	template<typename Mode, typename LogOp> void executeLine(EmuTime::param limit);
	template<typename Mode, typename LogOp> void executeLmmv(EmuTime::param limit);
	template<typename Mode, typename LogOp> void executeLmmm(EmuTime::param limit);
	template<typename Mode>                 void executeLmcm(EmuTime::param limit);
	template<typename Mode, typename LogOp> void executeLmmc(EmuTime::param limit);
	template<typename Mode>                 void executeHmmv(EmuTime::param limit);
	template<typename Mode>                 void executeHmmm(EmuTime::param limit);
	template<typename Mode>                 void executeYmmm(EmuTime::param limit);
	template<typename Mode>                 void executeHmmc(EmuTime::param limit);

	// Advance to the next access slot at or past the given time.
	inline EmuTime getNextAccessSlot(EmuTime::param time) const {
		return vdp.getAccessSlot(time, VDPAccessSlots::Delta::D0);
	}
	inline void nextAccessSlot(EmuTime::param time) {
		engineTime = getNextAccessSlot(time);
	}
	// Advance to the next access slot that is at least 'delta' cycles past
	// the current one.
	inline EmuTime getNextAccessSlot(EmuTime::param time, VDPAccessSlots::Delta delta) const {
		return vdp.getAccessSlot(time, delta);
	}
	inline void nextAccessSlot(VDPAccessSlots::Delta delta) {
		engineTime = getNextAccessSlot(engineTime, delta);
	}
	inline VDPAccessSlots::Calculator getSlotCalculator(
			EmuTime::param limit) const {
		return vdp.getAccessSlotCalculator(engineTime, limit);
	}

	/** Finished executing graphical operation.
	  */
	void commandDone(EmuTime::param time);

	/** Report the VDP command specified in the registers.
	  */
	void reportVdpCommand() const;

private:
	/** The VDP this command engine is part of.
	  */
	VDP& vdp;
	VDPVRAM& vram;

	/** Only call reportVdpCommand() when this setting is turned on
	  */
	BooleanSetting cmdTraceSetting;
	TclCallback cmdInProgressCallback;

	Probe<bool> executingProbe;

	/** Time at which the next vram access slot is available.
	  * Only valid when a command is executing.
	  */
	EmuTime engineTime{EmuTime::zero()};

	/** Lower bound for the time when the status register will change, IOW
	  * the status register will not change before this time.
	  * Can also be EmuTime::zero -> status can change any moment
	  * or EmuTime::infinity -> this command doesn't change the status
	  */
	EmuTime statusChangeTime{EmuTime::infinity()};

	/** Some commands execute multiple VRAM accesses per pixel
	  * (e.g. LMMM does two reads and a write). This variable keeps
	  * track of where in the (sub)command we are. */
	int phase{0};

	/** Current screen mode.
	  * 0 -> SCREEN5, 1 -> SCREEN6, 2 -> SCREEN7, 3 -> SCREEN8,
	  * 4 -> Non-BitMap mode (like SCREEN8 but non-planar addressing)
	  * -1 -> other.
	  */
	int scrMode{-1};

	/** VDP command registers.
	  */
	unsigned SX{0}, SY{0}, DX{0}, DY{0}, NX{0}, NY{0}; // registers that can be set by CPU
	unsigned ASX{0}, ADX{0}, ANX{0}; // Temporary registers used in the VDP commands
	                                 // Register ASX can be read (via status register 8/9)
	byte COL{0}, ARG{0}, CMD{0};

	/** The last executed command (for debugging only).
	  * A copy of the above registers when the command starts. Remains valid
	  * until the next command starts (also if the corresponding VDP
	  * registers are being changed).
	  */
	int lastSX{0}, lastSY{0}, lastDX{0}, lastDY{0}, lastNX{0}, lastNY{0};
	byte lastCOL{0}, lastARG{0}, lastCMD{0};

	/** When a command needs multiple VRAM accesses per pixel, the result
	 * of intermediate reads is stored in these variables. */
	byte tmpSrc{0};
	byte tmpDst{0};

	/** The command engine status (part of S#2).
	  * Bit 7 (TR) is set when the command engine is ready for
	  * a pixel transfer.
	  * Bit 4 (BD) is set when the boundary color is detected.
	  * Bit 0 (CE) is set when a command is in progress.
	  */
	byte status{0};

	/** Used in LMCM LMMC HMMC cmds, true when CPU has read or written
	  * next byte.
	  */
	bool transfer{false};

	/** Flag that indicated whether extended VRAM is available
	 */
	const bool hasExtendedVRAM;
};
SERIALIZE_CLASS_VERSION(VDPCmdEngine, 4);

} // namespace openmsx

#endif
