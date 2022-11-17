#ifndef MSXCPU_HH
#define MSXCPU_HH

#include "InfoTopic.hh"
#include "SimpleDebuggable.hh"
#include "Observer.hh"
#include "BooleanSetting.hh"
#include "CacheLine.hh"
#include "EmuTime.hh"
#include "TclCallback.hh"
#include "serialize_meta.hh"
#include "openmsx.hh"
#include <array>
#include <memory>
#include <span>

namespace openmsx {

class MSXMotherBoard;
class MSXCPUInterface;
class CPUClock;
class CPURegs;
class Z80TYPE;
class R800TYPE;
template<typename T> class CPUCore;
class TclObject;
class Interpreter;

class MSXCPU final : private Observer<Setting>
{
public:
	enum CPUType { CPU_Z80, CPU_R800 };

	explicit MSXCPU(MSXMotherBoard& motherboard);
	~MSXCPU();

	/** Reset CPU.
	  * Requires CPU is not in the middle of an instruction,
	  * so exitCPULoop was called and execute() method returned.
	 */
	void doReset(EmuTime::param time);

	/** Switch between Z80/R800. */
	void setActiveCPU(CPUType cpu);

	/** Sets DRAM or ROM mode (influences memory access speed for R800). */
	void setDRAMmode(bool dram);

	/** Inform CPU of bank switch. This will invalidate memory cache and
	  * update memory timings on R800. */
	void updateVisiblePage(byte page, byte primarySlot, byte secondarySlot);

	/** Invalidate the CPU its cache for the interval [start, start + size)
	  * For example MSXMemoryMapper and MSXGameCartridge need to call this
	  * method when a 'memory switch' occurs. */
	void invalidateAllSlotsRWCache(word start, unsigned size);

	/** Similar to the method above, but only invalidates one specific slot.
	  * One small tweak: lines that are in 'disallowRead/Write' are
	  * immediately marked as 'non-cacheable' instead of (first) as
	  * 'unknown'.
	  */
	void invalidateRWCache(unsigned start, unsigned size, int ps, int ss,
	                       std::span<const byte, 256> disallowRead,
	                       std::span<const byte, 256> disallowWrite);
	void invalidateRCache (unsigned start, unsigned size, int ps, int ss,
	                       std::span<const byte, 256> disallowRead,
	                       std::span<const byte, 256> disallowWrite);
	void invalidateWCache (unsigned start, unsigned size, int ps, int ss,
	                       std::span<const byte, 256> disallowRead,
	                       std::span<const byte, 256> disallowWrite);

	/** Fill the read and write cache lines for a specific slot with the
	 * specified value. Except for the lines where the corresponding
	 * 'disallow{Read,Write}' array is non-zero, those lines are marked
	 * non-cacheable.
	 * This is useful on e.g. a memory mapper bank switch because:
	 * - Marking the lines 'unknown' (as done by invalidateMemCache) is
	 *   as much work as directly setting the correct value.
	 * - Directly setting the correct value saves work later on.
	 */
	void fillRWCache(unsigned start, unsigned size, const byte* rData, byte* wData, int ps, int ss,
	                 std::span<const byte, 256> disallowRead,
	                 std::span<const byte, 256> disallowWrite);
	void fillRCache (unsigned start, unsigned size, const byte* rData, int ps, int ss,
	                 std::span<const byte, 256> disallowRead,
	                 std::span<const byte, 256> disallowWrite);
	void fillWCache (unsigned start, unsigned size, byte* wData, int ps, int ss,
	                 std::span<const byte, 256> disallowRead,
	                 std::span<const byte, 256> disallowWrite);

	/** This method raises a maskable interrupt. A device may call this
	  * method more than once. If the device wants to lower the
	  * interrupt again it must call the lowerIRQ() method exactly as
	  * many times.
	  * Before using this method take a look at IRQHelper. */
	void raiseIRQ();

	/** This methods lowers the maskable interrupt again. A device may never
	  * call this method more often than it called the method
	  * raiseIRQ().
	  * Before using this method take a look at IRQHelper. */
	void lowerIRQ();

	/** This method raises a non-maskable interrupt. A device may call this
	  * method more than once. If the device wants to lower the
	  * interrupt again it must call the lowerNMI() method exactly as
	  * many times.
	  * Before using this method take a look at IRQHelper. */
	void raiseNMI();

	/** This methods lowers the non-maskable interrupt again. A device may never
	  * call this method more often than it called the method
	  * raiseNMI().
	  * Before using this method take a look at IRQHelper. */
	void lowerNMI();

	/** Should only be used from within a MSXDevice::readMem() method.
	  * Returns true if that read was the first byte of an instruction
	  * (the Z80 M1 pin is active). This implementation is not 100%
	  * accurate, but good enough for now.
	  */
	[[nodiscard]] bool isM1Cycle(unsigned address) const;

	/** See CPUCore::exitCPULoopSync() */
	void exitCPULoopSync();
	/** See CPUCore::exitCPULoopAsync() */
	void exitCPULoopAsync();

	/** Is the R800 currently active? */
	[[nodiscard]] bool isR800Active() const { return !z80Active; }

	/** Switch the Z80 clock freq. */
	void setZ80Freq(unsigned freq);

	void setInterface(MSXCPUInterface* interface);

	void disasmCommand(Interpreter& interp,
	                   std::span<const TclObject> tokens,
	                   TclObject& result) const;

	/** (un)pause CPU. During pause the CPU executes NOP instructions
	  * continuously (just like during HALT). Used by turbor hw pause. */
	void setPaused(bool paused);

	void setNextSyncPoint(EmuTime::param time);

	void wait(EmuTime::param time);
	EmuTime waitCyclesZ80(EmuTime::param time, unsigned cycles);
	EmuTime waitCyclesR800(EmuTime::param time, unsigned cycles);

	[[nodiscard]] CPURegs& getRegisters();

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void invalidateMemCacheSlot();

	// only for MSXMotherBoard
	void execute(bool fastForward);
	friend class MSXMotherBoard;

	/** The time returned by this method is not safe to use for Scheduler
	  * or IO related stuff (because it can sometimes already be higher then
	  * still to be scheduled sync points). Use Scheduler::getCurrentTime()
	  * instead.
	  * TODO is this comment still true? */
	EmuTime::param getCurrentTime() const;

	// Observer<Setting>
	void update(const Setting& setting) noexcept override;

	template<bool READ, bool WRITE, bool SUB_START>
	void setRWCache(unsigned start, unsigned size, const byte* rData, byte* wData, int ps, int ss,
	                std::span<const byte, 256> disallowRead, std::span<const byte, 256> disallowWrite);

private:
	MSXMotherBoard& motherboard;
	BooleanSetting traceSetting;
	TclCallback diHaltCallback;
	const std::unique_ptr<CPUCore<Z80TYPE>> z80;
	const std::unique_ptr<CPUCore<R800TYPE>> r800; // can be nullptr

	std::array<std::array<const byte*, CacheLine::NUM>, 16> slotReadLines;
	std::array<std::array<      byte*, CacheLine::NUM>, 16> slotWriteLines;
	std::array<byte, 4> slots; // active slot for page (= 4 * primSlot + secSlot)

	struct TimeInfoTopic final : InfoTopic {
		explicit TimeInfoTopic(InfoCommand& machineInfoCommand);
		void execute(std::span<const TclObject> tokens,
			     TclObject& result) const override;
		[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
	} timeInfo;

	class CPUFreqInfoTopic final : public InfoTopic {
	public:
		CPUFreqInfoTopic(InfoCommand& machineInfoCommand,
				 const std::string& name, CPUClock& clock);
		void execute(std::span<const TclObject> tokens,
			     TclObject& result) const override;
		[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
	private:
		CPUClock& clock;
	};
	CPUFreqInfoTopic                        z80FreqInfo;  // always present
	const std::unique_ptr<CPUFreqInfoTopic> r800FreqInfo; // can be nullptr

	struct Debuggable final : SimpleDebuggable {
		explicit Debuggable(MSXMotherBoard& motherboard);
		[[nodiscard]] byte read(unsigned address) override;
		void write(unsigned address, byte value) override;
	} debuggable;

	EmuTime reference;
	bool z80Active;
	bool newZ80Active;

	MSXCPUInterface* interface = nullptr; // only used for debug
};
SERIALIZE_CLASS_VERSION(MSXCPU, 2);

} // namespace openmsx

#endif
