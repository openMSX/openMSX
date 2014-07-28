#ifndef VDP_HH
#define VDP_HH

#include "MSXDevice.hh"
#include "Schedulable.hh"
#include "VideoSystemChangeListener.hh"
#include "IRQHelper.hh"
#include "Clock.hh"
#include "DisplayMode.hh"
#include "Observer.hh"
#include "openmsx.hh"
#include <memory>

namespace openmsx {

class PostProcessor;
class Renderer;
class VDPCmdEngine;
class VDPVRAM;
class SpriteChecker;
class VDPRegDebug;
class VDPStatusRegDebug;
class VDPPaletteDebug;
class VRAMPointerDebug;
class FrameCountInfo;
class CycleInFrameInfo;
class LineInFrameInfo;
class CycleInLineInfo;
class MsxYPosInfo;
class MsxX256PosInfo;
class MsxX512PosInfo;
class Display;
class RawFrame;
class Setting;
template<typename> class EnumSetting;
namespace VDPAccessSlots {
	enum Delta : int;
	class Calculator;
}

/** Unified implementation of MSX Video Display Processors (VDPs).
  * MSX1 VDP is Texas Instruments TMS9918A or TMS9928A.
  * MSX2 VDP is Yamaha V9938.
  * MSX2+ and turbo R VDP is Yamaha V9958.
  *
  * Current implementation is written by Maarten ter Huurne,
  * with some contributions from other openMSX developers.
  * The first implementation was based on Sean Young's code from MESS,
  * which was in turn based on code by Mike Balfour.
  * Integration of MESS code into openMSX was done by David Heremans.
  *
  * All undocumented features as described in the following file
  * should be emulated:
  * http://www.msxnet.org/tech/tms9918a.txt
  *
  * This class is the VDP core, it uses a separate Renderer to
  * convert the VRAM contents into pixels on the host machine.
  * Communicates with the Renderer through a pull variant of the
  * Observer pattern: a VDP object fires events when its state changes,
  * the Renderer can retrieve the new state if necessary by calling
  * methods on the VDP object.
  *
  * A note about timing: the start of a frame or line is defined as
  * the starting time of the corresponding sync (vsync, hsync).
  */
class VDP : public MSXDevice, public Schedulable,
            private VideoSystemChangeListener, Observer<Setting>
{
public:
	/** Number of VDP clock ticks per second.
	  */
	static const int TICKS_PER_SECOND = 3579545 * 6; // 21.5MHz;
	typedef Clock<TICKS_PER_SECOND> VDPClock;

	/** Number of VDP clock ticks per line.
	  */
	static const int TICKS_PER_LINE = 1368;

	explicit VDP(const DeviceConfig& config);
	virtual ~VDP();

	virtual void powerUp(EmuTime::param time);
	virtual void reset(EmuTime::param time);
	virtual byte readIO(word port, EmuTime::param time);
	virtual byte peekIO(word port, EmuTime::param time) const;
	virtual void writeIO(word port, byte value, EmuTime::param time);
	virtual void executeUntil(EmuTime::param time, int userData);

	/** Used by Video9000 to be able to couple the VDP and V9990 output.
	 * Can return nullptr in case of renderer=none. This value can change
	 * over the lifetime of the VDP object (on renderer switch).
	 */
	PostProcessor* getPostProcessor() const;

	/** Is this an MSX1 VDP?
	  * @return True if this is an MSX1 VDP
	  *   False otherwise.
	  */
	inline bool isMSX1VDP() const {
		return version & VM_MSX1;
	}

	/** Is this a MSX1 VDP with PAL?
	  * @return True iff this is an MSX1 VDP with PAL
	  */
	inline bool isMSX1VDPwithPAL() const {
		return version & (VM_MSX1 | VM_PAL);
	}

	/** Is this a VDP that lacks mirroring?
	  * @return True iff this VDP lacks the screen 2 mirrored mode
	  */
	inline bool vdpLacksMirroring() const {
		return version & VM_NO_MIRRORING;
	}

	/** Is this a VDP that has pattern/colortable mirroring?
	 * @return True iff this VDP has pattern/colortable mirroring
	 */
	inline bool vdpHasPatColMirroring() const {
		return version & VM_PALCOL_MIRRORING;
	}

	/** Does this MSX1 VDP have VRAM remapping when switching from 4k to 8/16k mode?
	  * @return True iff this is an MSX1 VDP with VRAM remapping
	  */
	inline bool isMSX1VDPwithVRAMremapping() const {
		return version & (VM_MSX1 | VM_VRAM_REMAPPING);
	}

	/** Is this a VDP with a Toshiba palette?
	  * @return True iff this VDP has the Toshiba palette
	  */
	inline bool hasToshibaPalette() const {
		return version & VM_TOSHIBA_PALETTE;
	}

	/** Does this VDP support YJK display?
	  * @return True for V9958, false otherwise.
	  */
	inline bool hasYJK() const {
		return version & VM_YJK;
	}

	/** Get the display mode the VDP is in.
	  * @return The current display mode.
	  */
	inline DisplayMode getDisplayMode() const {
		return displayMode;
	}

	/** Get the VRAM object for this VDP.
	  */
	inline VDPVRAM& getVRAM() {
		return *vram;
	}

	/** Are we currently superimposing?
	 * In case of superimpose, returns a pointer to the to-be-superimposed
	 * frame. Returns nullptr if superimpose is not active.
	 */
	inline const RawFrame* isSuperimposing() const {
		// Note that bit 0 of r#0 has no effect on an V9938 or higher,
		// but this bit is masked out. Also note that on an MSX1, if
		// bit 0 of r#0 is enabled and there is no external video
		// source, then we lose sync.
		// Also note that because this property is fixed per frame we
		// cannot (re)calculate it from register values.
		return superimposing;
	}

	/** Get the sprite checker for this VDP.
	  */
	inline SpriteChecker& getSpriteChecker() {
		return *spriteChecker;
	}

	/** Gets the current transparency setting.
	  * @return True iff color 0 is transparent.
	  */
	inline bool getTransparency() const {
		return (controlRegs[8] & 0x20) == 0;
	}

	/** Gets the current foreground color.
	  * @return Color index [0..15].
	  */
	inline int getForegroundColor() const {
		return controlRegs[7] >> 4;
	}

	/** Gets the current background color.
	  * @return Color index.
	  *   In Graphic5 mode, the range is [0..15];
	  *   bits 3-2 contains the color for even pixels,
	  *   bits 1-0 contains the color for odd pixels.
	  *   In Graphic7 mode with YJK off, the range is [0..255].
	  *   In other modes, the range is [0..15].
	  */
	inline int getBackgroundColor() const {
		byte reg7 = controlRegs[7];
		if (displayMode.getByte() == DisplayMode::GRAPHIC7) {
			return reg7;
		} else {
			return reg7 & 0x0F;
		}
	}

	/** Gets the current blinking color for blinking text.
	  * @return Color index [0..15].
	  */
	inline int getBlinkForegroundColor() const {
		return controlRegs[12] >> 4;
	}

	/** Gets the current blinking color for blinking text.
	  * @return Color index [0..15].
	  */
	inline int getBlinkBackgroundColor() const {
		return controlRegs[12] & 0x0F;
	}

	/** Gets the current blink state.
	  * @return True iff alternate colors / page should be displayed.
	  */
	inline bool getBlinkState() const {
		return blinkState;
	}

	/** Gets a palette entry.
	  * @param index The index [0..15] in the palette.
	  * @return Color value in the format of the palette registers:
	  *   bit 10..8 is green, bit 6..4 is red and bit 2..0 is blue.
	  */
	inline word getPalette(int index) const {
		return palette[index];
	}

	/** Is the display enabled?
	  * Both the regular border and forced blanking by clearing
	  * the display enable bit are considered disabled display.
	  * @return true iff enabled.
	  */
	inline bool isDisplayEnabled() const {
		return isDisplayArea && displayEnabled;
	}

	/** Are sprites enabled?
	  * @return True iff blanking is off, the current mode supports
	  *   sprites and sprites are not disabled.
	  */
	inline bool spritesEnabled() const {
		return displayEnabled &&
		       (displayMode.getSpriteMode(isMSX1VDP()) != 0) &&
		       ((controlRegs[8] & 0x02) == 0x00);
	}

	/** Same as spritesEnabled(), but may only be called in sprite
	  * mode 1 or 2. Is a tiny bit faster.
	  */
	inline bool spritesEnabledFast() const {
		assert(displayMode.getSpriteMode(isMSX1VDP()) != 0);
		return displayEnabled && ((controlRegs[8] & 0x02) == 0x00);
	}

	/** Gets the current vertical scroll (line displayed at Y=0).
	  * @return Vertical scroll register value.
	  */
	inline byte getVerticalScroll() const {
		return controlRegs[23];
	}

	/** Gets the current horizontal scroll lower bits.
	  * Rather than actually scrolling the screen, this setting shifts the
	  * screen 0..7 bytes to the right.
	  * @return Horizontal scroll low register value.
	  */
	inline byte getHorizontalScrollLow() const {
		return controlRegs[27];
	}

	/** Gets the current horizontal scroll higher bits.
	  * This value determines how many 8-pixel steps the background is
	  * rotated to the left.
	  * @return Horizontal scroll high register value.
	  */
	inline byte getHorizontalScrollHigh() const {
		return controlRegs[26];
	}

	/** Gets the current border mask setting.
	  * Border mask extends the left border by 8 pixels if enabled.
	  * This is a V9958 feature, on older VDPs it always returns false.
	  * @return true iff enabled.
	  */
	inline bool isBorderMasked() const {
		return (controlRegs[25] & 0x02) != 0;
	}

	/** Is multi page scrolling enabled?
	  * It is considered enabled if both the multi page scrolling flag is
	  * enabled and the current page is odd. Scrolling wraps into the
	  * lower even page.
	  * @return true iff enabled.
	  */
	inline bool isMultiPageScrolling() const {
		return (controlRegs[25] & 0x01) && (controlRegs[2] & 0x20);
	}

	/** Get the absolute line number of display line zero.
	  * Usually this is equal to the height of the top border,
	  * but not so during overscan.
	  */
	inline int getLineZero() const {
		return displayStart / TICKS_PER_LINE;
	}

	/** Is PAL timing active?
	  * This setting is fixed at start of frame.
	  * @return True if PAL timing, false if NTSC timing.
	  */
	inline bool isPalTiming() const {
		return palTiming;
	}

	/** Get interlace status.
	  * Interlace means the odd fields are displayed half a line lower
	  * than the even fields. Together with even/odd page alternation
	  * this can be used to create an interlaced display.
	  * This setting is fixed at start of frame.
	  * @return True iff interlace is enabled.
	  */
	inline bool isInterlaced() const {
		return interlaced;
	}

	/** Get even/odd page alternation status.
	  * Interlace means the odd fields are displayed half a line lower
	  * than the even fields. Together with even/odd page alternation
	  * this can be used to create an interlaced display.
	  * This setting is NOT fixed at start of frame.
	  * TODO: Find out how real VDP does it.
	  *       If it fixes it at start of frame, so should we.
	  *       If it handles it dynamically (my guess), then an update method
	  *       should be added on the Renderer interface.
	  * @return True iff even/odd page alternation is enabled.
	  */
	inline bool isEvenOddEnabled() const {
		return (controlRegs[9] & 4) != 0;
	}

	/** Is the even or odd field being displayed?
	  * @return True iff this field should be displayed half a line lower.
	  */
	inline bool getEvenOdd() const {
		return (statusReg2 & 2) != 0;
	}

	/** Expresses the state of even/odd page interchange in a mask
	  * on the line number. If even/odd interchange is active, for some
	  * frames lines 256..511 (page 1) are replaced by 0..255 (page 0)
	  * and 768..1023 (page 3, if appicable) by 512..767 (page 2).
	  * Together with the interlace setting this can be used to create
	  * an interlaced display.
	  * Even/odd interchange can also happen because of the 'blink'
	  * feature in bitmap modes.
	  * @return Line number mask that expressed even/odd state.
	  */
	inline int getEvenOddMask() const {
		// TODO: Verify which page is displayed on even fields.
		return (((~controlRegs[9] & 4) << 6) | ((statusReg2 & 2) << 7)) &
		       (!blinkState << 8);
	}

	/** Gets the number of VDP clock ticks (21MHz) elapsed between
	  * a given time and the start of this frame.
	  */
	inline int getTicksThisFrame(EmuTime::param time) const {
		return frameStartTime.getTicksTill_fast(time);
	}

	inline EmuTime::param getFrameStartTime() const {
		return frameStartTime.getTime();
	}

	/** Gets the sprite size in pixels (8/16).
	  */
	inline int getSpriteSize() const {
		return ((controlRegs[1] & 2) << 2) + 8;
	}

	/** Are sprites magnified?
	  */
	inline bool isSpriteMag() const {
		return controlRegs[1] & 1;
	}

	/** Are commands possible in non Graphic modes? (V9958 only)
	  * @return True iff CMD bit set.
	  */
	inline bool getCmdBit() const {
		return (controlRegs[25] & 0x40) != 0;
	}

	/** Gets the number of VDP clockticks (21MHz) per frame.
	  */
	inline int getTicksPerFrame() const {
		return palTiming ? TICKS_PER_LINE * 313 : TICKS_PER_LINE * 262;
	}

	/** Is the given timestamp inside the current frame?
	  * Mainly useful for debugging, because relevant timestamps should
	  * always be inside the current frame.
	  * The end time of a frame is still considered inside,
	  * to support the case when the given timestamp is exclusive itself:
	  * a typically "limit" variable.
	  * @param time Timestamp to check.
	  * @return True iff the timestamp is inside the current frame.
	  */
	inline bool isInsideFrame(EmuTime::param time) const {
		return time >= frameStartTime.getTime() &&
			getTicksThisFrame(time) <= getTicksPerFrame();
	}

	/** This is a combination of the (horizontal) set adjust register and
	  * the YJK-mode bit.
	  */
	inline int getHorizontalAdjust() const {
		return horizontalAdjust;
	}

	/** Gets the number of VDP clockticks between start of line and the start
	  * of the sprite plane.
	  * The location of the sprite plane is not influenced by horizontal scroll
	  * or border mask.
	  * TODO: Leave out the text mode case, since there are no sprites
	  *       in text mode?
	  */
	inline int getLeftSprites() const {
		return 100 + 102 + 56
			+ (horizontalAdjust - 7) * 4
			+ (displayMode.isTextMode() ? 36 : 0);
	}

	/** Gets the number of VDP clockticks between start of line and the end
	  * of the left border.
	  * Does not include extra pixels of horizontal scroll low, since those
	  * are not actually border pixels (sprites appear in front of them).
	  */
	inline int getLeftBorder() const {
		return getLeftSprites() + (isBorderMasked() ? 8 * 4 : 0);
	}

	/** Gets the number of VDP clockticks between start of line and the start
	  * of the right border.
	  */
	inline int getRightBorder() const {
		return getLeftSprites()
			+ (displayMode.isTextMode() ? 960 : 1024);
	}

	/** Gets the number of VDP clockticks between start of line and the time
	  * when the background pixel with X coordinate 0 would be drawn.
	  * This includes extra pixels of horizontal scroll low,
	  * but disregards border mask.
	  */
	inline int getLeftBackground() const {
		return getLeftSprites() + getHorizontalScrollLow() * 4;
	}

	/** Should only be used by SpriteChecker. Returns the current value
	  * of status register 0 (both the F-flag and the sprite related bits).
	  */
	byte getStatusReg0() const { return statusReg0; }

	/** Should only be used by SpriteChecker. Change the sprite related
	  * bits of status register 0 (leaves the F-flag unchanged).
	  * Bit 6 (5S) is set when more than 4 (sprite mode 1) or 8 (sprite
	  *   mode 2) sprites occur on the same line.
	  * Bit 5 (C) is set when sprites collide.
	  * Bit 4..0 (5th sprite number) contains the number of the first
	  *   sprite to exceed the limit per line.
	  */
	void setSpriteStatus(byte value)
	{
		statusReg0 = (statusReg0 & 0x80) | (value & 0x7F);
	}

	/** Returns current VR mode.
	  * false -> VR=0,  true -> VR=1
	  */
	bool getVRMode() const {
		return (controlRegs[8] & 8) != 0;
	}

	/** Enable superimposing
	  */
	void setExternalVideoSource(const RawFrame* externalSource);

	/** Get the earliest access slot that is at least 'delta' cycles in
	  * the future. */
	EmuTime getAccessSlot(EmuTime::param time, VDPAccessSlots::Delta delta) const;

	/** Same as getAccessSlot(), but it can be _much_ faster for repeated
	  * calls, e.g. in the implementation of VDP commands. However it does
	  * have some limitations:
	  * - The returned calculator only remains valid for as long as
	  *   the VDP access timing remains the same (display/sprite enable).
	  * - The calculator needs to see _all_ time changes.
	  * (So this means that in every VDPCmd::execute() method you need
	  * to construct a new calculator).
	  */
	VDPAccessSlots::Calculator getAccessSlotCalculator(
		EmuTime::param time, EmuTime::param limit) const;

	/** Is there a CPU-VRAM access scheduled. */
	bool cpuAccessScheduled() const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void initTables();

	// VdpVersion bitmasks
	static const unsigned VM_MSX1             =  1; // set-> MSX1,       unset-> MSX2 or MSX2+
	static const unsigned VM_PAL              =  2; // set-> fixed PAL,  unset-> fixed NTSC or switchable
	static const unsigned VM_NO_MIRRORING     =  4; // set-> no (screen2) mirroring
	static const unsigned VM_PALCOL_MIRRORING =  8; // set-> pattern/color-table mirroring
	static const unsigned VM_VRAM_REMAPPING   = 16; // set-> 4k,8/16k VRAM remapping
	static const unsigned VM_TOSHIBA_PALETTE  = 32; // set-> has Toshiba palette
	static const unsigned VM_YJK              = 64; // set-> has YJK (MSX2+)

	/** VDP version: the VDP model being emulated. */
	enum VdpVersion {
		/** MSX1 VDP, NTSC version.
		  * TMS9918A has NTSC encoding built in,
		  * while TMS9928A has color difference output;
		  * in emulation there is no difference. */
		TMS99X8A   = VM_MSX1 | VM_PALCOL_MIRRORING | VM_VRAM_REMAPPING,

		/** MSX1 VDP, PAL version. */
		TMS9929A   = VM_MSX1 | VM_PALCOL_MIRRORING | VM_VRAM_REMAPPING | VM_PAL,

		/** newer variant PAL. */
		TMS9129    = VM_MSX1 | VM_PAL,

		/** newer variant NTSC. */
		TMS91X8    = VM_MSX1,

		/** Toshiba clone (hardwired as PAL). */
		T6950PAL   = VM_MSX1 | VM_TOSHIBA_PALETTE | VM_NO_MIRRORING | VM_PAL,

		/** Toshiba clone (hardwired as NTSC). */
		T6950NTSC  = VM_MSX1 | VM_TOSHIBA_PALETTE | VM_NO_MIRRORING,

		/** VDP in Toshiba T7937A engine (hardwired as PAL). */
		T7937APAL  = VM_MSX1 | VM_TOSHIBA_PALETTE | VM_PAL,

		/** VDP in Toshiba T7937A engine (hardwired as NTSC). */
		T7937ANTSC = VM_MSX1 | VM_TOSHIBA_PALETTE,

		/** MSX2 VDP. */
		V9938      = 0,

		/** MSX2+ and turbo R VDP. */
		V9958      = VM_YJK,
	};

	/** Types of VDP sync points that can be scheduled.
	  */
	enum SyncType {
		/** Vertical sync: the transition from one frame to the next. */
		VSYNC,
		/** Start of display. */
		DISPLAY_START,
		/** Vertical scanning: end of display. */
		VSCAN,
		/** Horizontal scanning: line interrupt. */
		HSCAN,
		/** Horizontal adjust change. */
		HOR_ADJUST,
		/** Change mode. */
		SET_MODE,
		/** Enable/disable screen. */
		SET_BLANK,
		/** CPU read/write VRAM access. */
		CPU_VRAM_ACCESS,
	};

	/** Time at which the internal VDP display line counter is reset,
	  * expressed in ticks after vsync.
	  * I would expect the counter to reset at line 16, but measurements
	  * on NMS8250 show it is one line earlier. I'm not sure whether the
	  * actual counter reset happens on line 15 or whether the VDP
	  * timing may be one line off for some reason.
	  * TODO: This is just an assumption, more measurements on real MSX
	  *       are necessary to verify there is really such a thing and
	  *       if so, that the value is accurate.
	  */
	static const int LINE_COUNT_RESET_TICKS = 15 * TICKS_PER_LINE;

	/** Gets the number of display lines per screen.
	  * @return 192 or 212.
	  */
	inline int getNumberOfLines() const {
		return controlRegs[9] & 0x80 ? 212 : 192;
	}

	/** Gets the value of the horizontal retrace status bit.
	  * Note that HR flipping continues at all times, not just during
	  * vertical display range.
	  * @param ticksThisFrame The screen position (in VDP ticks)
	  *    to return HR for.
	  * @return True iff the VDP scanning is inside the left/right
	  *   border or left/right erase or horizontal sync.
	  *   False iff the VDP scanning is in the display range.
	  */
	inline bool getHR(int ticksThisFrame) const {
		// Note: These constants are located inside this function because
		//       GCC 4.0.x won't link if they are in the class scope.
		/** Length of horizontal blank (HR=1) in text mode, measured in VDP
		  * ticks.
		  */
		static const int HBLANK_LEN_TXT = 404;
		/** Length of horizontal blank (HR=1) in graphics mode, measured in VDP
		  * ticks.
		  */
		static const int HBLANK_LEN_GFX = 312;
		return
			( ticksThisFrame + TICKS_PER_LINE - getRightBorder()
				) % TICKS_PER_LINE
			< (displayMode.isTextMode() ? HBLANK_LEN_TXT : HBLANK_LEN_GFX);
	}

	// VideoSystemChangeListener interface:
	virtual void preVideoSystemChange();
	virtual void postVideoSystemChange();

	/** Called both on init and on reset.
	  * Puts VDP into reset state.
	  * Does not call any renderer methods.
	  */
	void resetInit();

	/** Companion to resetInit: in resetInit the registers are reset,
	  * in this method the new base masks are distributed to the VDP
	  * subsystems.
	  */
	void resetMasks(EmuTime::param time);

	/** Start a new frame.
	  * @param time The moment in emulated time the frame starts.
	  */
	void frameStart(EmuTime::param time);

	/** Schedules a DISPLAY_START sync point.
	  * Also removes a pending DISPLAY_START sync, if any.
	  * Since HSCAN and VSCAN are relative to display start,
	  * their schedule methods are called by this method.
	  * @param time The moment in emulated time this call takes place.
	  *   Note: time is not the DISPLAY_START sync time!
	  */
	void scheduleDisplayStart(EmuTime::param time);

	/** Schedules a VSCAN sync point.
	  * Also removes a pending VSCAN sync, if any.
	  * @param time The moment in emulated time this call takes place.
	  *   Note: time is not the VSCAN sync time!
	  */
	void scheduleVScan(EmuTime::param time);

	/** Schedules a HSCAN sync point.
	  * Also removes a pending HSCAN sync, if any.
	  * @param time The moment in emulated time this call takes place.
	  *   Note: time is not the HSCAN sync time!
	  */
	void scheduleHScan(EmuTime::param time);

	/** Byte is written to VRAM by the CPU.
	  */
	void vramWrite(byte value, EmuTime::param time);

	/** Byte is read from VRAM by the CPU.
	  */
	byte vramRead(EmuTime::param time);

	/** Helper methods for CPU-VRAM access. */
	void scheduleCpuVramAccess(bool isRead, byte write, EmuTime::param time);
	void executeCpuVramAccess(EmuTime::param time);

	/** Read the contents of a status register
	  */
	byte peekStatusReg(byte reg, EmuTime::param time) const;
	byte readStatusReg(byte reg, EmuTime::param time);

	/** VDP control register has changed, work out the consequences.
	  */
	void changeRegister(byte reg, byte val, EmuTime::param time);

	/** Schedule a sync point at the start of the next line.
	  */
	void syncAtNextLine(SyncType type, EmuTime::param time);

	/** Create a new renderer.
	  */
	void createRenderer();

	/** Name base mask has changed.
	  * Inform the renderer and the VRAM.
	  */
	void updateNameBase(EmuTime::param time);

	/** Color base mask has changed.
	  * Inform the renderer and the VRAM.
	  */
	void updateColorBase(EmuTime::param time);

	/** Pattern base mask has changed.
	  * Inform the renderer and the VRAM.
	  */
	void updatePatternBase(EmuTime::param time);

	/** Sprite attribute base mask has changed.
	  * Inform the SpriteChecker and the VRAM.
	  */
	void updateSpriteAttributeBase(EmuTime::param time);

	/** Sprite pattern base mask has changed.
	  * Inform the SpriteChecker and the VRAM.
	  */
	void updateSpritePatternBase(EmuTime::param time);

	/** Display mode has changed.
	  * Update displayMode's value and inform the Renderer.
	  */
	void updateDisplayMode(DisplayMode newMode, EmuTime::param time);

	/** Sets a palette entry.
	  * @param index The index [0..15] in the palette.
	  * @param grb value in the format of the palette registers:
	  *   bit 10..8 is green, bit 6..4 is red and bit 2..0 is blue.
	  * @param time Moment in time palette change occurs.
	  */
	void setPalette(int index, word grb, EmuTime::param time);

	// Observer<Setting>
	virtual void update(const Setting& setting);

private:
	Display& display;
	EnumSetting<bool>& cmdTiming;
	EnumSetting<bool>& tooFastAccess;

	friend class VDPRegDebug;
	friend class VDPStatusRegDebug;
	friend class VDPPaletteDebug;
	friend class VRAMPointerDebug;
	friend class FrameCountInfo;
	const std::unique_ptr<VDPRegDebug>       vdpRegDebug;
	const std::unique_ptr<VDPStatusRegDebug> vdpStatusRegDebug;
	const std::unique_ptr<VDPPaletteDebug>   vdpPaletteDebug;
	const std::unique_ptr<VRAMPointerDebug>  vramPointerDebug;
	const std::unique_ptr<FrameCountInfo>    frameCountInfo;
	const std::unique_ptr<CycleInFrameInfo>  cycleInFrameInfo;
	const std::unique_ptr<LineInFrameInfo>   lineInFrameInfo;
	const std::unique_ptr<CycleInLineInfo>   cycleInLineInfo;
	const std::unique_ptr<MsxYPosInfo>       msxYPosInfo;
	const std::unique_ptr<MsxX256PosInfo>    msxX256PosInfo;
	const std::unique_ptr<MsxX512PosInfo>    msxX512PosInfo;

	/** Renderer that converts this VDP's state into an image.
	  */
	std::unique_ptr<Renderer> renderer;

	/** Command engine: the part of the V9938/58 that executes commands.
	  */
	std::unique_ptr<VDPCmdEngine> cmdEngine;

	/** Sprite checker: calculates sprite patterns and collisions.
	  */
	std::unique_ptr<SpriteChecker> spriteChecker;

	/** VRAM management object.
	  */
	std::unique_ptr<VDPVRAM> vram;

	/** Is there an external video source which we must superimpose
	  * upon?
	  */
	const RawFrame* externalVideo;

	/** Are we currently superimposing?
	 * This is a combination of the 'externalVideo' member (see above) and
	 * the superimpose-enable bit in VDP register R#0. This property only
	 * changes at most once per frame (at the beginning of the frame).
	 */
	const RawFrame* superimposing;

	/** The emulation time when this frame was started (vsync).
	  */
	VDPClock frameStartTime;

	/** Manages vertical scanning interrupt request.
	  */
	IRQHelper irqVertical;

	/** Manages horizontal scanning interrupt request.
	  */
	IRQHelper irqHorizontal;

	/** Time of last set DISPLAY_START sync point.
	  */
	EmuTime displayStartSyncTime;

	/** Time of last set VSCAN sync point.
	  */
	EmuTime vScanSyncTime;

	/** Time of last set HSCAN sync point.
	  */
	EmuTime hScanSyncTime;

	/** VDP version.
	  */
	VdpVersion version;

	/** The number of already fully rendered frames.
	  * Not used for actual emulation, only for 'frame_count' info topic.
	  * This value cannot be calculated from EmuTime because framerate is
	  * not constant (PAL/NTSC).
	  */
	int frameCount;

	/** VDP ticks between start of frame and start of display.
	  */
	int displayStart;

	/** VDP ticks between start of frame and the moment horizontal
	  * scan match occurs.
	  */
	int horizontalScanOffset;

	/** Horizontal display adjust.
	  * This value is update at the start of a line.
	  */
	int horizontalAdjust;

	/** Control registers.
	  */
	byte controlRegs[32];

	/** Mask on the control register index:
	  * makes MSX2 registers inaccessible on MSX1,
	  * instead the MSX1 registers are mirrored.
	  */
	int controlRegMask;

	/** Mask on the values of control registers.
	  * This saves a lot of masking when using the register values,
	  * because it is guaranteed non-existant bits are always zero.
	  * It also disables access to VDP features on a VDP model
	  * which does not support those features.
	  */
	byte controlValueMasks[32];

	/** Blinking count: number of frames until next state.
	  * If the ON or OFF period is 0, blinkCount is fixed to 0.
	  */
	int blinkCount;

	/** VRAM read/write access pointer.
	  * Contains the lower 14 bits of the current VRAM access address.
	  */
	int vramPointer;

	/** V9938 palette.
	  */
	word palette[16];

	/** Is the current scan position inside the display area?
	  */
	bool isDisplayArea;

	/** Is PAL timing active? False means NTSC timing.
	  * This value is updated at the start of every frame,
	  * to avoid problems with mid-screen PAL/NTSC switching.
	  * @see isPalTiming.
	  */
	bool palTiming;

	/** Is interlace active?
	  * @see isInterlaced.
	  */
	bool interlaced;

	/** Status register 0.
	  * Both the F flag (bit 7) and the sprite related bits (bits 6-0)
	  * are stored here.
	  */
	byte statusReg0;

	/** Status register 1.
	  * Bit 7 and 6 are always zero because light pen is not implemented.
	  * Bit 0 is always zero; its calculation depends on IE1.
	  * So all that remains is the version number.
	  */
	byte statusReg1;

	/** Status register 2.
	  * Bit 7, 4 and 0 of this field are always zero,
	  * their value can be retrieved from the command engine.
	  */
	byte statusReg2;

	/** Blinking state: should alternate color / page be displayed?
	  */
	bool blinkState;

	/** First byte written through port #99, #9A or #9B.
	  */
	byte dataLatch;

	/** Does the data latch have register data (port #99) stored?
	  */
	bool registerDataStored;

	/** Does the data latch have palette data (port #9A) stored?
	  */
	bool paletteDataStored;

	/** VRAM is read as soon as VRAM pointer changes.
	  * TODO: Is this actually what happens?
	  *   On TMS9928A the VRAM interface is the only access method.
	  *   But on V9938/58 there are other ways to access VRAM;
	  *   I wonder if they are consistent with this implementation.
	  * This also holds the soon-to-be-written data for CPU-VRAM writes.
	  */
	byte cpuVramData;

	/** CPU-VRAM requests are not executed immediately (though soon). This
	  * variable indicates whether the pending request is read or write.
	  */
	bool cpuVramReqIsRead;
	unsigned cpuVramReqAddr;

	/** Does CPU interface access main VRAM (false) or extended VRAM (true)?
	  * This is determined by MXC (R#45, bit 6).
	  */
	bool cpuExtendedVram;

	/** Current dispay mode. Note that this is not always the same as the
	  * display mode that can be obtained by combining the different mode
	  * bits because a mode change only takes place at the start of the
	  * next line.
	  */
	DisplayMode displayMode;

	/** Is display enabled. Note that this is not always the same as bit 6
	  * of R#1 because the display enable status change only takes place at
	  * the start of the next line.
	  */
	bool displayEnabled;

	/** Has a warning been printed.
	  * This is set when a warning about setting the dotclock direction
	  * is printed.  */
	bool warningPrinted;

	/** Cached version of cmdTiming/tooFastAccess setting. */
	bool brokenCmdTiming;
	bool allowTooFastAccess;
};
SERIALIZE_CLASS_VERSION(VDP, 6);

} // namespace openmsx

#endif
