// $Id$

#ifndef __V9990_HH__
#define __V9990_HH__

#include <string>
#include "openmsx.hh"
#include "Schedulable.hh"
#include "EventListener.hh"
#include "MSXDevice.hh"
#include "Debuggable.hh"
#include "IRQHelper.hh"
#include "Command.hh"
#include "V9990DisplayTiming.hh"
#include "V9990ModeEnum.hh"

namespace openmsx {

class V9990VRAM;
class V9990CmdEngine;
class V9990Renderer;
class EmuTime;

/** Implementation of the Yamaha V9990 VDP as used in the GFX9000
  * cartridge by Sunrise.
  */
class V9990 : public MSXDevice,
              private Schedulable,
              private EventListener
{
public:
	/** Constructor
	  */ 
	V9990(const XMLElement& config, const EmuTime& time);

	/** Destructor
	  */ 
	virtual ~V9990();

	// MSXDevice interface:
	virtual void reset(const EmuTime& time);
	virtual byte readIO(byte port, const EmuTime& time);
	virtual byte peekIO(byte port, const EmuTime& time) const;
	virtual void writeIO(byte port, byte value, const EmuTime& time);

	/** Obtain a reference to the V9990's VRAM
	  */
	inline V9990VRAM* getVRAM() {
		return vram.get();
	}

	/** Get even/odd page alternation status.
	  * @return True iff even/odd page alternation is enabled.
	  */
	inline bool isEvenOddEnabled() const {
		return regs[SCREEN_MODE_1] & 0x04;
	}

	/** Is the even or odd field being displayed?
	  * @return True iff the odd lines should be displayed.
	  */
	inline bool getEvenOdd() const {
		return status & 0x02;
	}

	/** Is the display enabled?
	  *  Note this is simpler than the V99x8 version. Probably ok
	  *  because V9990 doesn't have the same overscan trick (?)
	  * @return true iff enabled
	  */
	inline bool isDisplayEnabled() const {
		return regs[CONTROL] & 0x80;
	}

	/** Are sprites (cursors) enabled?
	  * @return true iff enabled
	  */
	inline bool spritesEnabled() const {
		return !(regs[CONTROL] & 0x40);
	}

	/** Get palette offset. 
	  * This is a number between [0..63], lowest two bits are always 0. 
	  * @return palette offset 
	  */
	inline byte getPaletteOffset() const {
		return (regs[PALETTE_CONTROL] & 0x0F) << 2;
	}
	
	/** Get the number of elapsed UC ticks in this frame.
	  * @param  time Point in emulated time.
	  * @return      Number of UC ticks.
	  */
	inline int getUCTicksThisFrame(const EmuTime& time) const {
		return frameStartTime.getTicksTill(time);
	}

	/** Is PAL timing active?
	  * This setting is fixed at start of frame.
	  * @return True if PAL timing, false if NTSC timing.
	  */
	inline bool isPalTiming() const {
		return palTiming;
	}

	/** Convert UC ticks to V9990 pixel position on a line
	  * @param ticks  Nr of UC Ticks
	  * @param mode   Display mode
	  * @return       Pixel position
	  * TODO: Move this to V9990DisplayTiming??
	  */
	static inline int UCtoX(int ticks, V9990DisplayMode mode) {
		int x;
		ticks = ticks % V9990DisplayTiming::UC_TICKS_PER_LINE;
		switch(mode) {
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
	
	/** Get VRAM offset for (X,Y) position.  Depending on the colormode,
	  * one byte in VRAM may span several pixels, or one pixel may span
	  * 1 or 2 bytes. 
	  * @param x     Pointer to X position - on exit, the X position is the
	  *              X position of the left most pixel at this VRAM address
	  * @param y     Y position
	  * @param mode  Color mode
	  * @return      VRAM offset
	  * TODO: Move this to V9990VRAM ??
	  */
	inline int XYtoVRAM(int *x, int y, V9990ColorMode mode) {
		int offset = *x + y * getImageWidth();
		switch(mode) {
			case PP:
			case BYUV:
			case BYUVP:
			case BYJK:
			case BYJKP:
			case BD8:
			case BP6:  break;
			case BD16: offset *= 2; break;
			case BP4:  offset /= 2; *x &= ~1; break;
			case BP2:  offset /= 4; *x &= ~3; break;
			default:   offset = 0; break;
		}
		return offset;
	}

	/** Return the current display mode
	  */
	inline V9990DisplayMode getDisplayMode() const {
		return mode;
	}

	/** Return the current color mode
	  */
	V9990ColorMode getColorMode();

	/** Return the current back drop color
	  * @return  Index the color palette
	  */
	inline int getBackDropColor() {
		return regs[BACK_DROP_COLOR];
	}

	/** Returns the X scroll offset for screen A of P1 and other modes
	  */
	inline int getScrollAX() {
		return regs[SCROLL_CONTROL_AX0] + 8 * regs[SCROLL_CONTROL_AX1];
	}

	/** Returns the Y scroll offset for screen A of P1 and other modes
	  */
	inline int getScrollAY() {
		return regs[SCROLL_CONTROL_AY0] + 256 * regs[SCROLL_CONTROL_AY1];
	}

	/** Returns the X scroll offset for screen B of P1 and other modes
	  */
	inline int getScrollBX() {
		return regs[SCROLL_CONTROL_BX0] + 8 * regs[SCROLL_CONTROL_BX1];
	}

	/** Returns the Y scroll offset for screen B of P1 and other modes
	  */
	inline int getScrollBY() {
		return regs[SCROLL_CONTROL_BY0] + 256 * regs[SCROLL_CONTROL_BY1];
	}

	/** Return the image width
	  */
	inline int getImageWidth() {
		return (256 << ((regs[SCREEN_MODE_0] & 0x0C) >> 2));
	}
			
	/** Command execution started
	  */
	inline void cmdStart() { status |= 0x01; }

	/** Command execution ready
	  */
	inline void cmdReady() {
		status &= 0xFE;
		raiseIRQ(CMD_IRQ);
	}

private:
	// Schedulable interface:
	virtual void executeUntil(const EmuTime& time, int userData);
	virtual const std::string& schedName() const;

	// EventListener interface:
	virtual bool signalEvent(const Event& event);
	
	// Debuggable: registers
	class V9990RegDebug : public Debuggable {
	public:
		V9990RegDebug(V9990& parent);
		virtual unsigned getSize() const;
		virtual const std::string& getDescription() const;
		virtual byte read(unsigned address);
		virtual void write(unsigned address, byte value);
	private:
		V9990& parent;
	} v9990RegDebug;
	
	// Debuggable: palette
	class V9990PalDebug : public Debuggable {
	public:
		V9990PalDebug(V9990& parent);
		virtual unsigned getSize() const;
		virtual const std::string& getDescription() const;
		virtual byte read(unsigned address);
		virtual void write(unsigned address, byte value);
	private:
		V9990& parent;
	} v9990PalDebug;

	// Command:
	class V9990RegsCmd : public SimpleCommand {
	public:
		V9990RegsCmd(V9990& v9990);
		virtual std::string execute(const std::vector<std::string>& tokens);
		virtual std::string help(const std::vector<std::string>& tokens) const;
	private:
		V9990& v9990;
	} v9990RegsCmd;

	// --- types ------------------------------------------------------

	/** Types of V9990 Sync points that can be scheduled
	  */
	enum V9990SyncType {
		/** Vertical Sync: transition to next frame.
		  */
		V9990_VSYNC,

		/** Horizontal Sync (line interrupt)
		  */
		V9990_HSCAN,
	};
	
	/** IRQ types
	  */
	enum IRQType {
		VER_IRQ = 1,
		HOR_IRQ = 2,
		CMD_IRQ = 4,
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

	IRQHelper irq;

	/** Status port (P#5)
	  */
	byte status;

	/** Interrupt flag (P#6)
	  */   
	byte pendingIRQs;

	/** Registers
	  */ 
	byte regs[0x40];
	byte regSelect;

	/** VRAM
	  */
	std::auto_ptr<V9990VRAM> vram;

	/** Command Engine
	  */
	std::auto_ptr<V9990CmdEngine> cmdEngine;

	/** Renderer
	  */
	std::auto_ptr<V9990Renderer> renderer;

	/** Palette
	  */
	byte palette[256];

	/** Is PAL timing active?  False means NTSC timing
	  */
	bool palTiming;

	/** Emulation time when this frame was started (VSYNC)
	  */
	Clock<V9990DisplayTiming::UC_TICKS_PER_SECOND> frameStartTime;

	/** Store display mode because it's queried a lot
	  */ 
	V9990DisplayMode mode;

	/** Time of the last set HSCAN sync point
	  */
	EmuTime hScanSyncTime;
	
	// --- methods ----------------------------------------------------

	/** Get VRAM read or write address from V9990 registers
	  * @param base  VRAM_READ_ADDRESS_0 or VRAM_WRITE_ADDRESS_0
	  * @returns     VRAM read or write address
	  */
	inline unsigned getVRAMAddr(RegisterId base) const;
	
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
	byte readRegister(byte reg, const EmuTime& time);

	/** Write V9990 register value
	  * @param reg   Register to write to
	  * @param val   Value to write
	  * @param time  Moment in emulated time to write register
	  */
	void writeRegister(byte reg, byte val, const EmuTime& time);

	/** Write V9990 palette register
	  * @param reg   Register to write to
	  * @param val   Value to write
	  * @param time  Moment in emulated time to write register
	  */
	void writePaletteRegister(byte reg, byte val, const EmuTime& time);
	
	/** Create a new renderer.
	  * @param time  Moment in emulated time to create the renderer
	  */
	void createRenderer(const EmuTime& time);

	/** Start a new frame. 
	  * @param time  Moment in emulated time to start the frame
	  */
	void frameStart(const EmuTime& time);
	
	/** Raise an IRQ
	  * @param irqType  Type of IRQ
	  */ 
	void raiseIRQ(IRQType irqType);
	
	/** Precalculate the display mode
	  */ 
	void calcDisplayMode();

	/** Calculate the moment in time the next line interrupt will occur
	  * @param time The current time
	  * @result Timestamp for next hor irq
	  */
	void scheduleHscan(const EmuTime& time);
};

} // namespace openmsx

#endif
