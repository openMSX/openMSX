// $Id$

#ifndef __VDP_HH__
#define __VDP_HH__

#include "openmsx.hh"
#include "Schedulable.hh"
#include "MSXIODevice.hh"
#include "IRQHelper.hh"
#include "EmuTime.hh"
#include "Command.hh"
#include "DisplayMode.hh"
#include "Debuggable.hh"

#include <string>

using std::string;

namespace openmsx {

class Renderer;
class VDPCmdEngine;
class VDPVRAM;
class SpriteChecker;

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
class VDP : public MSXIODevice, private Schedulable
{
public:
	/** VDP version: the VDP model being emulated.
	  */
	enum VdpVersion {
		/** MSX1 VDP, NTSC version.
		  * TMS9918A has NTSC encoding built in,
		  * while TMS9928A has colour difference output;
		  * in emulation there is no difference.
		  */
		TMS99X8A,
		/** MSX1 VDP, PAL version.
		  */
		TMS9929A,
		/** MSX2 VDP.
		  */
		V9938,
		/** MSX2+ and turbo R VDP.
		  */
		V9958
	};

	/** Number of VDP clock ticks per second.
	  */
	static const int TICKS_PER_SECOND = 21477270;

	/** Number of VDP clock ticks per line.
	  */
	static const int TICKS_PER_LINE = 1368;

	/** Constructor.
	  */
	VDP(Device *config, const EmuTime &time);

	/** Destructor.
	  */
	virtual ~VDP();

	// mainlife cycle of an MSXDevice
	virtual void reset(const EmuTime &time);

	// interaction with CPU
	virtual byte readIO(byte port, const EmuTime &time);
	virtual void writeIO(byte port, byte value, const EmuTime &time);
	virtual void executeUntil(const EmuTime &time, int userData) throw();

	/** Is this an MSX1 VDP?
	  * @return True if this is an MSX1 VDP (TMS99X8A or TMS9929A),
	  *   False otherwise.
	  */
	inline bool isMSX1VDP() const {
		return version == TMS99X8A || version == TMS9929A;
	}

	/** Get the display mode the VDP is in.
	  * @return The current display mode.
	  */
	inline DisplayMode getDisplayMode() const {
		return displayMode;
	}

	/** Get the VRAM object for this VDP.
	  */
	inline VDPVRAM *getVRAM() {
		return vram;
	}

	/** Get the sprite checker for this VDP.
	  */
	inline SpriteChecker *getSpriteChecker() {
		return spriteChecker;
	}

	/** Gets the current transparency setting.
	  * @return True iff colour 0 is transparent.
	  */
	inline bool getTransparency() const {
		return (controlRegs[8] & 0x20) == 0;
	}

	/** Gets the current foreground colour.
	  * @return Colour value [0..15].
	  */
	inline int getForegroundColour() const {
		return controlRegs[7] >> 4;
	}

	/** Gets the current background colour.
	  * @return Colour value [0..15].
	  */
	inline int getBackgroundColour() const {
		return controlRegs[7] & 0x0F;
	}

	/** Gets the current blinking colour for blinking text.
	  * @return Colour value [0..15].
	  */
	inline int getBlinkForegroundColour() const {
		return controlRegs[12] >> 4;
	}

	/** Gets the current blinking colour for blinking text.
	  * @return Colour value [0..15].
	  */
	inline int getBlinkBackgroundColour() const {
		return controlRegs[12] & 0x0F;
	}

	/** Gets the current blink state.
	  * @return True iff alternate colours / page should be displayed.
	  */
	inline bool getBlinkState() const {
		return blinkState;
	}

	/** Gets a palette entry.
	  * @param index The index [0..15] in the palette.
	  * @return Colour value in the format of the palette registers:
	  *   bit 10..8 is green, bit 6..4 is red and bit 2..0 is blue.
	  */
	inline int getPalette(int index) const {
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
		return controlRegs[25] & 0x02;
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

	/** Gets the current horizontal display adjust.
	  * @return Adjust: 0 is leftmost, 7 is center, 15 is rightmost.
	  */
	inline int getHorizontalAdjust() const {
		return horizontalAdjust;
	}

	/** Gets the current vertical display adjust.
	  * @return Adjust: 0 is topmost, 7 is center, 15 is bottommost.
	  */
	inline int getVerticalAdjust() const {
		return verticalAdjust;
	}

	/** Get the absolute line number of display line zero.
	  * Usually this is equal to the height of the top border,
	  * but not so during overscan.
	  */
	inline int getLineZero() const {
		return lineZero;
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
	  * @return True iff this field should be displayed half a line lower.
	  */
	inline bool isInterlaced() const {
		return interlaced;
	}

	/** Get interlace status.
	  * Interlace means the odd fields are displayed half a line lower
	  * than the even fields. Together with even/odd page alternation
	  * this can be used to create an interlaced display.
	  * This setting is fixed at start of frame.
	  * @return True iff this field should be displayed half a line lower.
	  */
	inline bool isEvenOddEnabled() const {
		return controlRegs[9] & 4;
	}

	/** Is the even or odd field being displayed?
	  * @return True iff this field should be displayed half a line lower.
	  */
	inline bool getEvenOdd() const {
		return statusReg2 & 2;
	}

	/** Expresses the state of even/odd page interchange in a mask
	  * on the line number. If even/off interchange is active, for some
	  * frames lines 256..511 (page 1) are replaced by 0..255 (page 0)
	  * and 768..1023 (page 3, if appicable) by 512..767 (page 2).
	  * Together with the interlace setting this can be used to create
	  * an interlaced display.
	  * @return Line number mask that expressed even/odd state.
	  */
	inline int getEvenOddMask() const {
		// TODO: Verify which page is displayed on even fields.
		return ((~controlRegs[9] & 4) << 6) | ((statusReg2 & 2) << 7);
	}

	/** Gets the number of VDP clock ticks (21MHz) elapsed between
	  * a given time and the start of this frame.
	  */
	inline int getTicksThisFrame(const EmuTime &time) const {
		return frameStartTime.getTicksTill(time);
	}

	/** Get VRAM access timing info.
	  * This is the internal format used by the command engine.
	  * TODO: When improving the timing accuracy, think of a clearer
	  *       way of sharing this information.
	  */
	inline int getAccessTiming() const {
		return (isDisplayEnabled() & 1)	// display enable
		       | (controlRegs[8] & 2);	// sprite enable
	}

	/** Gets the sprite size in pixels (8/16).
	  */
	inline int getSpriteSize() const {
		return ((controlRegs[1] & 2) << 2) + 8;
	}

	/** Gets the sprite magnification.
	  * @return Magnification: 1 = normal, 2 = double.
	  */
	inline int getSpriteMag() const {
		return (controlRegs[1] & 1) + 1;
	}

	/** Are sprites enabled?
	  * @return True iff blanking is off, the current mode supports
	  *   sprites and sprites are not disabled.
	  */
	inline bool spritesEnabled() const {
		return displayEnabled && !displayMode.isTextMode() &&
		       ((controlRegs[8] & 0x02) == 0x00);
	}

	/** Are commands possible in non Graphic modes? (V9958 only)
	  * @return True iff CMD bit set.
	  */
	inline bool getCmdBit() const {
		return controlRegs[25] & 0x40;
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
	inline bool isInsideFrame(const EmuTime &time) const {
		return time >= frameStartTime &&
			getTicksThisFrame(time) <= getTicksPerFrame();
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

private:
	class VDPRegDebug : public Debuggable {
	public:
		VDPRegDebug(VDP& parent);
		virtual unsigned getSize() const;
		virtual const string& getDescription() const;
		virtual byte read(unsigned address);
		virtual void write(unsigned address, byte value);
	private:
		VDP& parent;
	} vdpRegDebug;

	class VDPStatusRegDebug : public Debuggable {
	public:
		VDPStatusRegDebug(VDP& parent);
		virtual unsigned getSize() const;
		virtual const string& getDescription() const;
		virtual byte read(unsigned address);
		virtual void write(unsigned address, byte value);
	private:
		VDP& parent;
	} vdpStatusRegDebug;
	
	class VDPRegsCmd : public Command {
	public:
		VDPRegsCmd(VDP *vdp);
		virtual string execute(const vector<string> &tokens) throw();
		virtual string help(const vector<string> &tokens) const throw();
	private:
		VDP *vdp;
	};
	friend class VDPRegsCmd;

	class PaletteCmd : public Command {
	public:
		PaletteCmd(VDP *vdp);
		virtual string execute(const vector<string> &tokens) throw();
		virtual string help(const vector<string> &tokens) const throw();
	private:
		VDP *vdp;
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

	/** Types of VDP sync points that can be scheduled.
	  */
	enum SyncType {
		/** Vertical sync: the transition from one frame to the next.
		  */
		VSYNC,
		/** Start of a new frame.
		  * Usually this is done in the VSYNC handler, except when emulation
		  * is paused, in which case frame start is delayed until unpause.
		  */
		FRAME_START,
		/** Start of display.
		  */
		DISPLAY_START,
		/** Vertical scanning: end of display.
		  */
		VSCAN,
		/** Horizontal scanning: line interrupt.
		  */
		HSCAN,
		/** Horizontal adjust change
		  */
		HOR_ADJUST,
		/** Change mode
		  */
		SET_MODE,
		/** Enable/disable screen
		  */
		SET_BLANK,
	};

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
		return
			( ticksThisFrame + TICKS_PER_LINE - getRightBorder()
				) % TICKS_PER_LINE
			< TICKS_PER_LINE - (displayMode.isTextMode() ? 960 : 1024);
	}

	/** Called both on init and on reset.
	  * Puts VDP into reset state.
	  * Does not call any renderer methods.
	  */
	void resetInit(const EmuTime &time);

	/** Companion to resetInit: in resetInit the registers are reset,
	  * in this method the new base masks are distributed to the VDP
	  * subsystems.
	  */
	void resetMasks(const EmuTime &time);

	/** Start a new frame.
	  * @param time The moment in emulated time the frame starts.
	  */
	void frameStart(const EmuTime &time);

	/** Schedules a DISPLAY_START sync point.
	  * Also removes a pending DISPLAY_START sync, if any.
	  * Since HSCAN and VSCAN are relative to display start,
	  * their schedule methods are called by this method.
	  * @param time The moment in emulated time this call takes place.
	  *   Note: time is not the DISPLAY_START sync time!
	  */
	void scheduleDisplayStart(const EmuTime &time);

	/** Schedules a VSCAN sync point.
	  * Also removes a pending VSCAN sync, if any.
	  * @param time The moment in emulated time this call takes place.
	  *   Note: time is not the VSCAN sync time!
	  */
	void scheduleVScan(const EmuTime &time);

	/** Schedules a HSCAN sync point.
	  * Also removes a pending HSCAN sync, if any.
	  * @param time The moment in emulated time this call takes place.
	  *   Note: time is not the HSCAN sync time!
	  */
	void scheduleHScan(const EmuTime &time);

	/** Byte is read from VRAM by the CPU.
	  */
	byte vramRead(const EmuTime &time);

	/** Read the contents of a status register
	  */ 
	byte peekStatusReg(byte reg, const EmuTime& time) const;
	byte readStatusReg(byte reg, const EmuTime& time);

	/** VDP control register has changed, work out the consequences.
	  */
	void changeRegister(byte reg, byte val, const EmuTime &time);

	/** Schedule a sync point at the start of the next line.
	  */ 
	void syncAtNextLine(SyncType type, const EmuTime &time);

	/** Colour base mask has changed.
	  * Inform the renderer and the VRAM.
	  */
	void updateColourBase(const EmuTime &time);

	/** Pattern base mask has changed.
	  * Inform the renderer and the VRAM.
	  */
	void updatePatternBase(const EmuTime &time);

	/** Sprite attribute base mask has changed.
	  * Inform the SpriteChecker and the VRAM.
	  */
	void updateSpriteAttributeBase(const EmuTime &time);

	/** Sprite pattern base mask has changed.
	  * Inform the SpriteChecker and the VRAM.
	  */
	void updateSpritePatternBase(const EmuTime &time);

	/** Display mode has changed.
	  * Update displayMode's value and inform the Renderer.
	  */
	void updateDisplayMode(DisplayMode newMode, const EmuTime &time);

	/** Renderer that converts this VDP's state into an image.
	  */
	Renderer *renderer;

	/** Name of the current Renderer.
	  * TODO: Retrieve this from the Renderer object?
	  *       Possible, but avoid duplication of name->class mapping.
	  */
	string rendererName;

	/** Command engine: the part of the V9938/58 that executes commands.
	  */
	VDPCmdEngine *cmdEngine;

	/** Sprite checker: calculates sprite patterns and collisions.
	  */
	SpriteChecker *spriteChecker;

	/** VDP version.
	  */
	VdpVersion version;

	/** The emulation time when this frame was started (vsync).
	  */
	EmuTimeFreq<TICKS_PER_SECOND> frameStartTime;

	/** Manages vertical scanning interrupt request.
	  */
	IRQHelper irqVertical;

	/** Manages horizontal scanning interrupt request.
	  */
	IRQHelper irqHorizontal;

	/** Is the current scan position inside the display area?
	  */
	bool isDisplayArea;

	/** VDP ticks between start of frame and start of display.
	  */
	int displayStart;

	/** VDP ticks between start of frame and the moment horizontal
	  * scan match occurs.
	  */
	int horizontalScanOffset;

	/** Time of last set DISPLAY_START sync point.
	  */
	EmuTime displayStartSyncTime;

	/** Time of last set VSCAN sync point.
	  */
	EmuTime vScanSyncTime;

	/** Time of last set HSCAN sync point.
	  */
	EmuTime hScanSyncTime;

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

	/** Absolute line number of display line zero.
	  * @see getLineZero
	  */
	int lineZero;

	/** Horizontal display adjust.
	  * This value is update at the start of a line.
	  * @see getHorizontalAdjust.
	  */
	int horizontalAdjust;

	/** Vertical display adjust.
	  * This value is updated at the start of every frame.
	  * @see getVerticalAdjust.
	  */
	int verticalAdjust;

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

	/** Status register 0.
	  * All bits except bit 7 is always zero,
	  * their value can be retrieved from the sprite checker.
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

	/** V9938 palette.
	  */
	word palette[16];

	/** Blinking state: should alternate colour / page be displayed?
	  */
	bool blinkState;

	/** Blinking count: number of frames until next state.
	  * If the ON or OFF period is 0, blinkCount is fixed to 0.
	  */
	int blinkCount;

	/** VRAM management object.
	  */
	VDPVRAM *vram;

	/** VRAM mask: bit mask that indicates which address bits are
	  * present in the VRAM.
	  * Equal to VRAM size minus one because VRAM size is a power of two.
	  */
	int vramMask;

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
	  *   On TMS9928 the VRAM interface is the only access method.
	  *   But on V9938/58 there are other ways to access VRAM;
	  *   I wonder if they are consistent with this implementation.
	  */
	byte readAhead;

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

	/** VRAM read/write access pointer.
	  * Contains the lower 14 bits of the current VRAM access address.
	  */
	int vramPointer;

	/** Implements the vdp register print command.
	  */
	VDPRegsCmd vdpRegsCmd;

	/** Implements the palette print command.
	  */
	PaletteCmd paletteCmd;

};

} // namespace openmsx

#endif //__VDP_HH__

