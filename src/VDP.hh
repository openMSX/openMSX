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

#include <string>

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
class VDP : public MSXIODevice, public Schedulable
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
	virtual void executeUntilEmuTime(const EmuTime &time, int userData);

	/** Is this an MSX1 VDP?
	  * @return True if this is an MSX1 VDP (TMS99X8A or TMS9929A),
	  *   False otherwise.
	  */
	inline bool isMSX1VDP() {
		return version == TMS99X8A || version == TMS9929A;
	}

	/** Get the display mode the VDP is in.
	  * @return The current display mode.
	  */
	inline DisplayMode getDisplayMode() {
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
	inline bool getTransparency() {
		return (controlRegs[8] & 0x20) == 0;
	}

	/** Gets the current foreground colour.
	  * @return Colour value [0..15].
	  */
	inline int getForegroundColour() {
		return controlRegs[7] >> 4;
	}

	/** Gets the current background colour.
	  * @return Colour value [0..15].
	  */
	inline int getBackgroundColour() {
		return controlRegs[7] & 0x0F;
	}

	/** Gets the current blinking colour for blinking text.
	  * @return Colour value [0..15].
	  */
	inline int getBlinkForegroundColour() {
		return controlRegs[12] >> 4;
	}

	/** Gets the current blinking colour for blinking text.
	  * @return Colour value [0..15].
	  */
	inline int getBlinkBackgroundColour() {
		return controlRegs[12] & 0x0F;
	}

	/** Gets the current blink state.
	  * @return True iff alternate colours / page should be displayed.
	  */
	inline bool getBlinkState() {
		return blinkState;
	}

	/** Gets a palette entry.
	  * @param index The index [0..15] in the palette.
	  * @return Colour value in the format of the palette registers:
	  *   bit 10..8 is green, bit 6..4 is red and bit 2..0 is blue.
	  */
	inline int getPalette(int index) {
		return palette[index];
	}

	/** Is the display enabled?
	  * Both the regular border and forced blanking by clearing
	  * the display enable bit are considered disabled display.
	  * @return true iff enabled.
	  */
	inline bool isDisplayEnabled() {
		return isDisplayArea && (controlRegs[1] & 0x40);
	}

	/** Gets the current vertical scroll (line displayed at Y=0).
	  * @return Vertical scroll register value.
	  */
	inline byte getVerticalScroll() {
		return controlRegs[23];
	}

	/** Gets the current horizontal display adjust.
	  * @return Adjust: 0 is leftmost, 7 is center, 15 is rightmost.
	  */
	inline int getHorizontalAdjust() {
		return horizontalAdjust;
	}

	/** Gets the current vertical display adjust.
	  * @return Adjust: 0 is topmost, 7 is center, 15 is bottommost.
	  */
	inline int getVerticalAdjust() {
		return verticalAdjust;
	}

	/** Get the absolute line number of display line zero.
	  * Usually this is equal to the height of the top border,
	  * but not so during overscan.
	  */
	inline int getLineZero() {
		return lineZero;
	}

	/** Is PAL timing active?
	  * This setting is fixed at start of frame.
	  * @return True if PAL timing, false if NTSC timing.
	  */
	inline bool isPalTiming() {
		return palTiming;
	}

	/** Get interlace status.
	  * Interlace means the odd fields are displayed half a line lower
	  * than the even fields. Together with even/odd page alternation
	  * this can be used to create an interlaced display.
	  * This setting is fixed at start of frame.
	  * @return True iff this field should be displayed half a line lower.
	  */
	inline bool isInterlaced() {
		return interlaced;
	}

	/** Get interlace status.
	  * Interlace means the odd fields are displayed half a line lower
	  * than the even fields. Together with even/odd page alternation
	  * this can be used to create an interlaced display.
	  * This setting is fixed at start of frame.
	  * @return True iff this field should be displayed half a line lower.
	  */
	inline bool isEvenOddEnabled() {
		return controlRegs[9] & 4;
	}

	/** Is the even or odd field being displayed?
	  * @return True iff this field should be displayed half a line lower.
	  */
	inline bool getEvenOdd() {
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
	inline int getEvenOddMask() {
		// TODO: Verify which page is displayed on even fields.
		return ((~controlRegs[9] & 4) << 6) | ((statusReg2 & 2) << 7);
	}

	/** Gets the number of VDP clock ticks (21MHz) elapsed between
	  * a given time and the start of this frame.
	  */
	inline int getTicksThisFrame(const EmuTime &time) {
		return frameStartTime.getTicksTill(time);
	}

	/** Get VRAM access timing info.
	  * This is the internal format used by the command engine.
	  * TODO: When improving the timing accuracy, think of a clearer
	  *       way of sharing this information.
	  */
	inline int getAccessTiming() {
		return (isDisplayEnabled() & 1)	// display enable
		       | (controlRegs[8] & 2);	// sprite enable
	}

	/** Gets the sprite size in pixels (8/16).
	  */
	inline int getSpriteSize() {
		return ((controlRegs[1] & 2) << 2) + 8;
	}

	/** Gets the sprite magnification.
	  * @return Magnification: 0 = normal, 1 = double.
	  * TODO: Returning 1 and 2 may be more logical.
	  */
	inline int getSpriteMag() {
		return controlRegs[1] & 1;
	}

	/** Are sprites enabled?
	  * @return True iff blanking is off, the current mode supports
	  *   sprites and sprites are not disabled.
	  */
	inline bool spritesEnabled() {
		return ((controlRegs[1] & 0x50) == 0x40)
			&& ((controlRegs[8] & 0x02) == 0x00);
	}

	/** Gets the number of VDP clockticks (21MHz) per frame.
	  */
	inline int getTicksPerFrame() {
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
	inline bool isInsideFrame(const EmuTime &time) {
		return time >= frameStartTime &&
			getTicksThisFrame(time) <= getTicksPerFrame();
	}

private:
	class VDPRegsCmd : public Command {
	public:
		VDPRegsCmd(VDP *vdp);
		virtual void execute(const std::vector<std::string> &tokens,
		                     const EmuTime &time);
		virtual void help(const std::vector<std::string> &tokens) const;
	private:
		VDP *vdp;
	};
	friend class VDPRegsCmd;

	class PaletteCmd : public Command {
	public:
		PaletteCmd(VDP *vdp);
		virtual void execute(const std::vector<std::string> &tokens,
		                     const EmuTime &time);
		virtual void help(const std::vector<std::string> &tokens) const;
	private:
		VDP *vdp;
	};

	class RendererCmd : public Command {
	public:
		RendererCmd(VDP *vdp);
		virtual void execute(const std::vector<std::string> &tokens,
		                     const EmuTime &time);
		virtual void help(const std::vector<std::string> &tokens) const;
	private:
		VDP *vdp;
	};
	friend class RendererCmd;

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
	};

	/** Gets the number of display lines per screen.
	  * @return 192 or 212.
	  */
	inline int getNumberOfLines() {
		return controlRegs[9] & 0x80 ? 212 : 192;
	}

	/** Gets the value of the horizontal retrace status bit.
	  * Note that HR flipping continues at all times, not just during
	  * vertical display range.
	  * TODO: This method is used just once currently, if it stays
	  *   that way substitute the code instead of having a method.
	  * @param ticksThisFrame The screen position (in VDP ticks)
	  *    to return HR for.
	  * @return True iff the VDP scanning is inside the left/right
	  *   border or left/right erase or horizontal sync.
	  *   False iff the VDP scanning is in the display range.
	  */
	inline bool getHR(int ticksThisFrame) {
		// TODO: Use display adjust register (R#18).
		return (displayMode.isTextMode()
			? (ticksThisFrame + (87 + 27)) % TICKS_PER_LINE
			  < (TICKS_PER_LINE - 960)
			: (ticksThisFrame + (59 + 27)) % TICKS_PER_LINE
			  < (TICKS_PER_LINE - 1024)
			);
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

	/** VDP control register has changed, work out the consequences.
	  */
	void changeRegister(byte reg, byte val, const EmuTime &time);

	void setHorAdjust(const EmuTime &time);

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
	std::string rendererName;

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

	/** Current dispay mode.
	  */
	DisplayMode displayMode;

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

	/** Implements the renderer select command.
	  */
	RendererCmd rendererCmd;

	/** Execute a renderer switch at the earliest convenient time.
	  */
	bool switchRenderer;

};

#endif //__VDP_HH__

