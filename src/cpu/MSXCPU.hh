// $Id$

#ifndef __MSXCPU_HH__
#define __MSXCPU_HH__

#include "DebugInterface.hh"
#include "Z80.hh"
#include "R800.hh"

namespace openmsx {

class Scheduler;
class MSXCPUInterface;

class MSXCPU : public DebugInterface
{
public:
	enum CPUType { CPU_Z80, CPU_R800 };

	/**
	 * This is a singleton class. This method returns a reference
	 * to the single instance of this class.
	 */
	static MSXCPU* instance();

	virtual void reset(const EmuTime &time);
	
	const EmuTime& getCurrentTime() const;
	void setActiveCPU(CPUType cpu);
	
	/**
	 * Invalidate the CPU its cache for the interval 
	 * [start, start+num*CACHE_LINE_SIZE)
	 * For example MSXMemoryMapper and MSXGameCartrigde need to call this
	 * method when a 'memory switch' occurs.
	 */
	void invalidateCache(word start, int num);

	/**
	 * This method raises an interrupt. A device may call this
	 * method more than once. If the device wants to lower the
	 * interrupt again it must call the lowerIRQ() method exactly as
	 * many times.
	 * Before using this method take a look at IRQHelper
	 */
	void raiseIRQ();

	/**
	 * This methods lowers the interrupt again. A device may never
	 * call this method more often than it called the method
	 * raiseIRQ().
	 * Before using this method take a look at IRQHelper
	 */
	void lowerIRQ();
	
	/**
	 * Send wait states to the CPU
	 */
	void wait(const EmuTime& time);

	/**
	 * Is the R800 currently active
	 */
	bool isR800Active();
	
	// DebugInterface
	virtual dword getDataSize() const;
	virtual const string getRegisterName(dword regNr) const;
	virtual dword getRegisterNumber(const string& regName) const;
	virtual byte readDebugData (dword address) const;
	virtual const string& getDeviceName() const;

	void setInterface(MSXCPUInterface* interf);

private:
	MSXCPU();
	virtual ~MSXCPU();
	
	// only for Scheduler
	void executeUntilTarget(const EmuTime& time);
	void setTargetTime(const EmuTime& time);
	const EmuTime& getTargetTime() const;
	void init(Scheduler* scheduler);
	friend class Scheduler;

	Z80 z80;
	R800 r800;

	CPU* activeCPU;
};

} // namespace openmsx
#endif //__MSXCPU_HH__
