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

class CliConnection;

class GlobalCliComm : public CliComm, private noncopyable
{
public:
	GlobalCliComm();
	virtual ~GlobalCliComm();

	static const char* const* getUpdateStrs();

	void addConnection(std::auto_ptr<CliConnection> connection);
	void setXMLOutput();

	// CliComm
	virtual void log(LogLevel level, const std::string& message);
	virtual void update(UpdateType type, const std::string& name,
	                    const std::string& value);

private:
	void updateHelper(UpdateType type, const std::string& machine,
	                  const std::string& name, const std::string& value);

	std::map<std::string, std::string> prevValues[NUM_UPDATES];

	typedef std::vector<CliConnection*> Connections;
	Connections connections;
	Semaphore sem; // lock access to connections member
	bool xmlOutput;

	friend class MSXCliComm;
};

} // namespace openmsx

#endif
