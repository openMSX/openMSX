#ifndef V9990CMDENGINE_HH
#define V9990CMDENGINE_HH

#include "Observer.hh"
#include "EmuTime.hh"
#include "serialize_meta.hh"
#include "openmsx.hh"

namespace openmsx {

class V9990;
class V9990VRAM;
class Setting;
class RenderSettings;
class BooleanSetting;

/** Command engine.
  */
class V9990CmdEngine final : private Observer<Setting>
{
public:
	// status bits
	static constexpr byte TR = 0x80;
	static constexpr byte BD = 0x10;
	static constexpr byte CE = 0x01;

	V9990CmdEngine(V9990& vdp, EmuTime::param time,
	               RenderSettings& settings);
	~V9990CmdEngine();

	/** Re-initialise the command engine's state
	  * @param time   Moment in emulated time the reset occurs
	  */
	void reset(EmuTime::param time);

	/** Synchronizes the command engine with the V9990
	  * @param time The moment in emulated time to sync to.
	  */
	inline void sync(EmuTime::param time) {
		if (CMD >> 4) sync2(time);
	}
	void sync2(EmuTime::param time);

	/** Set a value to one of the command registers
	  */
	void setCmdReg(byte reg, byte val, EmuTime::param time);

	/** set the data byte
	  */
	void setCmdData(byte value, EmuTime::param time);

	/** read the command data byte
	  */
	[[nodiscard]] byte getCmdData(EmuTime::param time);

	/** read the command data byte (without side-effects)
	  */
	[[nodiscard]] byte peekCmdData(EmuTime::param time) const;

	/** Get command engine related status bits
	  *  - TR command data transfer ready (bit 7)
	  *  - BD border color detect         (bit 4)
	  *  - CE command being executed      (bit 0)
	  */
	[[nodiscard]] byte getStatus(EmuTime::param time) const {
		// note: used for both normal and debug read
		const_cast<V9990CmdEngine*>(this)->sync(time);
		return status;
	}

	[[nodiscard]] word getBorderX(EmuTime::param time) const {
		// note: used for both normal and debug read
		const_cast<V9990CmdEngine*>(this)->sync(time);
		return borderX;
	}

	/** Calculate an (under-)estimation for when the command will finish.
	  * The main purpose of this function is to insert extra sync points
	  * so that the Command-End (CE) interrupt properly synchronizes with
	  * CPU emulation.
	  * This estimation:
	  * - Is the current time (engineTime) when no command is executing
	  *   or the command doesn't require extra syncs.
	  * - Is allowed to be an underestimation, on the condition that (when
	  *   that point in time is reached) the new estimation is more
	  *   accurate and converges to the actual end time.
	  */
	[[nodiscard]] EmuTime estimateCmdEnd() const;

	[[nodiscard]] const V9990& getVDP() const { return vdp; }
	[[nodiscard]] bool getBrokenTiming() const { return brokenTiming; }

	void serialize(Archive auto& ar, unsigned version);

private:
	class V9990P1 {
	public:
		using Type = byte;
		static constexpr word BITS_PER_PIXEL  = 4;
		static constexpr word PIXELS_PER_BYTE = 2;
		static inline unsigned getPitch(unsigned width);
		static inline unsigned addressOf(unsigned x, unsigned y, unsigned pitch);
		static inline byte point(V9990VRAM& vram,
		                         unsigned x, unsigned y, unsigned pitch);
		static inline byte shift(byte value, unsigned fromX, unsigned toX);
		static inline byte shiftMask(unsigned x);
		static inline const byte* getLogOpLUT(byte op);
		static inline byte logOp(const byte* lut, byte src, byte dst);
		static inline void pset(
			V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
			byte srcColor, word mask, const byte* lut, byte op);
		static inline void psetColor(
			V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
			word color, word mask, const byte* lut, byte op);
	};

	class V9990P2 {
	public:
		using Type = byte;
		static constexpr word BITS_PER_PIXEL  = 4;
		static constexpr word PIXELS_PER_BYTE = 2;
		static inline unsigned getPitch(unsigned width);
		static inline unsigned addressOf(unsigned x, unsigned y, unsigned pitch);
		static inline byte point(V9990VRAM& vram,
		                         unsigned x, unsigned y, unsigned pitch);
		static inline byte shift(byte value, unsigned fromX, unsigned toX);
		static inline byte shiftMask(unsigned x);
		static inline const byte* getLogOpLUT(byte op);
		static inline byte logOp(const byte* lut, byte src, byte dst);
		static inline void pset(
			V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
			byte srcColor, word mask, const byte* lut, byte op);
		static inline void psetColor(
			V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
			word color, word mask, const byte* lut, byte op);
	};

	class V9990Bpp2 {
	public:
		using Type = byte;
		static constexpr word BITS_PER_PIXEL  = 2;
		static constexpr word PIXELS_PER_BYTE = 4;
		static inline unsigned getPitch(unsigned width);
		static inline unsigned addressOf(unsigned x, unsigned y, unsigned pitch);
		static inline byte point(V9990VRAM& vram,
		                         unsigned x, unsigned y, unsigned pitch);
		static inline byte shift(byte value, unsigned fromX, unsigned toX);
		static inline byte shiftMask(unsigned x);
		static inline const byte* getLogOpLUT(byte op);
		static inline byte logOp(const byte* lut, byte src, byte dst);
		static inline void pset(
			V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
			byte srcColor, word mask, const byte* lut, byte op);
		static inline void psetColor(
			V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
			word color, word mask, const byte* lut, byte op);
	};

	class V9990Bpp4 {
	public:
		using Type = byte;
		static constexpr word BITS_PER_PIXEL  = 4;
		static constexpr word PIXELS_PER_BYTE = 2;
		static inline unsigned getPitch(unsigned width);
		static inline unsigned addressOf(unsigned x, unsigned y, unsigned pitch);
		static inline byte point(V9990VRAM& vram,
		                         unsigned x, unsigned y, unsigned pitch);
		static inline byte shift(byte value, unsigned fromX, unsigned toX);
		static inline byte shiftMask(unsigned x);
		static inline const byte* getLogOpLUT(byte op);
		static inline byte logOp(const byte* lut, byte src, byte dst);
		static inline void pset(
			V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
			byte srcColor, word mask, const byte* lut, byte op);
		static inline void psetColor(
			V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
			word color, word mask, const byte* lut, byte op);
	};

	class V9990Bpp8 {
	public:
		using Type = byte;
		static constexpr word BITS_PER_PIXEL  = 8;
		static constexpr word PIXELS_PER_BYTE = 1;
		static inline unsigned getPitch(unsigned width);
		static inline unsigned addressOf(unsigned x, unsigned y, unsigned pitch);
		static inline byte point(V9990VRAM& vram,
		                         unsigned x, unsigned y, unsigned pitch);
		static inline byte shift(byte value, unsigned fromX, unsigned toX);
		static inline byte shiftMask(unsigned x);
		static inline const byte* getLogOpLUT(byte op);
		static inline byte logOp(const byte* lut, byte src, byte dst);
		static inline void pset(
			V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
			byte srcColor, word mask, const byte* lut, byte op);
		static inline void psetColor(
			V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
			word color, word mask, const byte* lut, byte op);
	};

	class V9990Bpp16 {
	public:
		using Type = word;
		static constexpr word BITS_PER_PIXEL  = 16;
		static constexpr word PIXELS_PER_BYTE = 0;
		static inline unsigned getPitch(unsigned width);
		static inline unsigned addressOf(unsigned x, unsigned y, unsigned pitch);
		static inline word point(V9990VRAM& vram,
		                         unsigned x, unsigned y, unsigned pitch);
		static inline word shift(word value, unsigned fromX, unsigned toX);
		static inline word shiftMask(unsigned x);
		static inline const byte* getLogOpLUT(byte op);
		static inline word logOp(const byte* lut, word src, word dst, bool transp);
		static inline void pset(
			V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
			word srcColor, word mask, const byte* lut, byte op);
		static inline void psetColor(
			V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
			word color, word mask, const byte* lut, byte op);
	};

	void startSTOP  (EmuTime::param time);
	void startLMMC  (EmuTime::param time);
	void startLMMC16(EmuTime::param time);
	void startLMMV  (EmuTime::param time);
	void startLMCM  (EmuTime::param time);
	void startLMCM16(EmuTime::param time);
	void startLMMM  (EmuTime::param time);
	void startCMMC  (EmuTime::param time);
	void startCMMK  (EmuTime::param time);
	void startCMMM  (EmuTime::param time);
	void startBMXL  (EmuTime::param time);
	void startBMLX  (EmuTime::param time);
	void startBMLL  (EmuTime::param time);
	void startBMLL16(EmuTime::param time);
	void startLINE  (EmuTime::param time);
	void startSRCH  (EmuTime::param time);
	template<typename Mode> void startPOINT(EmuTime::param time);
	template<typename Mode> void startPSET (EmuTime::param time);
	void startADVN  (EmuTime::param time);

	                        void executeSTOP (EmuTime::param limit);
	template<typename Mode> void executeLMMC (EmuTime::param limit);
	template<typename Mode> void executeLMMV (EmuTime::param limit);
	template<typename Mode> void executeLMCM (EmuTime::param limit);
	template<typename Mode> void executeLMMM (EmuTime::param limit);
	template<typename Mode> void executeCMMC (EmuTime::param limit);
	                        void executeCMMK (EmuTime::param limit);
	template<typename Mode> void executeCMMM (EmuTime::param limit);
	template<typename Mode> void executeBMXL (EmuTime::param limit);
	template<typename Mode> void executeBMLX (EmuTime::param limit);
	template<typename Mode> void executeBMLL (EmuTime::param limit);
	template<typename Mode> void executeLINE (EmuTime::param limit);
	template<typename Mode> void executeSRCH (EmuTime::param limit);
	template<typename Mode> void executePOINT(EmuTime::param limit);
	                        void executePSET (EmuTime::param limit);
	                        void executeADVN (EmuTime::param limit);

	RenderSettings& settings;

	/** Only call reportV9990Command() when this setting is turned on
	  */
	std::shared_ptr<BooleanSetting> cmdTraceSetting;

	/** V9990 VDP this engine belongs to
	  */
	V9990& vdp;
	V9990VRAM& vram;

	EmuTime engineTime;

	/** VRAM read/write address for various commands
	  */
	unsigned srcAddress;
	unsigned dstAddress;
	unsigned nbBytes;

	/** The X coord of a border detected by SRCH
	 */
	word borderX;

	/** counters
	  */
	word ASX, ADX, ANX, ANY;

	/** Command parameters
	  */
	word SX, SY, DX, DY, NX, NY;
	word WM, fgCol, bgCol;
	byte ARG, LOG, CMD;

	unsigned cmdMode; // TODO keep this up-to-date (now it's calculated at the start of a command)

	/** Status bits
	 */
	byte status;

	/** Data byte to transfer between V9990 and CPU
	  */
	byte data;

	/** Bit counter for CMMx commands
	  */
	byte bitsLeft;

	/** Partial data for LMMC command
	  */
	byte partial;

	/** Should command end after next getCmdData().
	 */
	bool endAfterRead;

	/** Real command timing or instantaneous (broken) timing
	 */
	bool brokenTiming;

	/** The running command is complete. Perform necessary clean-up actions.
	  */
	void cmdReady(EmuTime::param time);

	/** For debugging: Print the info about the current command.
	  */
	void reportV9990Command() const;

	// Observer<Setting>
	void update(const Setting& setting) noexcept override;

	void setCommandMode();

	[[nodiscard]] inline unsigned getWrappedNX() const {
		return NX ? NX : 2048;
	}
	[[nodiscard]] inline unsigned getWrappedNY() const {
		return NY ? NY : 4096;
	}
};
SERIALIZE_CLASS_VERSION(V9990CmdEngine, 2);

} // namespace openmsx

#endif
