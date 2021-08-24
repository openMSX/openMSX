#ifndef PROBE_HH
#define PROBE_HH

#include "static_string_view.hh"
#include "Subject.hh"
#include "strCat.hh"
#include <string>

namespace openmsx {

class Debugger;

class ProbeBase : public Subject<ProbeBase>
{
public:
	ProbeBase(const ProbeBase&) = delete;
	ProbeBase& operator=(const ProbeBase&) = delete;

	[[nodiscard]] const std::string& getName() const { return name; }
	[[nodiscard]] std::string_view getDescription() const { return description; }
	[[nodiscard]] virtual std::string getValue() const = 0;

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
	[[nodiscard]] std::string getValue() const override;

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
std::string Probe<T>::getValue() const
{
	return strCat(value);
}

// specialization for void
template<> class Probe<void> final : public ProbeBase
{
public:
	Probe(Debugger& debugger, std::string name, static_string_view description);
	void signal();

private:
	[[nodiscard]] std::string getValue() const override;
};

} // namespace openmsx

#endif
