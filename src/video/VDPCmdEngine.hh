#ifndef VDPCMDENGINE_HH
#define VDPCMDENGINE_HH

#include "VDP.hh"
#include "VDPAccessSlots.hh"

#include "BooleanSetting.hh"
#include "Probe.hh"
#include "TclCallback.hh"
#include "serialize_meta.hh"

#include <cstdint>

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
	static constexpr uint8_t MXD = 0x20;
	static constexpr uint8_t MXS = 0x10;
	static constexpr uint8_t DIY = 0x08;
	static constexpr uint8_t DIX = 0x04;
	static constexpr uint8_t EQ  = 0x02;
	static constexpr uint8_t MAJ = 0x01;

public:
	VDPCmdEngine(VDP& vdp, CommandController& commandController);

	/** Reinitialize Renderer state.
	  * @param time The moment in time the reset occurs.
	  */
	void reset(EmuTime time);

	/** Synchronizes the command engine with the VDP.
	  * Ideally this would be a private method, but the current
	  * design doesn't allow that.
	  * @param time The moment in emulated time to sync to.
	  */
	void sync(EmuTime time) {
		if (CMD) sync2(time);
	}
	void sync2(EmuTime time);

	/** Steal a VRAM access slot from the CmdEngine.
	 * Used when the CPU reads/writes VRAM.
	 * @param time The moment in time the CPU read/write is performed.
	 */
	void stealAccessSlot(EmuTime time) {
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
	[[nodiscard]] uint8_t getStatus(EmuTime time) {
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
	[[nodiscard]] uint8_t readColor(EmuTime time) {
		sync(time);
		return COL;
	}
	void resetColor() {
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
	[[nodiscard]] unsigned getBorderX(EmuTime time) {
		sync(time);
		return ASX;
	}

	/** Writes to a command register.
	  * @param index The register [0..14] to write to.
	  * @param value The new value for the specified register.
	  * @param time The moment in emulated time this write occurs.
	  */
	void setCmdReg(uint8_t index, uint8_t value, EmuTime time);

	/** Read the content of a command register. This method is meant to
	  * be used by the debugger, there is no strict guarantee that the
	  * returned value is the correct value at _exactly_ this moment in
	  * time (IOW this method does not sync the complete CmdEngine)
	  * @param index The register [0..14] to read from.
	  */
	[[nodiscard]] uint8_t peekCmdReg(uint8_t index, EmuTime time);

	/** Informs the command engine of a VDP display mode change.
	  * @param mode The new display mode.
	  * @param cmdBit Are VDP commands allowed in non-bitmap mode.
	  * @param time The moment in emulated time this change occurs.
	  */
	void updateDisplayMode(DisplayMode mode, bool cmdBit, EmuTime time);

	// For debugging only
	bool commandInProgress(EmuTime time) {
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
	void executeCommand(EmuTime time);

	void setStatusChangeTime(EmuTime t);
	void calcFinishTime(unsigned NX, unsigned NY, unsigned ticksPerPixel);

	                        void startAbrt(EmuTime time);
	                        void startPoint(EmuTime time);
	                        void startPset(EmuTime time);
	                        void startSrch(EmuTime time);
	                        void startLine(EmuTime time);
	template<typename Mode> void startLmmv(EmuTime time);
	template<typename Mode> void startLmmm(EmuTime time);
	template<typename Mode> void startLmcm(EmuTime time);
	template<typename Mode> void startLmmc(EmuTime time);
	template<typename Mode> void startHmmv(EmuTime time);
	template<typename Mode> void startHmmm(EmuTime time);
	template<typename Mode> void startYmmm(EmuTime time);
	template<typename Mode> void startHmmc(EmuTime time);

	template<typename Mode>                 void executePoint(EmuTime limit);
	template<typename Mode, typename LogOp> void executePset(EmuTime limit);
	template<typename Mode>                 void executeSrch(EmuTime limit);
	template<typename Mode, typename LogOp> void executeLine(EmuTime limit);
	template<typename Mode, typename LogOp> void executeLmmv(EmuTime limit);
	template<typename Mode, typename LogOp> void executeLmmm(EmuTime limit);
	template<typename Mode>                 void executeLmcm(EmuTime limit);
	template<typename Mode, typename LogOp> void executeLmmc(EmuTime limit);
	template<typename Mode>                 void executeHmmv(EmuTime limit);
	template<typename Mode>                 void executeHmmm(EmuTime limit);
	template<typename Mode>                 void executeYmmm(EmuTime limit);
	template<typename Mode>                 void executeHmmc(EmuTime limit);

	// Advance to the next access slot at or past the given time.
	EmuTime getNextAccessSlot(EmuTime time) const {
		return vdp.getAccessSlot(time, VDPAccessSlots::Delta::D0);
	}
	void nextAccessSlot(EmuTime time) {
		engineTime = getNextAccessSlot(time);
	}
	// Advance to the next access slot that is at least 'delta' cycles past
	// the current one.
	EmuTime getNextAccessSlot(EmuTime time, VDPAccessSlots::Delta delta) const {
		return vdp.getAccessSlot(time, delta);
	}
	void nextAccessSlot(VDPAccessSlots::Delta delta) {
		engineTime = getNextAccessSlot(engineTime, delta);
	}
	VDPAccessSlots::Calculator getSlotCalculator(
			EmuTime limit) const {
		return vdp.getAccessSlotCalculator(engineTime, limit);
	}

	/** Finished executing graphical operation.
	  */
	void commandDone(EmuTime time);

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
	uint8_t COL{0}, ARG{0}, CMD{0};

	/** The last executed command (for debugging only).
	  * A copy of the above registers when the command starts. Remains valid
	  * until the next command starts (also if the corresponding VDP
	  * registers are being changed).
	  */
	int lastSX{0}, lastSY{0}, lastDX{0}, lastDY{0}, lastNX{0}, lastNY{0};
	uint8_t lastCOL{0}, lastARG{0}, lastCMD{0};

	/** When a command needs multiple VRAM accesses per pixel, the result
	 * of intermediate reads is stored in these variables. */
	uint8_t tmpSrc{0};
	uint8_t tmpDst{0};

	/** The command engine status (part of S#2).
	  * Bit 7 (TR) is set when the command engine is ready for
	  * a pixel transfer.
	  * Bit 4 (BD) is set when the boundary color is detected.
	  * Bit 0 (CE) is set when a command is in progress.
	  */
	uint8_t status{0};

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
