// $Id$

#ifndef __CPU_HH__
#define __CPU_HH__

#include <set>
#include <string>
#include <memory>
#include "build-info.hh"
#include "openmsx.hh"
#include "EmuTime.hh"
#include "CPUTables.hh"
#include "BooleanSetting.hh"
#include "IntegerSetting.hh"
#include "SettingListener.hh"

using std::multiset;
using std::string;
using std::auto_ptr;

namespace openmsx {

class BooleanSetting;
class MSXCPUInterface;
class Scheduler;
class MSXMotherBoard;

typedef signed char offset;

template <bool bigEndian> struct z80regpair_8bit;
template <> struct z80regpair_8bit<false> { byte l, h; };
template <> struct z80regpair_8bit<true>  { byte h, l; };

class CPU : public CPUTables, private SettingListener
{
friend class MSXCPU;
public:
	typedef union {
		z80regpair_8bit<OPENMSX_BIGENDIAN> B;
		word w;
	} z80regpair;

	class CPURegs {
	public:
		z80regpair AF,  BC,  DE,  HL, IX, IY, PC, SP;
		z80regpair AF2, BC2, DE2, HL2;
		bool nextIFF1, IFF1, IFF2, HALT;
		byte IM, I;
		byte R, R2;	// refresh = R&127 | R2&128

		void ei() { IFF1 = nextIFF1 = IFF2 = true; }
		void di() { IFF1 = nextIFF1 = IFF2 = false; }
	};

	virtual ~CPU();
	
	void setMotherboard(MSXMotherBoard* motherboard);
	void setInterface(MSXCPUInterface* interf);

	/**
	 * Reset the CPU.
	 */
	void reset(const EmuTime& time);

	/**
	 * TODO
	 */
	virtual void execute() = 0;

	/**
	 * TODO
	 */
	void exitCPULoop();

	/**
	 * Sets the CPU its current time.
	 * This is used to 'warp' a CPU when you switch between Z80/R800.
	 */
	void advance(const EmuTime& time);

	/**
	 * Returns the CPU its current time.
	 */
	const EmuTime& getCurrentTime() const;

	/**
	 * Wait 
	 */
	void wait(const EmuTime& time);

	/**
	 * Read a byte from memory. If possible the byte is read from
	 * cache, otherwise the readMem() method of MSXCPUInterface is used.
	 */
	byte readMem(word address);

	/**
	 * Write a byte from memory. If possible the byte is written to
	 * cache, otherwise the writeMem() method of MSXCPUInterface is used.
	 */
	void writeMem(word address, byte value);

	/**
	 * Invalidate the CPU its cache for the interval
	 * [start, start+num*CACHE_LINE_SIZE).
	 */
	void invalidateCache(word start, int num);

	/**
	 * Raises the maskable interrupt count.
	 * Devices should call MSXCPU::raiseIRQ instead, or use the IRQHelper class.
	 */
	void raiseIRQ();

	/**
	 * Lowers the maskable interrupt count.
	 * Devices should call MSXCPU::lowerIRQ instead, or use the IRQHelper class.
	 */
	void lowerIRQ();

	/**
	 * Raises the non-maskable interrupt count.
	 * Devices should call MSXCPU::raiseNMI instead, or use the IRQHelper class.
	 */
	void raiseNMI();

	/**
	 * Lowers the non-maskable interrupt count.
	 * Devices should call MSXCPU::lowerNMI instead, or use the IRQHelper class.
	 */
	void lowerNMI();

	// cache constants
	static const int CACHE_LINE_BITS = 8;	// 256 bytes
	static const int CACHE_LINE_SIZE = 1 << CACHE_LINE_BITS;
	static const int CACHE_LINE_NUM  = 0x10000 / CACHE_LINE_SIZE;
	static const int CACHE_LINE_LOW  = CACHE_LINE_SIZE - 1;
	static const int CACHE_LINE_HIGH = 0xffff - CACHE_LINE_LOW;

	/**
	 * Change the clock freq.
	 */
	void setFreq(unsigned freq);
	
protected:
	CPU(const string& name, int defaultFreq,
	    const BooleanSetting& traceSetting);

	/**
	  * Execute further than strictly requested by
	  * caller of executeUntilTarget() or wait(). Typically used to
	  * be able to emulate complete instruction (part of it passed
	  * the target time border).
	  */
	/*void extendTarget(const EmuTime& time);*/

	/**
	 * Set to true when there was a rising edge on the NMI line
	 * (rising = non-active -> active).
	 * Set to false when the CPU jumps to the NMI handler address.
	 */
	bool nmiEdge;

	// state machine variables
	CPURegs R;
	int slowInstructions;
	int NMIStatus;
	int IRQStatus;
	bool exitLoop;

	z80regpair memptr;
	offset ofst;

	MSXCPUInterface* interface;
	DynamicClock clock;

	// memory cache
	const byte* readCacheLine[CACHE_LINE_NUM];
	byte* writeCacheLine[CACHE_LINE_NUM];
	bool readCacheTried [CACHE_LINE_NUM];
	bool writeCacheTried[CACHE_LINE_NUM];

	// debugger
	void doBreak2();
	void doStep();
	void doContinue();
	void doBreak();

	// TODO document why static 
	static multiset<word> breakPoints;
	static bool breaked;
	static bool continued;
	static bool step;
	
	Scheduler& scheduler;

public:
	byte debugmemory[65536];

private:
	virtual void update(const SettingLeafNode* setting);
	
	// dynamic freq
	BooleanSetting freqLocked;
	IntegerSetting freqValue;
	unsigned freq;
	
	MSXMotherBoard* motherboard;

protected:
	const BooleanSetting& traceSetting;
};

} // namespace openmsx
#endif //__CPU_HH__
