#ifndef IRQHELPER_HH
#define IRQHELPER_HH

#include "Probe.hh"
#include "noncopyable.hh"
#include <string>

namespace openmsx {

class MSXMotherBoard;
class MSXCPU;

/** Helper class for doing interrupt request (IRQ) administration.
  * IRQ is either enabled or disabled; when enabled it contributes
  * one to the CPU IRQ count, when disabled zero.
  * Calling set() in enabled state does nothing;
  * neither does calling reset() in disabled state.
  */

// policy class for IRQ source
class IRQSource
{
protected:
	explicit IRQSource(MSXCPU& cpu);
	void raise();
	void lower();
private:
	MSXCPU& cpu;
};

// policy class for NMI source
class NMISource
{
protected:
	explicit NMISource(MSXCPU& cpu);
	void raise();
	void lower();
private:
	MSXCPU& cpu;
};

// policy class that can dynamically switch between IRQ and NMI
class DynamicSource
{
public:
	enum IntType { IRQ, NMI };
	void setIntType(IntType type_) { type = type_; }
protected:
	explicit DynamicSource(MSXCPU& cpu);
	void raise();
	void lower();
private:
	MSXCPU& cpu;
	IntType type;
};

// generic implementation
template <typename SOURCE> class IntHelper : public SOURCE, private noncopyable
{
public:
	/** Create a new IntHelper.
	  * Initially there is no interrupt request on the bus.
	  */
	explicit IntHelper(MSXMotherBoard& motherboard, const std::string& name);

	/** Destroy this IntHelper.
	  * Resets interrupt request if it is active.
	  */
	~IntHelper() {
		reset();
	}

	/** Set the interrupt request on the bus.
	  */
	inline void set() {
		if (!request) {
			request = true;
			SOURCE::raise();
		}
	}

	/** Reset the interrupt request on the bus.
	  */
	inline void reset() {
		if (request) {
			request = false;
			SOURCE::lower();
		}
	}

	/** Get the interrupt state.
	  * @return true iff interrupt request is active.
	  */
	inline bool getState() const {
		return request;
	}

	template<typename Archive>
	void serialize(Archive& ar, unsigned /*version*/);

private:
	Probe<bool> request;
};

// convenience types
typedef IntHelper<IRQSource> IRQHelper;
//typedef IntHelper<NMISource> NMIHelper;
//typedef IntHelper<DynamicSource> IRQNMIHelper;

} // namespace openmsx

#endif
