
// $Id$ //

/*
 * This class implements the V9990 video chip, as used in the
 * Graphics9000 module, made by Sunrise.
 */

#ifndef __V9990_HH__
#define __V9990_HH__

#include <string>

#include "openmsx.hh"
#include "Schedulable.hh"
#include "MSXIODevice.hh"
#include "EmuTime.hh"
#include "Debuggable.hh"
#include "IRQHelper.hh"

using std::string;

namespace openmsx {

class V9990 : public MSXIODevice,
              private Schedulable
{
public:
	V9990(const XMLElement& config, const EmuTime& time);
	virtual ~V9990();

	/** Schedulable interface
	  */ 
	virtual void executeUntil(const EmuTime& time, int userData);
	virtual const string& schedName() const;

	/** MSXIODevice interface
	  */  
	virtual byte readIO(byte port, const EmuTime& time);
	virtual void writeIO(byte port, byte value, const EmuTime& time);

	/** MSXDevice interface
	  */ 
	virtual void reset(const EmuTime& time);
	

private:
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

	class V9990VRAMDebug : public Debuggable {
	public:
		V9990VRAMDebug(V9990& parent);
		virtual unsigned getSize() const;
		virtual const string& getDescription() const;
		virtual byte read(unsigned address);
		virtual void write(unsigned address, byte value);
	private:
		V9990& parent;
	} v9990VRAMDebug;

	/** The I/O Ports
	  */  
	enum PortId {
		VRAM_DATA,
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
	
	/** VDP Registers
	  */
	enum RegId {
		VRAM_WR_ADR0 =  0,
		VRAM_WR_ADR1 =  1,
		VRAM_WR_ADR2 =  2,
		VRAM_RD_ADR0 =  3,
		VRAM_RD_ADR1 =  4,
		VRAM_RD_ADR2 =  5,
		INT_ENABLE   =  9,
		PALETTE_PTR  = 14,
		/* ... */
	};
	byte regs[0x40];
	byte regSelect;

	inline unsigned getVRAMAddr(RegId base) const;
	inline void setVRAMAddr(RegId base, unsigned addr);

	byte readRegister(byte reg, const EmuTime& time);
	void writeRegister(byte reg, byte val, const EmuTime& time);
	byte readVRAM(unsigned addr, const EmuTime& time);
	void writeVRAM(unsigned addr, byte value, const EmuTime& time);

	/** VRAM
	  */
	byte* vram;

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
