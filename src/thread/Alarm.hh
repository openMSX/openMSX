// $Id$

#ifndef ALARM_HH
#define ALARM_HH

namespace openmsx {

class Alarm
{
public:
	virtual void alarm() = 0;
	
protected:
	Alarm(unsigned interval);
	virtual ~Alarm();
	
private:
	static unsigned helper(unsigned interval, void* param);
	void* id;
};

} // namespace openmsx

#endif
