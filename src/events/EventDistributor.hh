// $Id$

#ifndef __EVENTDISTRIBUTOR_HH__
#define __EVENTDISTRIBUTOR_HH__

#include <SDL/SDL.h>
#include <map>
#include <queue>
#include "Schedulable.hh"
#include "Command.hh"
#include "Settings.hh"

using std::multimap;
using std::queue;
using std::pair;

namespace openmsx {

class EventListener;


class EventDistributor : private SettingListener
{
public:
	static EventDistributor* instance();

	/** Poll / wait for an event and handle it.
	  * These methods should be called from the main thread.
	  */
	void poll();
	void wait();
	void notify();

	/**
	 * Use this method to (un)register a given class to receive
	 * certain SDLEvents.
	 * @param type The SDLEventType number of the events you want to receive
	 * @param listener Object that will be notified when the events arrives
	 * @param priority The priority of the listener (lower number is higher
	 *        priority). Higher priority listeners may block an event for
	 *        lower priority listeners. Normally you don't need to specify
	 *        a priority.
	 *        Note: in the current implementation there are only two
	 *              priority levels (0 and !=0)
	 * The delivery of the event is done by the 'main-emulation-thread',
	 * so there is no need for extra synchronization.
	 */
	void   registerEventListener(int type, EventListener* listener, int priority = 0);
	void unregisterEventListener(int type, EventListener* listener, int priority = 0);

private:
	EventDistributor();
	virtual ~EventDistributor();

	void handle(SDL_Event &event);

	// SettingListener
	virtual void update(const SettingLeafNode *setting);

	typedef multimap<int, EventListener*> ListenerMap;
	ListenerMap lowMap;
	ListenerMap highMap;
	queue <pair<SDL_Event, EventListener*> > lowQueue;
	queue <pair<SDL_Event, EventListener*> > highQueue;
	BooleanSetting grabInput;

	class AfterCommand : public Command {
	public:
		virtual string execute(const vector<string>& tokens)
			throw(CommandException);
		virtual string help(const vector<string>& tokens) const
			throw();
		virtual void tabCompletion(const vector<string>& tokens) const
			throw();
	private:
		string afterTime(const vector<string>& tokens);
		string afterIdle(const vector<string>& tokens);
		string afterInfo(const vector<string>& tokens);
		string afterCancel(const vector<string>& tokens);
	} afterCommand;
	friend class AfterCommand;
	
	enum AfterType { TIME, IDLE };
	string afterNew(const vector<string>& tokens, AfterType type);

	struct AfterCmd : public Schedulable {
		virtual ~AfterCmd() {}
		unsigned id;
		AfterType type;
		float time;
		string command;
	private:
		virtual void executeUntil(const EmuTime& time, int userData)
			throw();
		virtual const string& schedName() const;
	};
	map<unsigned, AfterCmd*> afterCmds;
	unsigned lastAfterId;
};

} // namespace openmsx

#endif
