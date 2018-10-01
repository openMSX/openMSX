#ifndef PROBE_HH
#define PROBE_HH

#include "Subject.hh"
#include "strCat.hh"
#include <string>

namespace openmsx {

class Debugger;

class ProbeBase : public Subject<ProbeBase>
{
public:
	const std::string& getName() const { return name; }
	const std::string& getDescription() const { return description; }
	virtual std::string getValue() const = 0;

protected:
	ProbeBase(Debugger& debugger, std::string name,
	          std::string description);
	~ProbeBase();

private:
	Debugger& debugger;
	const std::string name;
	const std::string description;
};


template<typename T> class Probe final : public ProbeBase
{
public:
	Probe(Debugger& debugger, std::string name,
	      std::string description, T t);

	const T& operator=(const T& newValue) {
		if (value != newValue) {
			value = newValue;
			notify();
		}
		return value;
	}

	operator const T&() const {
		return value;
	}

private:
	std::string getValue() const override;

	T value;
};

template<typename T>
Probe<T>::Probe(Debugger& debugger_, std::string name_,
                std::string description_, T t)
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
	Probe(Debugger& debugger, std::string name, std::string description);
	void signal();

private:
	std::string getValue() const override;
};

} // namespace openmsx

#endif
