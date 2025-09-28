#ifndef V9990CMDENGINE_HH
#define V9990CMDENGINE_HH

#include "EmuTime.hh"
#include "serialize_meta.hh"

#include "Observer.hh"

#include <cstdint>

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
	static constexpr uint8_t TR = 0x80;
	static constexpr uint8_t BD = 0x10;
	static constexpr uint8_t CE = 0x01;

	V9990CmdEngine(V9990& vdp, EmuTime time,
	               RenderSettings& settings);
	~V9990CmdEngine();

	/** Re-initialise the command engine's state
	  * @param time   Moment in emulated time the reset occurs
	  */
	void reset(EmuTime time);

	/** Synchronizes the command engine with the V9990
	  * @param time The moment in emulated time to sync to.
	  */
	void sync(EmuTime time) {
		if (CMD >> 4) sync2(time);
	}
	void sync2(EmuTime time);

	/** Set a value to one of the command registers
	  */
	void setCmdReg(uint8_t reg, uint8_t val, EmuTime time);

	/** set the data byte
	  */
	void setCmdData(uint8_t value, EmuTime time);

	/** read the command data byte
	  */
	[[nodiscard]] uint8_t getCmdData(EmuTime time);

	/** read the command data byte (without side-effects)
	  */
	[[nodiscard]] uint8_t peekCmdData(EmuTime time) const;

	/** Get command engine related status bits
	  *  - TR command data transfer ready (bit 7)
	  *  - BD border color detect         (bit 4)
	  *  - CE command being executed      (bit 0)
	  */
	[[nodiscard]] uint8_t getStatus(EmuTime time) const {
		// note: used for both normal and debug read
		const_cast<V9990CmdEngine*>(this)->sync(time);
		return status;
	}

	[[nodiscard]] uint16_t getBorderX(EmuTime time) const {
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

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	class V9990P1 {
	public:
		using Type = uint8_t;
		static constexpr uint16_t BITS_PER_PIXEL  = 4;
		static constexpr uint16_t PIXELS_PER_BYTE = 2;
		static unsigned getPitch(unsigned width);
		static unsigned addressOf(unsigned x, unsigned y, unsigned pitch);
		static uint8_t point(const V9990VRAM& vram,
		                  unsigned x, unsigned y, unsigned pitch);
		static uint8_t shift(uint8_t value, unsigned fromX, unsigned toX);
		static uint8_t shiftMask(unsigned x);
		static std::span<const uint8_t, 256 * 256> getLogOpLUT(uint8_t op);
		static uint8_t logOp(std::span<const uint8_t, 256 * 256> lut, uint8_t src, uint8_t dst);
		static void pset(
			V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
			uint8_t srcColor, uint16_t mask, std::span<const uint8_t, 256 * 256> lut, uint8_t op);
		static void psetColor(
			V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
			uint16_t color, uint16_t mask, std::span<const uint8_t, 256 * 256> lut, uint8_t op);
	};

	class V9990P2 {
	public:
		using Type = uint8_t;
		static constexpr uint16_t BITS_PER_PIXEL  = 4;
		static constexpr uint16_t PIXELS_PER_BYTE = 2;
		static unsigned getPitch(unsigned width);
		static unsigned addressOf(unsigned x, unsigned y, unsigned pitch);
		static uint8_t point(const V9990VRAM& vram,
		                  unsigned x, unsigned y, unsigned pitch);
		static uint8_t shift(uint8_t value, unsigned fromX, unsigned toX);
		static uint8_t shiftMask(unsigned x);
		static std::span<const uint8_t, 256 * 256> getLogOpLUT(uint8_t op);
		static uint8_t logOp(std::span<const uint8_t, 256 * 256> lut, uint8_t src, uint8_t dst);
		static void pset(
			V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
			uint8_t srcColor, uint16_t mask, std::span<const uint8_t, 256 * 256> lut, uint8_t op);
		static void psetColor(
			V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
			uint16_t color, uint16_t mask, std::span<const uint8_t, 256 * 256> lut, uint8_t op);
	};

	class V9990Bpp2 {
	public:
		using Type = uint8_t;
		static constexpr uint16_t BITS_PER_PIXEL  = 2;
		static constexpr uint16_t PIXELS_PER_BYTE = 4;
		static unsigned getPitch(unsigned width);
		static unsigned addressOf(unsigned x, unsigned y, unsigned pitch);
		static uint8_t point(const V9990VRAM& vram,
		                  unsigned x, unsigned y, unsigned pitch);
		static uint8_t shift(uint8_t value, unsigned fromX, unsigned toX);
		static uint8_t shiftMask(unsigned x);
		static std::span<const uint8_t, 256 * 256> getLogOpLUT(uint8_t op);
		static uint8_t logOp(std::span<const uint8_t, 256 * 256> lut, uint8_t src, uint8_t dst);
		static void pset(
			V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
			uint8_t srcColor, uint16_t mask, std::span<const uint8_t, 256 * 256> lut, uint8_t op);
		static void psetColor(
			V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
			uint16_t color, uint16_t mask, std::span<const uint8_t, 256 * 256> lut, uint8_t op);
	};

	class V9990Bpp4 {
	public:
		using Type = uint8_t;
		static constexpr uint16_t BITS_PER_PIXEL  = 4;
		static constexpr uint16_t PIXELS_PER_BYTE = 2;
		static unsigned getPitch(unsigned width);
		static unsigned addressOf(unsigned x, unsigned y, unsigned pitch);
		static uint8_t point(const V9990VRAM& vram,
		                  unsigned x, unsigned y, unsigned pitch);
		static uint8_t shift(uint8_t value, unsigned fromX, unsigned toX);
		static uint8_t shiftMask(unsigned x);
		static std::span<const uint8_t, 256 * 256> getLogOpLUT(uint8_t op);
		static uint8_t logOp(std::span<const uint8_t, 256 * 256> lut, uint8_t src, uint8_t dst);
		static void pset(
			V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
			uint8_t srcColor, uint16_t mask, std::span<const uint8_t, 256 * 256> lut, uint8_t op);
		static void psetColor(
			V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
			uint16_t color, uint16_t mask, std::span<const uint8_t, 256 * 256> lut, uint8_t op);
	};

	class V9990Bpp8 {
	public:
		using Type = uint8_t;
		static constexpr uint16_t BITS_PER_PIXEL  = 8;
		static constexpr uint16_t PIXELS_PER_BYTE = 1;
		static unsigned getPitch(unsigned width);
		static unsigned addressOf(unsigned x, unsigned y, unsigned pitch);
		static uint8_t point(const V9990VRAM& vram,
		                  unsigned x, unsigned y, unsigned pitch);
		static uint8_t shift(uint8_t value, unsigned fromX, unsigned toX);
		static uint8_t shiftMask(unsigned x);
		static std::span<const uint8_t, 256 * 256> getLogOpLUT(uint8_t op);
		static uint8_t logOp(std::span<const uint8_t, 256 * 256> lut, uint8_t src, uint8_t dst);
		static void pset(
			V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
			uint8_t srcColor, uint16_t mask, std::span<const uint8_t, 256 * 256> lut, uint8_t op);
		static void psetColor(
			V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
			uint16_t color, uint16_t mask, std::span<const uint8_t, 256 * 256> lut, uint8_t op);
	};

	class V9990Bpp16 {
	public:
		using Type = uint16_t;
		static constexpr uint16_t BITS_PER_PIXEL  = 16;
		static constexpr uint16_t PIXELS_PER_BYTE = 0;
		static unsigned getPitch(unsigned width);
		static unsigned addressOf(unsigned x, unsigned y, unsigned pitch);
		static uint16_t point(const V9990VRAM& vram,
		                  unsigned x, unsigned y, unsigned pitch);
		static uint16_t shift(uint16_t value, unsigned fromX, unsigned toX);
		static uint16_t shiftMask(unsigned x);
		static std::span<const uint8_t, 256 * 256> getLogOpLUT(uint8_t op);
		static uint16_t logOp(std::span<const uint8_t, 256 * 256> lut, uint16_t src, uint16_t dst, bool transp);
		static void pset(
			V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
			uint16_t srcColor, uint16_t mask, std::span<const uint8_t, 256 * 256> lut, uint8_t op);
		static void psetColor(
			V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
			uint16_t color, uint16_t mask, std::span<const uint8_t, 256 * 256> lut, uint8_t op);
	};

	void startSTOP  (EmuTime time);
	void startLMMC  (EmuTime time);
	void startLMMC16(EmuTime time);
	void startLMMV  (EmuTime time);
	void startLMCM  (EmuTime time);
	void startLMCM16(EmuTime time);
	void startLMMM  (EmuTime time);
	void startCMMC  (EmuTime time);
	void startCMMK  (EmuTime time);
	void startCMMM  (EmuTime time);
	void startBMXL  (EmuTime time);
	void startBMLX  (EmuTime time);
	void startBMLL  (EmuTime time);
	void startBMLL16(EmuTime time);
	void startLINE  (EmuTime time);
	void startSRCH  (EmuTime time);
	template<typename Mode> void startPOINT(EmuTime time);
	template<typename Mode> void startPSET (EmuTime time);
	void startADVN  (EmuTime time);

	                        void executeSTOP (EmuTime limit);
	template<typename Mode> void executeLMMC (EmuTime limit);
	template<typename Mode> void executeLMMV (EmuTime limit);
	template<typename Mode> void executeLMCM (EmuTime limit);
	template<typename Mode> void executeLMMM (EmuTime limit);
	template<typename Mode> void executeCMMC (EmuTime limit);
	                        void executeCMMK (EmuTime limit);
	template<typename Mode> void executeCMMM (EmuTime limit);
	template<typename Mode> void executeBMXL (EmuTime limit);
	template<typename Mode> void executeBMLX (EmuTime limit);
	template<typename Mode> void executeBMLL (EmuTime limit);
	template<typename Mode> void executeLINE (EmuTime limit);
	template<typename Mode> void executeSRCH (EmuTime limit);
	template<typename Mode> void executePOINT(EmuTime limit);
	                        void executePSET (EmuTime limit);
	                        void executeADVN (EmuTime limit);

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
	uint16_t borderX;

	/** counters
	  */
	uint16_t ASX, ADX, ANX, ANY;

	/** Command parameters
	  */
	uint16_t SX, SY, DX, DY, NX, NY;
	uint16_t WM, fgCol, bgCol;
	uint8_t ARG, LOG, CMD;

	unsigned cmdMode; // TODO keep this up-to-date (now it's calculated at the start of a command)

	/** Status bits
	 */
	uint8_t status;

	/** Data byte to transfer between V9990 and CPU
	  */
	uint8_t data;

	/** Bit counter for CMMx commands
	  */
	uint8_t bitsLeft;

	/** Partial data for LMMC command
	  */
	uint8_t partial;

	/** Should command end after next getCmdData().
	 */
	bool endAfterRead;

	/** Real command timing or instantaneous (broken) timing
	 */
	bool brokenTiming;

	/** The running command is complete. Perform necessary clean-up actions.
	  */
	void cmdReady(EmuTime time);

	/** For debugging: Print the info about the current command.
	  */
	void reportV9990Command() const;

	// Observer<Setting>
	void update(const Setting& setting) noexcept override;

	void setCommandMode();

	[[nodiscard]] uint16_t getWrappedNX() const {
		return NX ? NX : 2048;
	}
	[[nodiscard]] uint16_t getWrappedNY() const {
		return NY ? NY : 4096;
	}
};
SERIALIZE_CLASS_VERSION(V9990CmdEngine, 2);

} // namespace openmsx

#endif
