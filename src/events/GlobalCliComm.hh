#ifndef GLOBALCLICOMM_HH
#define GLOBALCLICOMM_HH

#include "CliComm.hh"
#include "Semaphore.hh"
#include "StringMap.hh"
#include "noncopyable.hh"
#include <vector>

namespace openmsx {

class CliListener;

class GlobalCliComm : public CliComm, private noncopyable
{
public:
	GlobalCliComm();
	virtual ~GlobalCliComm();

	void addListener(CliListener* listener);
	void removeListener(CliListener* listener);

	// CliComm
	virtual void log(LogLevel level, string_ref message);
	virtual void update(UpdateType type, string_ref name,
	                    string_ref value);

private:
	void updateHelper(UpdateType type, string_ref machine,
	                  string_ref name, string_ref value);

	StringMap<std::string> prevValues[NUM_UPDATES];

	std::vector<CliListener*> listeners;
	Semaphore sem; // lock access to listeners member
	bool delivering;

	friend class MSXCliComm;
};

} // namespace openmsx

#endif
