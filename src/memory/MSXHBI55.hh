// $Id$

/**
 * Submitted By: Albert Beevendorp (bifimsx)
 *
 * Some Sony MSX machines have built-in firmware 
 * containing an address 'book', memo and scheduler and a 
 * few of these can store these data on a data cartridge. 
 * 
 * In Basic it's possible to use it with the "CAT:" device.
 * 
 * I'm not sure if there are more types, though the HBI-55 
 * is a 4KB SRAM containing cartridge accessed by I/O 
 * ports connected to a 8255 chip:
 * 
 * B0 = LSB address
 * B1 = MSB address (D7-D6 = 01 write, 11 read)
 * B2 = Data port
 * B3 = Access control
 * 
 * Sample basic program:
 * 
 * 10  FOR I=0 TO 4095: OUT &HB3,128: OUT &HB0,I MOD 256:
 *     OUT &HB1,64 OR I\256: OUT &HB2,I MOD 256: NEXT I
 * 20  FOR I=0 TO 4095:OUT &HB3,128-9*(I<>0): OUT &HB0,I MOD 256: 
 *     OUT &HB1,192 OR I\256: IF I MOD 256=INP(&HB2) THEN NEXT
 *     ELSE PRINT "Error comparing byte:";I: END
 * 30  PRINT "Done!"
 */

#ifndef __MSXHBI55_HH__
#define __MSXHBI55_HH__

#include "MSXIODevice.hh"
#include "I8255.hh"
#include "SRAM.hh"


namespace openmsx {

class MSXHBI55 : public MSXIODevice, public I8255Interface
{
	// MSXDevice
	public:
		MSXHBI55(Device *config, const EmuTime &time); 
		virtual ~MSXHBI55(); 

		virtual void reset(const EmuTime &time);
		virtual byte readIO(byte port, const EmuTime &time);
		virtual void writeIO(byte port, byte value, const EmuTime &time);

	private:
		I8255 *i8255;
	
	// I8255Interface
	public:
		virtual byte readA(const EmuTime &time);
		virtual byte readB(const EmuTime &time);
		virtual nibble readC0(const EmuTime &time);
		virtual nibble readC1(const EmuTime &time);
		virtual void writeA(byte value, const EmuTime &time);
		virtual void writeB(byte value, const EmuTime &time);
		virtual void writeC0(nibble value, const EmuTime &time);
		virtual void writeC1(nibble value, const EmuTime &time);
	
	private:
		SRAM sram;
		word address;
		byte mode;
};

} // namespace openmsx

#endif
