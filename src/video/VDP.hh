#ifndef VDP_HH
#define VDP_HH

#include "MSXDevice.hh"
#include "Schedulable.hh"
#include "VideoSystemChangeListener.hh"
#include "SimpleDebuggable.hh"
#include "TclCallback.hh"
#include "InfoTopic.hh"
#include "IRQHelper.hh"
#include "Clock.hh"
#include "DisplayMode.hh"
#include "EnumSetting.hh"
#include "openmsx.hh"

#include "Observer.hh"
#include "gl_vec.hh"
#include "narrow.hh"
#include "outer.hh"

#include <memory>
#include <array>

namespace openmsx {

class PostProcessor;
class Renderer;
class VDPCmdEngine;
class VDPVRAM;
class MSXCPU;
class SpriteChecker;
class Display;
class RawFrame;
class Setting;
namespace VDPAccessSlots {
	enum class Delta : int;
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
class VDP final : public MSXDevice, private VideoSystemChangeListener
                , private Observer<Setting>
{
public:
	/** Number of VDP clock ticks per second.
	  */
	static constexpr int TICKS_PER_SECOND = 3579545 * 6; // 21.5MHz;
	using VDPClock = Clock<TICKS_PER_SECOND>;

	/** Number of VDP clock ticks per line.
	  */
	static constexpr int TICKS_PER_LINE = 1368;

	explicit VDP(const DeviceConfig& config);
	~VDP() override;

	void powerUp(EmuTime::param time) override;
	void reset(EmuTime::param time) override;
	[[nodiscard]] byte readIO(word port, EmuTime::param time) override;
	[[nodiscard]] byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;

	void getExtraDeviceInfo(TclObject& result) const override;
	[[nodiscard]] std::string_view getVersionString() const;

	[[nodiscard]] byte peekRegister(unsigned address) const;
	[[nodiscard]] byte peekStatusReg(byte reg, EmuTime::param time) const;

	/** VDP control register has changed, work out the consequences.
	  */
	void changeRegister(byte reg, byte val, EmuTime::param time);


	/** Used by Video9000 to be able to couple the VDP and V9990 output.
	 * Can return nullptr in case of renderer=none. This value can change
	 * over the lifetime of the VDP object (on renderer switch).
	 */
	[[nodiscard]] PostProcessor* getPostProcessor() const;

	/** Is this an MSX1 VDP?
	  * @return True if this is an MSX1 VDP
	  *   False otherwise.
	  */
	[[nodiscard]] inline bool isMSX1VDP() const {
		return (version & VM_MSX1) != 0;
	}

	/** Is this a VDP only capable of PAL?
	  * @return True iff this is a PAL only VDP
	  */
	[[nodiscard]] inline bool isVDPwithPALonly() const {
		return (version & VM_PAL) != 0;
	}

	/** Is this a VDP that lacks mirroring?
	  * @return True iff this VDP lacks the screen 2 mirrored mode
	  */
	[[nodiscard]] inline bool vdpLacksMirroring() const {
		return (version & VM_NO_MIRRORING) != 0;
	}

	/** Is this a VDP that has pattern/color table mirroring?
	 * @return True iff this VDP has pattern/color table mirroring
	 */
	[[nodiscard]] inline bool vdpHasPatColMirroring() const {
		return (version & VM_PALCOL_MIRRORING) != 0;
	}

	/** Does this VDP have VRAM remapping when switching from 4k to 8/16k mode?
	  * @return True iff this is a VDP with VRAM remapping
	  */
	[[nodiscard]] inline bool isVDPwithVRAMremapping() const {
		return (version & VM_VRAM_REMAPPING) != 0;
	}

	/** Does this VDP support YJK display?
	  * @return True for V9958, false otherwise.
	  */
	[[nodiscard]] inline bool hasYJK() const {
		return (version & VM_YJK) != 0;
	}

	/** Get the (fixed) palette for this MSX1 VDP.
	  * Don't use this if it's not an MSX1 VDP!
	  * @return an array of 16 RGB triplets
	  */
	[[nodiscard]] std::array<std::array<uint8_t, 3>, 16> getMSX1Palette() const;

	/** Get the display mode the VDP is in.
	  * @return The current display mode.
	  */
	[[nodiscard]] inline DisplayMode getDisplayMode() const {
		return displayMode;
	}

	/** Get the VRAM object for this VDP.
	  */
	[[nodiscard]] inline VDPVRAM& getVRAM() {
		return *vram;
	}

	/** Are we currently superimposing?
	 * In case of superimpose, returns a pointer to the to-be-superimposed
	 * frame. Returns nullptr if superimpose is not active.
	 */
	[[nodiscard]] inline const RawFrame* isSuperimposing() const {
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
	[[nodiscard]] inline SpriteChecker& getSpriteChecker() {
		return *spriteChecker;
	}

	/** Gets the current transparency setting.
	  * @return True iff color 0 is transparent.
	  */
	[[nodiscard]] inline bool getTransparency() const {
		return (controlRegs[8] & 0x20) == 0;
	}

	/** Can a sprite which has color=0 collide with some other sprite?
	 */
	[[nodiscard]] bool canSpriteColor0Collide() const {
		// On MSX1 (so far only tested a TMS9129(?)) sprites with
		// color=0 can always collide with other sprites. Though on
		// V99x8 (only tested V9958) collisions only occur when color 0
		// is not transparent. For more details see:
		//   https://github.com/openMSX/openMSX/issues/1198
		return isMSX1VDP() || !getTransparency();
	}

	/** Gets the current foreground color.
	  * @return Color index [0..15].
	  */
	[[nodiscard]] inline int getForegroundColor() const {
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
	[[nodiscard]] inline byte getBackgroundColor() const {
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
	[[nodiscard]] inline int getBlinkForegroundColor() const {
		return controlRegs[12] >> 4;
	}

	/** Gets the current blinking color for blinking text.
	  * @return Color index [0..15].
	  */
	[[nodiscard]] inline int getBlinkBackgroundColor() const {
		return controlRegs[12] & 0x0F;
	}

	/** Gets the current blink state.
	  * @return True iff alternate colors / page should be displayed.
	  */
	[[nodiscard]] inline bool getBlinkState() const {
		return blinkState;
	}

	/** Get address of pattern table (only for debugger) */
	[[nodiscard]] int getPatternTableBase() const {
		return controlRegs[4] << 11;
	}
	/** Get address of color table (only for debugger) */
	[[nodiscard]] int getColorTableBase() const {
		return (controlRegs[10] << 14) | (controlRegs[3] << 6);
	}
	/** Get address of name table (only for debugger) */
	[[nodiscard]] int getNameTableBase() const {
		return controlRegs[2] << 10;
	}
	/** Get address of pattern table (only for debugger) */
	[[nodiscard]] int getSpritePatternTableBase() const {
		return controlRegs[6] << 11;
	}
	/** Get address of color table (only for debugger) */
	[[nodiscard]] int getSpriteAttributeTableBase() const {
		return (controlRegs[11] << 15) | (controlRegs[5] << 7);
	}
	/** Get vram pointer (14-bit) (only for debugger) */
	[[nodiscard]] int getVramPointer() const {
		return vramPointer;
	}

	/** Gets a palette entry.
	  * @param index The index [0..15] in the palette.
	  * @return Color value in the format of the palette registers:
	  *   bit 10..8 is green, bit 6..4 is red and bit 2..0 is blue.
	  */
	[[nodiscard]] inline uint16_t getPalette(unsigned index) const {
		return palette[index];
	}
	[[nodiscard]] inline std::span<const uint16_t, 16> getPalette() const {
		return palette;
	}

	/** Sets a palette entry.
	  * @param index The index [0..15] in the palette.
	  * @param grb value in the format of the palette registers:
	  *   bit 10..8 is green, bit 6..4 is red and bit 2..0 is blue.
	  * @param time Moment in time palette change occurs.
	  */
	void setPalette(unsigned index, word grb, EmuTime::param time);

	/** Is the display enabled?
	  * Both the regular border and forced blanking by clearing
	  * the display enable bit are considered disabled display.
	  * @return true iff enabled.
	  */
	[[nodiscard]] inline bool isDisplayEnabled() const {
		return isDisplayArea && displayEnabled;
	}

	/** Are sprites enabled?
	  * @return True iff blanking is off, the current mode supports
	  *   sprites and sprites are not disabled.
	  */
	[[nodiscard]] inline bool spritesEnabled() const {
		return displayEnabled &&
		       (displayMode.getSpriteMode(isMSX1VDP()) != 0) &&
		       spriteEnabled;
	}

	/** Same as spritesEnabled(), but may only be called in sprite
	  * mode 1 or 2. Is a tiny bit faster.
	  */
	[[nodiscard]] inline bool spritesEnabledFast() const {
		assert(displayMode.getSpriteMode(isMSX1VDP()) != 0);
		return displayEnabled && spriteEnabled;
	}

	/** Still faster variant (just looks at the sprite-enabled-bit).
	  * But only valid in sprite mode 1/2 with screen enabled.
	  */
	[[nodiscard]] inline bool spritesEnabledRegister() const {
		return spriteEnabled;
	}

	/** Gets the current vertical scroll (line displayed at Y=0).
	  * @return Vertical scroll register value.
	  */
	[[nodiscard]] inline byte getVerticalScroll() const {
		return controlRegs[23];
	}

	/** Gets the current horizontal scroll lower bits.
	  * Rather than actually scrolling the screen, this setting shifts the
	  * screen 0..7 bytes to the right.
	  * @return Horizontal scroll low register value.
	  */
	[[nodiscard]] inline byte getHorizontalScrollLow() const {
		return controlRegs[27];
	}

	/** Gets the current horizontal scroll higher bits.
	  * This value determines how many 8-pixel steps the background is
	  * rotated to the left.
	  * @return Horizontal scroll high register value.
	  */
	[[nodiscard]] inline byte getHorizontalScrollHigh() const {
		return controlRegs[26];
	}

	/** Gets the current border mask setting.
	  * Border mask extends the left border by 8 pixels if enabled.
	  * This is a V9958 feature, on older VDPs it always returns false.
	  * @return true iff enabled.
	  */
	[[nodiscard]] inline bool isBorderMasked() const {
		return (controlRegs[25] & 0x02) != 0;
	}

	/** Is multi page scrolling enabled?
	  * It is considered enabled if both the multi page scrolling flag is
	  * enabled and the current page is odd. Scrolling wraps into the
	  * lower even page.
	  * @return true iff enabled.
	  */
	[[nodiscard]] inline bool isMultiPageScrolling() const {
		return (controlRegs[25] & 0x01) && (controlRegs[2] & 0x20);
	}

	/** Only used by debugger
	  * @return page 0-3
	  */
	[[nodiscard]] int getDisplayPage() const {
		return (controlRegs[2] >> 5) & 3;
	}

	/** Get the absolute line number of display line zero.
	  * Usually this is equal to the height of the top border,
	  * but not so during overscan.
	  */
	[[nodiscard]] inline int getLineZero() const {
		return displayStart / TICKS_PER_LINE;
	}

	/** Is PAL timing active?
	  * This setting is fixed at start of frame.
	  * @return True if PAL timing, false if NTSC timing.
	  */
	[[nodiscard]] inline bool isPalTiming() const {
		return palTiming;
	}

	/** Get interlace status.
	  * Interlace means the odd fields are displayed half a line lower
	  * than the even fields. Together with even/odd page alternation
	  * this can be used to create an interlaced display.
	  * This setting is fixed at start of frame.
	  * @return True iff interlace is enabled.
	  */
	[[nodiscard]] inline bool isInterlaced() const {
		return interlaced;
	}

	/** Get 'fast-blink' status.
	  * Normally blinking timing (alternating between pages) is based on
	  * frames. Though the V99x8 has an undocumented feature which changes
	  * this timing to lines. Sometimes this is called the "Cadari" feature
	  * after Luciano Cadari who discovered it.
	  *
	  * See ticket#1091: "Support for undocumented V99x8 register 1 bit 2"
	  *    https://github.com/openMSX/openMSX/issues/1091
	  * for test cases and links to more information.
	  */
	[[nodiscard]] inline bool isFastBlinkEnabled() const {
		return (controlRegs[1] & 4) != 0;
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
	[[nodiscard]] inline bool isEvenOddEnabled() const {
		if (isFastBlinkEnabled()) return false;
		return (controlRegs[9] & 4) != 0;
	}

	/** Is the even or odd field being displayed?
	  * @return True iff this field should be displayed half a line lower.
	  */
	[[nodiscard]] inline bool getEvenOdd() const {
		return (statusReg2 & 2) != 0;
	}

	/** Expresses the state of even/odd page interchange in a mask
	  * on the line number. If even/odd interchange is active, for some
	  * frames lines 256..511 (page 1) are replaced by 0..255 (page 0)
	  * and 768..1023 (page 3, if applicable) by 512..767 (page 2).
	  * Together with the interlace setting this can be used to create
	  * an interlaced display.
	  * Even/odd interchange can also happen because of the 'blink'
	  * feature in bitmap modes.
	  * @pre !isFastBlinkEnabled()
	  * @return Line number mask that expressed even/odd state.
	  */
	[[nodiscard]] inline unsigned getEvenOddMask() const {
		// TODO: Verify which page is displayed on even fields.
		assert(!isFastBlinkEnabled());
		return (((~controlRegs[9] & 4) << 6) | ((statusReg2 & 2) << 7)) &
		       (!blinkState << 8);
	}

	/** Similar to the above getEvenOddMask() method, but can also be
	  * called when 'isFastBlinkEnabled() == true'. In the latter case
	  * the timing is based on lines instead of frames. This means the
	  * result is no longer fixed per frame, and thus this method takes
	  * an additional line parameter.
	  */
	[[nodiscard]] inline unsigned getEvenOddMask(int line) const {
		if (isFastBlinkEnabled()) {
			// EO and IL not considered in this mode
			auto p = calculateLineBlinkState(line);
			return (!p.state) << 8;
		} else {
			return getEvenOddMask();
		}
	}

	/** Calculates what 'blinkState' and 'blinkCount' would be at a specific line.
	  * (The actual 'blinkState' and 'blinkCount' variables represent the values
	  * for line 0 and remain fixed for the duration of the frame.
	  */
	struct BlinkStateCount {
		bool state;
		int count;
	};
	[[nodiscard]] BlinkStateCount calculateLineBlinkState(unsigned line) const {
		assert(isFastBlinkEnabled());

		if (blinkCount == 0) { // not changing
			return {blinkState, blinkCount};
		}

		unsigned evenLen = ((controlRegs[13] >> 4) & 0x0F) * 10;
		unsigned oddLen  = ((controlRegs[13] >> 0) & 0x0F) * 10;
		unsigned totalLen = evenLen + oddLen;
		assert(totalLen != 0); // because this implies 'blinkCount == 0'
		line %= totalLen; // reduce double flips

		bool resultState = blinkState; // initial guess, adjusted later
		if (blinkState) {
			// We start in the 'even' period -> check first for
			// even/odd transition, next for odd/even
		} else {
			// We start in the 'odd' period -> do the opposite
			std::swap(evenLen, oddLen);
		}
		int newCount = blinkCount - narrow<int>(line);
		if (newCount <= 0) {
			// switch even->odd    (or odd->even)
			resultState = !resultState;
			newCount += narrow<int>(oddLen);
			if (newCount <= 0) {
				// switch odd->even   (or even->odd)
				resultState = !resultState;
				newCount += narrow<int>(evenLen);
				assert(newCount > 0);
			}
		}
		return {resultState, newCount};
	}

	/** Gets the number of VDP clock ticks (21MHz) elapsed between
	  * a given time and the start of this frame.
	  */
	[[nodiscard]] inline int getTicksThisFrame(EmuTime::param time) const {
		return narrow<int>(frameStartTime.getTicksTill_fast(time));
	}

	[[nodiscard]] inline EmuTime::param getFrameStartTime() const {
		return frameStartTime.getTime();
	}

	/** Gets the sprite size in pixels (8/16).
	  */
	[[nodiscard]] inline int getSpriteSize() const {
		return ((controlRegs[1] & 2) << 2) + 8;
	}

	/** Are sprites magnified?
	  */
	[[nodiscard]] inline bool isSpriteMag() const {
		return controlRegs[1] & 1;
	}

	/** Are commands possible in non Graphic modes? (V9958 only)
	  * @return True iff CMD bit set.
	  */
	[[nodiscard]] inline bool getCmdBit() const {
		return (controlRegs[25] & 0x40) != 0;
	}

	/** Gets the number of lines per frame.
	  */
	[[nodiscard]] inline int getLinesPerFrame() const {
		return palTiming ? 313 : 262;
	}

	/** Gets the number of display lines per screen.
	  * @return 192 or 212.
	  */
	[[nodiscard]] inline int getNumberOfLines() const {
		return controlRegs[9] & 0x80 ? 212 : 192;
	}

	/** Gets the number of VDP clock ticks (21MHz) per frame.
	  */
	[[nodiscard]] inline int getTicksPerFrame() const {
		return getLinesPerFrame() * TICKS_PER_LINE;
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
	[[nodiscard]] inline bool isInsideFrame(EmuTime::param time) const {
		return time >= frameStartTime.getTime() &&
			getTicksThisFrame(time) <= getTicksPerFrame();
	}

	/** This is a combination of the (horizontal) set adjust register and
	  * the YJK-mode bit.
	  */
	[[nodiscard]] inline int getHorizontalAdjust() const {
		return horizontalAdjust;
	}

	/** Gets the number of VDP clock ticks between start of line and the start
	  * of the sprite plane.
	  * The location of the sprite plane is not influenced by horizontal scroll
	  * or border mask.
	  * TODO: Leave out the text mode case, since there are no sprites
	  *       in text mode?
	  */
	[[nodiscard]] inline int getLeftSprites() const {
		return 100 + 102 + 56
			+ (horizontalAdjust - 7) * 4
			+ (displayMode.isTextMode() ? 36 : 0);
	}

	/** Gets the number of VDP clock ticks between start of line and the end
	  * of the left border.
	  * Does not include extra pixels of horizontal scroll low, since those
	  * are not actually border pixels (sprites appear in front of them).
	  */
	[[nodiscard]] inline int getLeftBorder() const {
		return getLeftSprites() + (isBorderMasked() ? 8 * 4 : 0);
	}

	/** Gets the number of VDP clock ticks between start of line and the start
	  * of the right border.
	  */
	[[nodiscard]] inline int getRightBorder() const {
		return getLeftSprites()
			+ (displayMode.isTextMode() ? 960 : 1024);
	}

	/** Gets the number of VDP clock ticks between start of line and the time
	  * when the background pixel with X coordinate 0 would be drawn.
	  * This includes extra pixels of horizontal scroll low,
	  * but disregards border mask.
	  */
	[[nodiscard]] inline int getLeftBackground() const {
		return getLeftSprites() + getHorizontalScrollLow() * 4;
	}

	/** Should only be used by SpriteChecker. Returns the current value
	  * of status register 0 (both the F-flag and the sprite related bits).
	  */
	[[nodiscard]] byte getStatusReg0() const { return statusReg0; }

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
	[[nodiscard]] bool getVRMode() const {
		return (controlRegs[8] & 8) != 0;
	}

	/** Enable superimposing
	  */
	void setExternalVideoSource(const RawFrame* externalSource) {
		externalVideo = externalSource;
	}

	/** Value of the cmdTiming setting, true means commands have infinite speed.
	 */
	[[nodiscard]] bool getBrokenCmdTiming() const {
		return brokenCmdTiming;
	}

	/** Get the earliest access slot that is at least 'delta' cycles in
	  * the future. */
	[[nodiscard]] EmuTime getAccessSlot(EmuTime::param time, VDPAccessSlots::Delta delta) const;

	/** Same as getAccessSlot(), but it can be _much_ faster for repeated
	  * calls, e.g. in the implementation of VDP commands. However it does
	  * have some limitations:
	  * - The returned calculator only remains valid for as long as
	  *   the VDP access timing remains the same (display/sprite enable).
	  * - The calculator needs to see _all_ time changes.
	  * (So this means that in every VDPCmd::execute() method you need
	  * to construct a new calculator).
	  */
	[[nodiscard]] VDPAccessSlots::Calculator getAccessSlotCalculator(
		EmuTime::param time, EmuTime::param limit) const;

	/** Only used when there are commandExecuting-probe listeners.
	 *
	 * Call to announce a (lower-bound) estimate for when the VDP command
	 * will finish executing. In response the VDP will schedule a
	 * synchronization point to sync with VDPCmdEngine emulation.
	 *
	 * Normally it's not required to pro-actively sync with the end of a
	 * VDP command. Instead these sync happen reactively on VDP status
	 * reads (e.g. polling the CE bit) or on VRAM reads (rendering or CPU
	 * VRAM read). This is in contrast with the V9990 where we DO need an
	 * active sync because the V9990 can generate an IRQ on command end.
	 *
	 * Though when the VDP.commandExecuting probe is in use we do want a
	 * reasonably timing accurate reaction of that probe. So (only) then we
	 * do add the extra syncs (thus with extra emulation overhead when you
	 * use that probe).
	 */
	void scheduleCmdSync(EmuTime t) {
		if (auto now = getCurrentTime(); t <= now) {
			// The largest amount of VDP cycles between 'progress'
			// in command emulation:
			// - worst case the LMMM takes 120+64 cycles to fully process one pixel
			// - the largest gap between access slots is 70 cycles
			// - but if we're unlucky the CPU steals that slot
			int LARGEST_STALL = 184 + 2 * 70;

			t = now + VDPClock::duration(LARGEST_STALL);
		}
		syncCmdDone.setSyncPoint(t);
	}

	/**
	 * Returns the position of the raster beam expressed in 'narrow' MSX
	 * screen coordinates (like 'screen 7').
	 *
	 * Horizontally: pixels in the left border have a negative coordinate,
	 * pixels in the right border have a coordinate bigger than or equal to
	 * 512.
	 * For MSX screen modes that are 256 pixels wide (e.g. 'screen 5')
	 * divide the x-coordinate by 2. For text modes (aka screen 0, width
	 * 40/80) subtract 16 from the x-coordinate (= (512-480)/2), and divide
	 * by 2 (for width 40).
	 *
	 * Vertically: lines in the top border have negative coordinates, lines
	 * in the bottom border have coordinates bigger or equal to 192 or 212.
	 */
	[[nodiscard]] gl::ivec2 getMSXPos(EmuTime::param time) const {
		auto ticks = getTicksThisFrame(time);
		return {((ticks % VDP::TICKS_PER_LINE) - getLeftSprites()) / 2,
		         (ticks / VDP::TICKS_PER_LINE) - getLineZero()};
	}

	// for debugging only
	VDPCmdEngine& getCmdEngine() { return *cmdEngine; }

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void initTables();

	// VdpVersion bitmasks
	static constexpr unsigned VM_MSX1             =   1; // set-> MSX1,       unset-> MSX2 or MSX2+
	static constexpr unsigned VM_PAL              =   2; // set-> fixed PAL,  unset-> fixed NTSC or switchable
	static constexpr unsigned VM_NO_MIRRORING     =   4; // set-> no (screen2) mirroring
	static constexpr unsigned VM_PALCOL_MIRRORING =   8; // set-> pattern/color-table mirroring
	static constexpr unsigned VM_VRAM_REMAPPING   =  16; // set-> 4k,8/16k VRAM remapping
	static constexpr unsigned VM_TOSHIBA_PALETTE  =  32; // set-> has Toshiba palette
	static constexpr unsigned VM_YJK              =  64; // set-> has YJK (MSX2+)
	static constexpr unsigned VM_YM2220_PALETTE   = 128; // set-> has YM2220 palette

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

		/** Yamaha clone (hardwired as PAL). */
		YM2220PAL  = VM_MSX1 | VM_YM2220_PALETTE | VM_PALCOL_MIRRORING | VM_PAL,

		/** Yamaha clone (hardwired as NTSC). */
		YM2220NTSC = VM_MSX1 | VM_YM2220_PALETTE | VM_PALCOL_MIRRORING,

		/** MSX2 VDP. */
		V9938      = 0,

		/** MSX2+ and turbo R VDP. */
		V9958      = VM_YJK,
	};

	struct SyncBase : public Schedulable {
		explicit SyncBase(const VDP& vdp_) : Schedulable(vdp_.getScheduler()) {}
		using Schedulable::removeSyncPoint;
		using Schedulable::setSyncPoint;
		using Schedulable::pendingSyncPoint;
	protected:
		~SyncBase() = default;
	};

	struct SyncVSync final : public SyncBase {
		using SyncBase::SyncBase;
		void executeUntil(EmuTime::param time) override {
			auto& vdp = OUTER(VDP, syncVSync);
			vdp.execVSync(time);
		}
	} syncVSync;

	struct SyncDisplayStart final : public SyncBase {
		using SyncBase::SyncBase;
		void executeUntil(EmuTime::param time) override {
			auto& vdp = OUTER(VDP, syncDisplayStart);
			vdp.execDisplayStart(time);
		}
	} syncDisplayStart;

	struct SyncVScan final : public SyncBase {
		using SyncBase::SyncBase;
		void executeUntil(EmuTime::param time) override {
			auto& vdp = OUTER(VDP, syncVScan);
			vdp.execVScan(time);
		}
	} syncVScan;

	struct SyncHScan final : public SyncBase {
		using SyncBase::SyncBase;
		void executeUntil(EmuTime::param /*time*/) override {
			auto& vdp = OUTER(VDP, syncHScan);
			vdp.execHScan();
		}
	} syncHScan;

	struct SyncHorAdjust final : public SyncBase {
		using SyncBase::SyncBase;
		void executeUntil(EmuTime::param time) override {
			auto& vdp = OUTER(VDP, syncHorAdjust);
			vdp.execHorAdjust(time);
		}
	} syncHorAdjust;

	struct SyncSetMode final : public SyncBase {
		using SyncBase::SyncBase;
		void executeUntil(EmuTime::param time) override {
			auto& vdp = OUTER(VDP, syncSetMode);
			vdp.execSetMode(time);
		}
	} syncSetMode;

	struct SyncSetBlank final : public SyncBase {
		using SyncBase::SyncBase;
		void executeUntil(EmuTime::param time) override {
			auto& vdp = OUTER(VDP, syncSetBlank);
			vdp.execSetBlank(time);
		}
	} syncSetBlank;

	struct SyncSetSprites final : public SyncBase {
		using SyncBase::SyncBase;
		void executeUntil(EmuTime::param time) override {
			auto& vdp = OUTER(VDP, syncSetSprites);
			vdp.execSetSprites(time);
		}
	} syncSetSprites;

	struct SyncCpuVramAccess final : public SyncBase {
		using SyncBase::SyncBase;
		void executeUntil(EmuTime::param time) override {
			auto& vdp = OUTER(VDP, syncCpuVramAccess);
			vdp.execCpuVramAccess(time);
		}
	} syncCpuVramAccess;

	struct SyncCmdDone final : public SyncBase {
		using SyncBase::SyncBase;
		void executeUntil(EmuTime::param time) override {
			auto& vdp = OUTER(VDP, syncCmdDone);
			vdp.execSyncCmdDone(time);
		}
	} syncCmdDone;

	void execVSync(EmuTime::param time);
	void execDisplayStart(EmuTime::param time);
	void execVScan(EmuTime::param time);
	void execHScan();
	void execHorAdjust(EmuTime::param time);
	void execSetMode(EmuTime::param time);
	void execSetBlank(EmuTime::param time);
	void execSetSprites(EmuTime::param time);
	void execCpuVramAccess(EmuTime::param time);
	void execSyncCmdDone(EmuTime::param time);

	/** Returns the amount of vertical set-adjust 0..15.
	  * Neutral set-adjust (that is 'set adjust(0,0)') returns the value '7'.
	  */
	[[nodiscard]] int getVerticalAdjust() const {
		return (controlRegs[18] >> 4) ^ 0x07;
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
	[[nodiscard]] inline bool getHR(int ticksThisFrame) const {
		// Note: These constants are located inside this function because
		//       GCC 4.0.x won't link if they are in the class scope.
		/** Length of horizontal blank (HR=1) in text mode, measured in VDP
		  * ticks.
		  */
		static constexpr int HBLANK_LEN_TXT = 404;
		/** Length of horizontal blank (HR=1) in graphics mode, measured in VDP
		  * ticks.
		  */
		static constexpr int HBLANK_LEN_GFX = 312;
		return (ticksThisFrame + TICKS_PER_LINE - getRightBorder()) % TICKS_PER_LINE
		     < (displayMode.isTextMode() ? HBLANK_LEN_TXT : HBLANK_LEN_GFX);
	}

	// VideoSystemChangeListener interface:
	void preVideoSystemChange() noexcept override;
	void postVideoSystemChange() noexcept override;

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
	[[nodiscard]] byte vramRead(EmuTime::param time);

	/** Helper methods for CPU-VRAM access. */
	void scheduleCpuVramAccess(bool isRead, byte write, EmuTime::param time);
	void executeCpuVramAccess(EmuTime::param time);

	/** Read the contents of a status register
	  */
	[[nodiscard]] byte readStatusReg(byte reg, EmuTime::param time);

	/** Schedule a sync point at the start of the next line.
	  */
	void syncAtNextLine(SyncBase& type, EmuTime::param time) const;

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
	void updateDisplayMode(DisplayMode newMode, bool cmdBit, EmuTime::param time);

	// Observer<Setting>
	void update(const Setting& setting) noexcept override;

private:
	Display& display;
	EnumSetting<bool>& cmdTiming;
	EnumSetting<bool>& tooFastAccess;

	struct RegDebug final : SimpleDebuggable {
		explicit RegDebug(const VDP& vdp);
		[[nodiscard]] byte read(unsigned address) override;
		void write(unsigned address, byte value, EmuTime::param time) override;
	} vdpRegDebug;

	struct StatusRegDebug final : SimpleDebuggable {
		explicit StatusRegDebug(const VDP& vdp);
		[[nodiscard]] byte read(unsigned address, EmuTime::param time) override;
	} vdpStatusRegDebug;

	struct PaletteDebug final : SimpleDebuggable {
		explicit PaletteDebug(const VDP& vdp);
		[[nodiscard]] byte read(unsigned address) override;
		void write(unsigned address, byte value, EmuTime::param time) override;
	} vdpPaletteDebug;

	struct VRAMPointerDebug final : SimpleDebuggable {
		explicit VRAMPointerDebug(const VDP& vdp);
		[[nodiscard]] byte read(unsigned address) override;
		void write(unsigned address, byte value, EmuTime::param time) override;
	} vramPointerDebug;

	struct RegisterLatchStatusDebug final : SimpleDebuggable {
		explicit RegisterLatchStatusDebug(const VDP& vdp);
		[[nodiscard]] byte read(unsigned address) override;
	} registerLatchStatusDebug;

	struct VramAccessStatusDebug final : SimpleDebuggable {
		explicit VramAccessStatusDebug(const VDP& vdp);
		[[nodiscard]] byte read(unsigned address) override;
	} vramAccessStatusDebug;

	struct PaletteLatchStatusDebug final : SimpleDebuggable {
		explicit PaletteLatchStatusDebug(const VDP& vdp);
		[[nodiscard]] byte read(unsigned address) override;
	} paletteLatchStatusDebug;

	struct DataLatchDebug final : SimpleDebuggable {
		explicit DataLatchDebug(const VDP& vdp);
		[[nodiscard]] byte read(unsigned address) override;
	} dataLatchDebug;

	class Info : public InfoTopic {
	public:
		void execute(std::span<const TclObject> tokens,
		             TclObject& result) const override;
		[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
		[[nodiscard]] virtual int calc(const EmuTime& time) const = 0;
	protected:
		Info(VDP& vdp_, const std::string& name, std::string helpText_);
		~Info() = default;
		VDP& vdp;
		const std::string helpText;
	};

	struct FrameCountInfo final : Info {
		explicit FrameCountInfo(VDP& vdp);
		[[nodiscard]] int calc(const EmuTime& time) const override;
	} frameCountInfo;

	struct CycleInFrameInfo final : Info {
		explicit CycleInFrameInfo(VDP& vdp);
		[[nodiscard]] int calc(const EmuTime& time) const override;
	} cycleInFrameInfo;

	struct LineInFrameInfo final : Info {
		explicit LineInFrameInfo(VDP& vdp);
		[[nodiscard]] int calc(const EmuTime& time) const override;
	} lineInFrameInfo;

	struct CycleInLineInfo final : Info {
		explicit CycleInLineInfo(VDP& vdp);
		[[nodiscard]] int calc(const EmuTime& time) const override;
	} cycleInLineInfo;

	struct MsxYPosInfo final : Info {
		explicit MsxYPosInfo(VDP& vdp);
		[[nodiscard]] int calc(const EmuTime& time) const override;
	} msxYPosInfo;

	struct MsxX256PosInfo final : Info {
		explicit MsxX256PosInfo(VDP& vdp);
		[[nodiscard]] int calc(const EmuTime& time) const override;
	} msxX256PosInfo;

	struct MsxX512PosInfo final : Info {
		explicit MsxX512PosInfo(VDP& vdp);
		[[nodiscard]] int calc(const EmuTime& time) const override;
	} msxX512PosInfo;

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
	OptionalIRQHelper irqVertical;

	/** Manages horizontal scanning interrupt request.
	  */
	OptionalIRQHelper irqHorizontal;

	/** Time of last set DISPLAY_START sync point.
	  */
	EmuTime displayStartSyncTime;

	/** Time of last set VSCAN sync point.
	  */
	EmuTime vScanSyncTime;

	/** Time of last set HSCAN sync point.
	  */
	EmuTime hScanSyncTime;

	TclCallback tooFastCallback;
	TclCallback dotClockDirectionCallback;

	/** VDP version.
	  */
	VdpVersion version;

	/** Saturation of Pr component of TMS9XXXA output circuitry.
	  * The output of the VDP and the circuitry between the output and the
	  * output connector influences this value. Percentage in range [0:100]
	  */
	int saturationPr;

	/** Saturation of Pb component of TMS9XXXA output circuitry.
	  * The output of the VDP and the circuitry between the output and the
	  * output connector influences this value. Percentage in range [0:100]
	  */
	int saturationPb;

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
	std::array<byte, 32> controlRegs;

	/** Mask on the control register index:
	  * makes MSX2 registers inaccessible on MSX1,
	  * instead the MSX1 registers are mirrored.
	  */
	byte controlRegMask;

	/** Mask on the values of control registers.
	  * This saves a lot of masking when using the register values,
	  * because it is guaranteed non-existant bits are always zero.
	  * It also disables access to VDP features on a VDP model
	  * which does not support those features.
	  */
	std::array<byte, 32> controlValueMasks;

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
	std::array<uint16_t, 16> palette;

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
	bool interlaced = false;

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

	/** Direction of VRAM access for reading or writing
	  * Note: this variable is _only_ used for the 'VRAM access status' debuggable.
	  *   The real VDP allows to setup a read/write VRAM address and then do the opposite
	  *   out/in operation.
	  * See the variables 'cpuVramData' and 'cpuVramReqIsRead' for more details.
	  */
	bool writeAccess;

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
	bool pendingCpuAccess; // always equal to pendingSyncPoint(CPU_VRAM_ACCESS)

	/** Does CPU interface access main VRAM (false) or extended VRAM (true)?
	  * This is determined by MXC (R#45, bit 6).
	  */
	bool cpuExtendedVram;

	/** Current display mode. Note that this is not always the same as the
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

	/** Are sprites enabled. This only reflects the SPD bit in R#8. It does
	  * not take screen mode, screen enabled or vertical borders into
	  * account. It's not identical to the SPD bit because this variable is
	  * only updated at the start of the next line.
	  */
	bool spriteEnabled;

	/** Has a warning been printed.
	  * This is set when a warning about setting the dot clock direction
	  * is printed.  */
	bool warningPrinted = false;

	/** Cached version of cmdTiming/tooFastAccess setting. */
	bool brokenCmdTiming;
	bool allowTooFastAccess;

	/** Cached CPU reference */
	MSXCPU& cpu;
	const byte fixedVDPIOdelayCycles;
};
SERIALIZE_CLASS_VERSION(VDP, 10);

} // namespace openmsx

#endif
