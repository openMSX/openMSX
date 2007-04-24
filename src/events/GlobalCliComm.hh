// $Id$

#ifndef GLOBALCLICOMM_HH
#define GLOBALCLICOMM_HH

#include "CliComm.hh"
#include "EventListener.hh"
#include "Semaphore.hh"
#include "noncopyable.hh"
#include <map>
#include <string>
#include <vector>
#include <memory>

namespace openmsx {

class EventDistributor;
class GlobalCommandController;
class CliConnection;
class UpdateCmd;

class GlobalCliComm : public CliComm, private EventListener, private noncopyable
{
public:
	GlobalCliComm(GlobalCommandController& commandController,
	        EventDistributor& eventDistributor);
	virtual ~GlobalCliComm();

	void addConnection(std::auto_ptr<CliConnection> connection);
	void startInput(const std::string& option);

	// CliComm
	virtual void log(LogLevel level, const std::string& message);
	virtual void update(UpdateType type, const std::string& name,
	                    const std::string& value);

private:
	void update(UpdateType type, const std::string& machine,
	            const std::string& name, const std::string& value);

	// EventListener
	virtual bool signalEvent(shared_ptr<const Event> event);

	const std::auto_ptr<UpdateCmd> updateCmd;

	std::map<std::string, std::map<std::string, std::string> >
		prevValues[NUM_UPDATES];

	GlobalCommandController& commandController;
	EventDistributor& eventDistributor;

	typedef std::vector<CliConnection*> Connections;
	Connections connections;
	Semaphore sem; // lock access to connections member
	bool xmlOutput;

	friend class MSXCliComm;
};

} // namespace openmsx

#endif
