// $Id$

#ifndef __MSXCPU_HH__
#define __MSXCPU_HH__

#include "Debuggable.hh"
#include "InfoTopic.hh"
#include "SettingListener.hh"
#include "EmuTime.hh"
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class MSXCPUInterface;
class InfoCommand;
class Debugger;
class BooleanSetting;
class CPU;
class Z80TYPE;
class R800TYPE;
template <typename T> class CPUCore;

class MSXCPU : private Debuggable, private SettingListener
{
public:
	enum CPUType { CPU_Z80, CPU_R800 };

	static MSXCPU& instance();

	virtual void reset(const EmuTime& time);

	/**
	 * Switch between Z80 / R800
	 */
	void setActiveCPU(CPUType cpu);
	
	/**
	 * Invalidate the CPU its cache for the interval [start, start + size)
	 * For example MSXMemoryMapper and MSXGameCartrigde need to call this
	 * method when a 'memory switch' occurs.
	 */
	void invalidateMemCache(word start, unsigned size);

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
	
	void setInterface(MSXCPUInterface* interf);

	// Debuggable
	virtual unsigned getSize() const;
	virtual const std::string& getDescription() const;
	virtual byte read(unsigned address);
	virtual void write(unsigned address, byte value);

	// Breakpoint stuff
	std::string doStep();
	std::string doContinue();
	std::string doBreak();
	std::string setBreakPoint(word addr);
	std::string removeBreakPoint(word addr);
	std::string listBreakPoints() const;

private:
	MSXCPU();
	virtual ~MSXCPU();
	
	// only for MSXMotherBoard
	void execute();
	void setMotherboard(MSXMotherBoard* motherboard);
	friend class MSXMotherBoard;
	
	void wait(const EmuTime& time);
	friend class VDPIODelay;

	/**
	 * The time returned by this method is not safe to use for Scheduler
	 * or IO related stuff (because it can sometimes already be higher then
	 * still to be scheduled sync points). Use Scheduler::getCurrentTime()
	 * instead.
	 * TODO is this comment still true?
	 */
	const EmuTime& getCurrentTime() const;
	
	// SettingListener
	void update(const Setting* setting);

	const std::auto_ptr<BooleanSetting> traceSetting;
	const std::auto_ptr<CPUCore<Z80TYPE> > z80;
	const std::auto_ptr<CPUCore<R800TYPE> > r800;

	CPU* activeCPU;

	class TimeInfoTopic : public InfoTopic {
	public:
		TimeInfoTopic(MSXCPU& parent);
		virtual void execute(const std::vector<CommandArgument>& tokens,
		                     CommandArgument& result) const;
		virtual std::string help (const std::vector<std::string>& tokens) const;
	private:
		MSXCPU& parent;
	} timeInfo;
	EmuTime reference;

	InfoCommand& infoCmd;
	Debugger& debugger;
};

} // namespace openmsx
#endif //__MSXCPU_HH__
