// $Id$

#ifndef __MSXCPU_HH__
#define __MSXCPU_HH__

#include "Debuggable.hh"
#include "InfoTopic.hh"
#include "Z80.hh"
#include "R800.hh"

namespace openmsx {

class Scheduler;
class MSXCPUInterface;
class InfoCommand;
class Debugger;

class MSXCPU : private Debuggable
{
public:
	enum CPUType { CPU_Z80, CPU_R800 };

	static MSXCPU& instance();

	virtual void reset(const EmuTime &time);

	/**
	 * The time returned by this method is not safe to use for Scheduler
	 * or IO related stuff (because it can sometimes already be higher then
	 * still to be scheduled sync points). Use Scheduler::getCurrentTime()
	 * instead.
	 * OTOH this time is usally more accurate, so you can use this method
	 * for, for example, sound related calculations
	 */
	const EmuTime& getCurrentTimeUnsafe() const;
	
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
	
	// Debuggable
	virtual unsigned getSize() const;
	virtual const string& getDescription() const;
	virtual byte read(unsigned address);
	virtual void write(unsigned address, byte value);

	void setInterface(MSXCPUInterface* interf);

	string doStep();
	string doContinue();
	string setBreakPoint(const vector<string>& tokens);
	string removeBreakPoint(const vector<string>& tokens);
	string listBreakPoints() const;

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

	class TimeInfoTopic : public InfoTopic {
	public:
		TimeInfoTopic(MSXCPU& parent);
		virtual string execute(const vector<string>& tokens) const
			throw();
		virtual string help   (const vector<string>& tokens) const
			throw();
	private:
		MSXCPU& parent;
	} timeInfo;
	EmuTime reference;

	InfoCommand& infoCmd;
	Debugger& debugger;
};

} // namespace openmsx
#endif //__MSXCPU_HH__
