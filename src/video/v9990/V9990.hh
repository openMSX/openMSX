
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

using std::string;

namespace openmsx {

class V9990 : public MSXIODevice,
              private Schedulable
{
public:
	/** Constructor
	  */
	V9990(Config* config, const EmuTime& time);
	
	/** Destructor
	  */ 
	virtual ~V9990();

	/** Schedulable interface
	  */ 
	virtual void executeUntil(const EmuTime& time, int userData) throw();
	virtual const string& schedName() const;

	/** MSXIODevice interface
	  */  
	virtual byte readIO(byte port, const EmuTime &time);
	virtual void writeIO(byte port, byte value, const EmuTime &time);

	/** MSXDevice interface
	  */ 
	virtual void reset(const EmuTime &time);
	

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

	class V9990PortDebug : public Debuggable {
	public:
		V9990PortDebug(V9990& parent);
		virtual unsigned getSize() const;
		virtual const string& getDescription() const;
		virtual byte read(unsigned address);
		virtual void write(unsigned address, byte value);
	private:
		V9990& parent;
	} v9990PortDebug;

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

	byte ports[16];
	
	/** VDP Registers
	  */
	byte registers[54];

	virtual void changeRegister(byte reg, byte val, const EmuTime &time);
};

} // namespace openmsx

#endif
