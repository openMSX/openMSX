// $Id$

#ifndef USERINPUTEVENTLISTENER_HH
#define USERINPUTEVENTLISTENER_HH

namespace openmsx {

class UserInputEvent;

class UserInputEventListener
{
public:
	virtual ~UserInputEventListener() {}

	/**
	 * This method gets called when an event you are subscribed to occurs.
	 */
	virtual void signalEvent(const UserInputEvent& event) = 0;

protected:
	UserInputEventListener() {}
};

} // namespace openmsx

#endif // USERINPUTEVENTLISTENER_HH
