// $Id$

#ifndef GLOBALCLICOMM_HH
#define GLOBALCLICOMM_HH

#include "CliComm.hh"
#include "Semaphore.hh"
#include "noncopyable.hh"
#include <map>
#include <string>
#include <vector>
#include <memory>

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
	virtual void log(LogLevel level, const std::string& message);
	virtual void update(UpdateType type, const std::string& name,
	                    const std::string& value);

private:
	void updateHelper(UpdateType type, const std::string& machine,
	                  const std::string& name, const std::string& value);

	std::map<std::string, std::string> prevValues[NUM_UPDATES];

	typedef std::vector<CliListener*> Listeners;
	Listeners listeners;
	Semaphore sem; // lock access to listeners member
	bool delivering;

	friend class MSXCliComm;
};

} // namespace openmsx

#endif
