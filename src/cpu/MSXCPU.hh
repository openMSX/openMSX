// $Id$

#ifndef __MSXCPU_HH__
#define __MSXCPU_HH__

#include "Debuggable.hh"
#include "InfoTopic.hh"
#include "BooleanSetting.hh"
#include "Z80.hh"
#include "R800.hh"

namespace openmsx {

class MSXMotherBoard;
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
	 * This method raises a maskable interrupt. A device may call this
	 * method more than once. If the device wants to lower the
	 * interrupt again it must call the lowerIRQ() method exactly as
	 * many times.
	 * Before using this method take a look at IRQHelper.
	 */
	void raiseIRQ();

	/**
	 * This methods lowers the maskable interrupt again. A device may never
	 * call this method more often than it called the method
	 * raiseIRQ().
	 * Before using this method take a look at IRQHelper.
	 */
	void lowerIRQ();

	/**
	 * This method raises a non-maskable interrupt. A device may call this
	 * method more than once. If the device wants to lower the
	 * interrupt again it must call the lowerNMI() method exactly as
	 * many times.
	 * Before using this method take a look at IRQHelper.
	 */
	void raiseNMI();

	/**
	 * This methods lowers the non-maskable interrupt again. A device may never
	 * call this method more often than it called the method
	 * raiseNMI().
	 * Before using this method take a look at IRQHelper.
	 */
	void lowerNMI();

	/**
	 * TODO
	 */
	void exitCPULoop();
	
	/**
	 * Is the R800 currently active
	 */
	bool isR800Active();

	/**
	 * Switch the Z80 clock freq
	 */
	void setZ80Freq(unsigned freq);
	
	// Debuggable
	virtual unsigned getSize() const;
	virtual const string& getDescription() const;
	virtual byte read(unsigned address);
	virtual void write(unsigned address, byte value);

	void setInterface(MSXCPUInterface* interf);

	string doStep();
	string doContinue();
	string doBreak();
	string setBreakPoint(word addr);
	string removeBreakPoint(word addr);
	string listBreakPoints() const;

private:
	MSXCPU();
	virtual ~MSXCPU();
	
	// only for MSXMotherBoard
	void execute();
	void setMotherboard(MSXMotherBoard* motherboard);
	friend class MSXMotherBoard;
	
	void wait(const EmuTime& time);
	friend class VDPIODelay;

	BooleanSetting traceSetting;
	Z80 z80;
	R800 r800;

	CPU* activeCPU;

	class TimeInfoTopic : public InfoTopic {
	public:
		TimeInfoTopic(MSXCPU& parent);
		virtual void execute(const vector<CommandArgument>& tokens,
		                     CommandArgument& result) const;
		virtual string help (const vector<string>& tokens) const;
	private:
		MSXCPU& parent;
	} timeInfo;
	EmuTime reference;

	InfoCommand& infoCmd;
	Debugger& debugger;
};

} // namespace openmsx
#endif //__MSXCPU_HH__
