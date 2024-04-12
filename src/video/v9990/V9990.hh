#ifndef V9990_HH
#define V9990_HH

#include "MSXDevice.hh"
#include "Schedulable.hh"
#include "TclCallback.hh"
#include "VideoSystemChangeListener.hh"
#include "IRQHelper.hh"
#include "V9990CmdEngine.hh"
#include "V9990DisplayTiming.hh"
#include "V9990ModeEnum.hh"
#include "V9990VRAM.hh"
#include "SimpleDebuggable.hh"
#include "Clock.hh"
#include "narrow.hh"
#include "openmsx.hh"
#include "one_of.hh"
#include "outer.hh"
#include "serialize_meta.hh"
#include "unreachable.hh"
#include <array>
#include <memory>
#include <optional>

namespace openmsx {

class PostProcessor;
class Display;
class V9990Renderer;

/** Implementation of the Yamaha V9990 VDP as used in the GFX9000
  * cartridge by Sunrise.
  */
class V9990 final : public MSXDevice, private VideoSystemChangeListener
{
public:
	explicit V9990(const DeviceConfig& config);
	~V9990() override;

	// MSXDevice interface:
	void powerUp(EmuTime::param time) override;
	void reset(EmuTime::param time) override;
	[[nodiscard]] byte readIO(word port, EmuTime::param time) override;
	[[nodiscard]] byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;

	/** Used by Video9000 to be able to couple the VDP and V9990 output.
	 * Can return nullptr in case of renderer=none. This value can change
	 * over the lifetime of the V9990 object (on renderer switch).
	 */
	[[nodiscard]] PostProcessor* getPostProcessor() const;

	/** Obtain a reference to the V9990's VRAM
	  */
	[[nodiscard]] inline V9990VRAM& getVRAM() {
		return vram;
	}

	/** Get interlace status.
	  * @return True iff interlace is enabled.
	  */
	[[nodiscard]] inline bool isInterlaced() const {
		return interlaced;
	}

	/** Get even/odd page alternation status.
	  * @return True iff even/odd page alternation is enabled.
	  */
	[[nodiscard]] inline bool isEvenOddEnabled() const {
		return (regs[SCREEN_MODE_1] & 0x04) != 0;
	}

	/** Is the even or odd field being displayed?
	  * @return True iff the odd lines should be displayed.
	  */
	[[nodiscard]] inline bool getEvenOdd() const {
		return (status & 0x02) != 0;
	}

	/** Is the display enabled?
	  *  Note this is simpler than the V99x8 version. Probably ok
	  *  because V9990 doesn't have the same overscan trick (?)
	  * @return true iff enabled
	  */
	[[nodiscard]] inline bool isDisplayEnabled() const {
		return isDisplayArea && displayEnabled;
	}

	/** Are sprites (cursors) enabled?
	  * @return true iff enabled
	  */
	[[nodiscard]] inline bool spritesEnabled() const {
		return !(regs[CONTROL] & 0x40);
	}

	/** Get palette offset.
	  * Result is between [0..15] (this represents a logical number
	  * between [0..63] with lowest two bits always 0).
	  * @return palette offset
	  */
	[[nodiscard]] inline byte getPaletteOffset() const {
		return (regs[PALETTE_CONTROL] & 0x0F);
	}

	/** Get palette entry
	  * @param[in] index The palette index
	  * ys is only true iff
	  *  - bit 5 (YSE) in R#8 is set
	  *  - there is an external video source set
	  *  - the YS bit in the palette-entry is set
	  */
	struct GetPaletteResult {
		byte r, g, b;
		bool ys;
	};
	[[nodiscard]] GetPaletteResult getPalette(int index) const;

	/** Get the number of elapsed UC ticks in this frame.
	  * @param  time Point in emulated time.
	  * @return      Number of UC ticks.
	  */
	[[nodiscard]] inline int getUCTicksThisFrame(EmuTime::param time) const {
		return narrow<int>(frameStartTime.getTicksTill_fast(time));
	}

	/** Is PAL timing active?
	  * This setting is fixed at start of frame.
	  * @return True if PAL timing, false if NTSC timing.
	  */
	[[nodiscard]] inline bool isPalTiming() const {
		return palTiming;
	}

	/** Returns true iff in overscan mode
	  */
	[[nodiscard]] inline bool isOverScan() const {
		return mode == one_of(B0, B2, B4);
	}

	/** Should this frame be superimposed?
	  * This is a combination of bit 5 (YSE) in R#8 and the presence of an
	  * external video source (see setExternalVideoSource()). Though
	  * because this property only changes once per frame we can't directly
	  * calculate it like that.
	  */
	[[nodiscard]] inline bool isSuperimposing() const {
		return superimposing;
	}

	/** Is there an external video source available to superimpose on. */
	void setExternalVideoSource(bool enable) {
		externalVideoSource = enable;
	}

	/** In overscan mode the cursor position is still specified with
	  * 'normal' (non-overscan) y-coordinates. This method returns the
	  * offset between those two coord-systems.
	  */
	[[nodiscard]] inline unsigned getCursorYOffset() const {
		// TODO vertical set-adjust may or may not influence this,
		//      need to investigate that.
		if (!isOverScan()) return 0;
		return isPalTiming()
			? V9990DisplayTiming::displayPAL_MCLK .border1
			: V9990DisplayTiming::displayNTSC_MCLK.border1;
	}

	/** Convert UC ticks to V9990 pixel position on a line
	  * @param ticks  Nr of UC Ticks
	  * @param mode   Display mode
	  * @return       Pixel position
	  * TODO: Move this to V9990DisplayTiming??
	  */
	[[nodiscard]] static inline int UCtoX(int ticks, V9990DisplayMode mode) {
		int x;
		ticks = ticks % V9990DisplayTiming::UC_TICKS_PER_LINE;
		switch (mode) {
		case P1: x = ticks / 8;  break;
		case P2: x = ticks / 4;  break;
		case B0: x = ticks /12;  break;
		case B1: x = ticks / 8;  break;
		case B2: x = ticks / 6;  break;
		case B3: x = ticks / 4;  break;
		case B4: x = ticks / 3;  break;
		case B5: x = 1;          break;
		case B6: x = 1;          break;
		case B7: x = ticks / 2;  break;
		default: x = 1;
		}
		return x;
	}

	/** Return the current display mode
	  */
	[[nodiscard]] inline V9990DisplayMode getDisplayMode() const {
		return mode;
	}

	/** Return the current color mode
	  */
	V9990ColorMode getColorMode() const;

	/** Return the amount of bits per pixels.
	  * Result is only meaningful for Bx modes.
	  * @return Bpp 0 ->  2bpp
	  *             1 ->  4bpp
	  *             2 ->  8bpp
	  *             3 -> 16bpp
	  */
	[[nodiscard]] inline unsigned getColorDepth() const {
		return regs[SCREEN_MODE_0] & 0x03;
	}

	/** Return the current back drop color
	  * @return  Index the color palette
	  */
	[[nodiscard]] inline byte getBackDropColor() const {
		return regs[BACK_DROP_COLOR];
	}

	/** Returns the X scroll offset for screen A of P1 and other modes
	  */
	[[nodiscard]] inline unsigned getScrollAX() const {
		return regs[SCROLL_CONTROL_AX0] + 8 * regs[SCROLL_CONTROL_AX1];
	}

	/** Returns the Y scroll offset for screen A of P1 and other modes
	  */
	[[nodiscard]] inline unsigned getScrollAY() const {
		return regs[SCROLL_CONTROL_AY0] + 256 * scrollAYHigh;
	}

	/** Returns the X scroll offset for screen B of P1 mode
	  */
	[[nodiscard]] inline unsigned getScrollBX() const {
		return regs[SCROLL_CONTROL_BX0] + 8 * regs[SCROLL_CONTROL_BX1];
	}

	/** Returns the Y scroll offset for screen B of P1 mode
	  */
	[[nodiscard]] inline unsigned getScrollBY() const {
		return regs[SCROLL_CONTROL_BY0] + 256 * scrollBYHigh;
	}

	/** Returns the vertical roll mask
	  */
	[[nodiscard]] inline unsigned getRollMask(unsigned maxMask) const {
		static std::array<unsigned, 4> rollMasks = {
			0xFFFF, // no rolling (use maxMask)
			0x00FF,
			0x01FF,
			0x00FF // TODO check this (undocumented)
		};
		unsigned t = regs[SCROLL_CONTROL_AY1] >> 6;
		return t ? rollMasks[t] : maxMask;
	}

	/** Return the image width
	  */
	[[nodiscard]] inline unsigned getImageWidth() const {
		switch (regs[SCREEN_MODE_0] & 0xC0) {
		case 0x00: // P1
			return 256;
		case 0x40: // P2
			return 512;
		case 0x80: // Bx
		default:   // standby TODO check this
			return (256 << ((regs[SCREEN_MODE_0] & 0x0C) >> 2));
		}
	}
	/** Return the display width
	  */
	[[nodiscard]] inline unsigned getLineWidth() const {
		switch (getDisplayMode()) {
		case B0:          return  213;
		case P1: case B1: return  320;
		case B2:          return  426;
		case P2: case B3: return  640;
		case B4:          return  853;
		case B5: case B6: return    1; // not supported
		case B7:          return 1280;
		default:
			UNREACHABLE;
		}
	}

	/** Command execution ready
	  */
	inline void cmdReady() {
		raiseIRQ(CMD_IRQ);
	}

	/** Return the sprite pattern table base address
	  */
	[[nodiscard]] inline int getSpritePatternAddress(V9990DisplayMode m) const {
		switch (m) {
		case P1:
			return (int(regs[SPRITE_PATTERN_ADDRESS] & 0x0E) << 14);
		case P2:
			return (int(regs[SPRITE_PATTERN_ADDRESS] & 0x0F) << 15);
		default:
			return 0;
		}
	}

	/** return sprite palette offset
	  */
	[[nodiscard]] inline byte getSpritePaletteOffset() const {
		return narrow_cast<byte>(regs[SPRITE_PALETTE_CONTROL] << 2);
	}

	/** Get horizontal display timings
	 */
	[[nodiscard]] inline const V9990DisplayPeriod& getHorizontalTiming() const {
		return *horTiming;
	}

	/** Get the number of VDP clock-ticks between the start of the line and
	  * the end of the left border.
	  */
	[[nodiscard]] inline int getLeftBorder() const {
		return horTiming->blank + horTiming->border1 +
		       (((regs[DISPLAY_ADJUST] & 0x0F) ^ 7) - 8) * 8;
	}
	/** Get the number of VDP clock-ticks between the start of the line and
	  * the end of the right border.
	  */
	[[nodiscard]] inline int getRightBorder() const {
		return getLeftBorder() + horTiming->display;
	}

	/** Get vertical display timings
	 */
	[[nodiscard]] inline const V9990DisplayPeriod& getVerticalTiming() const {
		return *verTiming;
	}

	[[nodiscard]] inline int getTopBorder() const {
		return verTiming->blank + verTiming->border1 +
		       (((regs[DISPLAY_ADJUST] >> 4) ^ 7) - 8);
	}
	[[nodiscard]] inline int getBottomBorder() const {
		return getTopBorder() + verTiming->display;
	}

	[[nodiscard]] inline unsigned getPriorityControlX() const {
		unsigned t = regs[PRIORITY_CONTROL] & 0x03;
		return (t == 0) ? 256 : t << 6;
	}
	[[nodiscard]] inline unsigned getPriorityControlY() const {
		unsigned t = regs[PRIORITY_CONTROL] & 0x0C;
		return (t == 0) ? 256 : t << 4;
	}

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// VideoSystemChangeListener interface:
	void preVideoSystemChange() noexcept override;
	void postVideoSystemChange() noexcept override;

	// Scheduler stuff
	struct SyncBase : Schedulable {
		explicit SyncBase(V9990& v9990) : Schedulable(v9990.getScheduler()) {}
		using Schedulable::setSyncPoint;
		using Schedulable::removeSyncPoint;
	protected:
		~SyncBase() = default;
	};

	struct SyncVSync final : SyncBase {
		using SyncBase::SyncBase;
		void executeUntil(EmuTime::param time) override {
			auto& v9990 = OUTER(V9990, syncVSync);
			v9990.execVSync(time);
		}
	} syncVSync;

	struct SyncDisplayStart final : SyncBase {
		using SyncBase::SyncBase;
		void executeUntil(EmuTime::param time) override {
			auto& v9990 = OUTER(V9990, syncDisplayStart);
			v9990.execDisplayStart(time);
		}
	} syncDisplayStart;

	struct SyncVScan final : SyncBase {
		using SyncBase::SyncBase;
		void executeUntil(EmuTime::param time) override {
			auto& v9990 = OUTER(V9990, syncVScan);
			v9990.execVScan(time);
		}
	} syncVScan;

	struct SyncHScan final : SyncBase {
		using SyncBase::SyncBase;
		void executeUntil(EmuTime::param /*time*/) override {
			auto& v9990 = OUTER(V9990, syncHScan);
			v9990.execHScan();
		}
	} syncHScan;

	struct SyncSetMode final : SyncBase {
		using SyncBase::SyncBase;
		void executeUntil(EmuTime::param time) override {
			auto& v9990 = OUTER(V9990, syncSetMode);
			v9990.execSetMode(time);
		}
	} syncSetMode;

	struct SyncCmdEnd final : SyncBase {
		using SyncBase::SyncBase;
		void executeUntil(EmuTime::param time) override {
			auto& v9990 = OUTER(V9990, syncCmdEnd);
			v9990.execCheckCmdEnd(time);
		}
	} syncCmdEnd;

	void execVSync(EmuTime::param time);
	void execDisplayStart(EmuTime::param time);
	void execVScan(EmuTime::param time);
	void execHScan();
	void execSetMode(EmuTime::param time);
	void execCheckCmdEnd(EmuTime::param time);

	// --- types ------------------------------------------------------

	/** IRQ types
	  */
	enum IRQType : byte {
		VER_IRQ = 1,
		HOR_IRQ = 2,
		CMD_IRQ = 4
	};

	/** I/O Ports
	  */
	enum PortId {
		VRAM_DATA = 0,
		PALETTE_DATA,
		COMMAND_DATA,
		REGISTER_DATA,
		REGISTER_SELECT,
		STATUS,
		INTERRUPT_FLAG,
		SYSTEM_CONTROL,
		KANJI_ROM_0,
		KANJI_ROM_1,
		KANJI_ROM_2,
		KANJI_ROM_3,
		RESERVED_0,
		RESERVED_1,
		RESERVED_2,
		RESERVED_3
	};

	/** Registers
	  */
	enum RegisterId {
		VRAM_WRITE_ADDRESS_0 = 0,
		VRAM_WRITE_ADDRESS_1,
		VRAM_WRITE_ADDRESS_2,
		VRAM_READ_ADDRESS_0,
		VRAM_READ_ADDRESS_1,
		VRAM_READ_ADDRESS_2,
		SCREEN_MODE_0,
		SCREEN_MODE_1,
		CONTROL,
		INTERRUPT_0,
		INTERRUPT_1,
		INTERRUPT_2,
		INTERRUPT_3,
		PALETTE_CONTROL,
		PALETTE_POINTER,
		BACK_DROP_COLOR,
		DISPLAY_ADJUST,
		SCROLL_CONTROL_AY0,
		SCROLL_CONTROL_AY1,
		SCROLL_CONTROL_AX0,
		SCROLL_CONTROL_AX1,
		SCROLL_CONTROL_BY0,
		SCROLL_CONTROL_BY1,
		SCROLL_CONTROL_BX0,
		SCROLL_CONTROL_BX1,
		SPRITE_PATTERN_ADDRESS,
		LCD_CONTROL,
		PRIORITY_CONTROL,
		SPRITE_PALETTE_CONTROL,
		CMD_PARAM_SRC_ADDRESS_0 = 32,
		CMD_PARAM_SRC_ADDRESS_1,
		CMD_PARAM_SRC_ADDRESS_2,
		CMD_PARAM_SRC_ADDRESS_3,
		CMD_PARAM_DEST_ADDRESS_0,
		CMD_PARAM_DEST_ADDRESS_1,
		CMD_PARAM_DEST_ADDRESS_2,
		CMD_PARAM_DEST_ADDRESS_3,
		CMD_PARAM_SIZE_0,
		CMD_PARAM_SIZE_1,
		CMD_PARAM_SIZE_2,
		CMD_PARAM_SIZE_3,
		CMD_PARAM_ARGUMENT,
		CMD_PARAM_LOGOP,
		CMD_PARAM_WRITE_MASK_0,
		CMD_PARAM_WRITE_MASK_1,
		CMD_PARAM_FONT_COLOR_FC0,
		CMD_PARAM_FONT_COLOR_FC1,
		CMD_PARAM_FONT_COLOR_BC0,
		CMD_PARAM_FONT_COLOR_BC1,
		CMD_PARAM_OPCODE,
		CMD_PARAM_BORDER_X_0,
		CMD_PARAM_BORDER_X_1
	};

	// --- members ----------------------------------------------------

	struct RegDebug final : SimpleDebuggable {
		explicit RegDebug(V9990& v9990);
		[[nodiscard]] byte read(unsigned address) override;
		void write(unsigned address, byte value, EmuTime::param time) override;
	} v9990RegDebug;

	struct PalDebug final : SimpleDebuggable {
		explicit PalDebug(V9990& v9990);
		[[nodiscard]] byte read(unsigned address) override;
		void write(unsigned address, byte value, EmuTime::param time) override;
	} v9990PalDebug;

	IRQHelper irq;

	Display& display;

	TclCallback   invalidRegisterReadCallback;
	TclCallback   invalidRegisterWriteCallback;

	/** VRAM
	  */
	V9990VRAM vram;
	unsigned vramReadPtr, vramWritePtr;
	byte vramReadBuffer;

	/** Command Engine
	  */
	V9990CmdEngine cmdEngine;

	/** Renderer
	  */
	std::unique_ptr<V9990Renderer> renderer;

	/** Emulation time when this frame was started (VSYNC)
	  */
	Clock<V9990DisplayTiming::UC_TICKS_PER_SECOND> frameStartTime;

	/** Time of the last set HSCAN sync point
	  */
	EmuTime hScanSyncTime;

	/** Display timings
	  */
	const V9990DisplayPeriod* horTiming;
	const V9990DisplayPeriod* verTiming;

	/** Store display mode because it's queried a lot
	  */
	V9990DisplayMode mode;

	/** Palette
	  */
	std::array<byte, 0x100> palette;

	/** Status port (P#5)
	  */
	byte status;

	/** Interrupt flag (P#6)
	  */
	byte pendingIRQs = 0;

	/** Registers
	  */
	std::array<byte, 0x40> regs = {}; // fill with zero
	byte regSelect;

	/** Is PAL timing active?  False means NTSC timing
	  */
	bool palTiming{false};

	/** Is interlace active?
	  * @see isInterlaced.
	  */
	bool interlaced{false};

	/** Is the current scan position inside the display area?
	  */
	bool isDisplayArea{false};

	/** Is display enabled. Note that this is not always the same as bit 7
	  * of the CONTROL register because the display enable status change
	  * only takes place at the start of the next frame.
	  * Note: on V99x8, display enable takes effect the next line
	  */
	bool displayEnabled{false};

	/** Changing high byte of vertical scroll registers only takes effect
	  * at the start of the next page. These members hold the current
	  * value of the scroll value.
	  * note: writing the low byte has effect immediately (or at the next
	  *       line, TODO investigate this). But the effect is not the same
	  *       as on V99x8, see V9990PixelRenderer::updateScrollAYLow() for
	  *       details.
	  */
	byte scrollAYHigh;
	byte scrollBYHigh;

	/** Corresponds to bit 1 in the System Control Port.
	  * When this is true, all registers are held in the 'power ON reset'
	  * state, so writes to registers are ignored.
	  */
	bool systemReset;

	/** Is there an external video source available to superimpose on?
	  * In 32bpp we could just always output an alpha channel. But in
	  * 16bpp we only want to output the special color-key when later
	  * it will be replaced by a pixel from the external video source. */
	bool externalVideoSource{false};

	/** Combination of 'externalVideoSource' and R#8 bit 5 (YSE). This
	  * variable only changes once per frame, so we can't directly
	  * calculate it from the two former states. */
	bool superimposing{false};

	// --- methods ----------------------------------------------------

	void setHorizontalTiming();
	void setVerticalTiming();

	[[nodiscard]] V9990ColorMode getColorMode(byte pal_ctrl) const;

	/** Get VRAM read or write address from V9990 registers
	  * @param base  VRAM_READ_ADDRESS_0 or VRAM_WRITE_ADDRESS_0
	  * @returns     VRAM read or write address
	  */
	[[nodiscard]] inline unsigned getVRAMAddr(RegisterId base) const;

	/** set VRAM read or write address into V9990 registers
	  * @param base  VRAM_READ_ADDRESS_0 or VRAM_WRITE_ADDRESS_0
	  * @param addr  Address to set
	  */
	inline void setVRAMAddr(RegisterId base, unsigned addr);

	/** Read V9990 register value
	  * @param reg   Register to read from
	  * @param time  Moment in emulated time to read register
	  * @returns     Register value
	  */
	[[nodiscard]] byte readRegister(byte reg, EmuTime::param time) const;

	/** Write V9990 register value
	  * @param reg   Register to write to
	  * @param val   Value to write
	  * @param time  Moment in emulated time to write register
	  */
	void writeRegister(byte reg, byte val, EmuTime::param time);

	/** Write V9990 palette register
	  * @param reg   Register to write to
	  * @param val   Value to write
	  * @param time  Moment in emulated time to write register
	  */
	void writePaletteRegister(byte reg, byte val, EmuTime::param time);

	/** Schedule a sync point at the start of the next line
	 */
	void syncAtNextLine(SyncBase& type, EmuTime::param time);

	/** Create a new renderer.
	  * @param time  Moment in emulated time to create the renderer
	  */
	void createRenderer(EmuTime::param time);

	/** Start a new frame.
	  * @param time  Moment in emulated time to start the frame
	  */
	void frameStart(EmuTime::param time);

	/** Raise an IRQ
	  * @param irqType  Type of IRQ
	  */
	void raiseIRQ(IRQType irqType);

	/** Precalculate the display mode
	  */
	[[nodiscard]] V9990DisplayMode calcDisplayMode() const;
	void setDisplayMode(V9990DisplayMode newMode);

	/** Calculate the moment in time the next line interrupt will occur
	  * @param time The current time
	  * @result Timestamp for next hor irq
	  */
	void scheduleHscan(EmuTime::param time);

	/** Estimate when the current (if any) command will finish.
	 */
	void scheduleCmdEnd(EmuTime::param time);
};
SERIALIZE_CLASS_VERSION(V9990, 5);

} // namespace openmsx

#endif
