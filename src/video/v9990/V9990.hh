// $Id$

#ifndef __V9990_HH__
#define __V9990_HH__

#include <string>

#include "openmsx.hh"
#include "Schedulable.hh"
#include "MSXDevice.hh"
#include "EmuTime.hh"
#include "Debuggable.hh"
#include "IRQHelper.hh"
#include "Command.hh"

#include "V9990VRAM.hh"

using std::string;
using std::auto_ptr;

namespace openmsx {

class V9990VRAM;

/**
  * This class implements the V9990 video chip, as used in the
  * Graphics9000 module, made by Sunrise.
  */
class V9990 : public MSXDevice,
              private Schedulable
{
public:
	V9990(const XMLElement& config, const EmuTime& time);
	virtual ~V9990();

	// Schedulable interface:
	virtual void executeUntil(const EmuTime& time, int userData);
	virtual const string& schedName() const;

	// MSXDevice interface:
	virtual byte readIO(byte port, const EmuTime& time);
	virtual void writeIO(byte port, byte value, const EmuTime& time);
	virtual void reset(const EmuTime& time);

private:
	// Debuggable:
	class V9990RegDebug : public Debuggable {
	public:
		V9990RegDebug(V9990& parent);
		virtual unsigned getSize() const;
		virtual const string& getDescription() const;
		virtual byte read(unsigned address);
		virtual void write(unsigned address, byte value);
	private:
		V9990& parent;
	} v9990RegDebug;

	// Commands:
	class V9990RegsCmd : public SimpleCommand {
	public:
		V9990RegsCmd(V9990& v9990);
		virtual string execute(const vector<string>& tokens);
		virtual string help(const vector<string>& tokens) const;
	private:
		V9990& v9990;
	} v9990RegsCmd;

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
		CMD_PARAM_SRC_ADDRESS_0,
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

	byte regs[0x40];
	byte regSelect;

	/** VRAM
	  */
	auto_ptr<V9990VRAM> vram;
 
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
	
	/** Palette
	  */
	byte palette[256];

	/** IRQ
	  */
	enum IRQType {
		VER_IRQ = 1,
		HOR_IRQ = 2,
		CMD_IRQ = 4,
	};

	void raiseIRQ(IRQType irqType);
	
	IRQHelper irq;
	byte pendingIRQs;
};

} // namespace openmsx

#endif
