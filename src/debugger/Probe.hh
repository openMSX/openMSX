#ifndef PROBE_HH
#define PROBE_HH

#include "TclObject.hh"
#include "TraceValue.hh"

#include "Subject.hh"
#include "static_string_view.hh"

#include <string>
#include <type_traits>

namespace openmsx {

class Debugger;

class ProbeBase : public Subject<ProbeBase>
{
public:
	ProbeBase(const ProbeBase&) = delete;
	ProbeBase(ProbeBase&&) = delete;
	ProbeBase& operator=(const ProbeBase&) = delete;
	ProbeBase& operator=(ProbeBase&&) = delete;

	[[nodiscard]] const std::string& getName() const { return name; }
	[[nodiscard]] std::string_view getDescription() const { return description; }
	[[nodiscard]] virtual TclObject getValue() const = 0;
	[[nodiscard]] virtual TraceValue getTraceValue() const = 0;

protected:
	ProbeBase(Debugger& debugger, std::string name,
	          static_string_view description);
	~ProbeBase();

private:
	Debugger& debugger;
	const std::string name;
	const static_string_view description;
};


template<typename T> class Probe final : public ProbeBase
{
public:
	Probe(Debugger& debugger, std::string name,
	      static_string_view description, T t);

	Probe& operator=(const T& newValue) {
		if (value != newValue) {
			value = newValue;
			notify();
		}
		return *this;
	}

	[[nodiscard]] operator const T&() const {
		return value;
	}

private:
	[[nodiscard]] TclObject getValue() const override;
	[[nodiscard]] TraceValue getTraceValue() const override;

	T value;
};

template<typename T>
Probe<T>::Probe(Debugger& debugger_, std::string name_,
                static_string_view description_, T t)
	: ProbeBase(debugger_, std::move(name_), std::move(description_))
	, value(std::move(t))
{
}

template<typename T>
TclObject Probe<T>::getValue() const
{
	return TclObject(value);
}

template<typename T>
TraceValue Probe<T>::getTraceValue() const
{
	// Note: T=void is handled by full specialization below
	if constexpr (std::is_same_v<T, bool>) {
		return TraceValue{uint64_t(value)};
	} else if constexpr (std::is_integral_v<T>) {
		return TraceValue{uint64_t(value)};
	} else if constexpr (std::is_floating_point_v<T>) {
		return TraceValue{double(value)};
	} else {
		return TraceValue{zstring_view(value)};
	}
}

// specialization for void
template<> class Probe<void> final : public ProbeBase
{
public:
	Probe(Debugger& debugger, std::string name, static_string_view description);
	void signal() const;

private:
	[[nodiscard]] TclObject getValue() const override;
	[[nodiscard]] TraceValue getTraceValue() const override;
};

} // namespace openmsx

#endif
