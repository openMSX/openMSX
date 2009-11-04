// $Id$

#ifndef STATECHANGELISTENER_HH
#define STATECHANGELISTENER_HH

#include "shared_ptr.hh"

namespace openmsx {

class StateChange;

class StateChangeListener
{
public:
	/** This method gets called when an event you are subscribed to occurs.
	  */
	virtual void signalStateChange(shared_ptr<const StateChange> event) = 0;

protected:
	StateChangeListener() {}
	virtual ~StateChangeListener() {}
};

} // namespace openmsx

#endif
