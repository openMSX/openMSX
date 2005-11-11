// $Id$

#ifndef EMUTIMER_HH
#define EMUTIMER_HH

#include "Schedulable.hh"
#include "openmsx.hh"

namespace openmsx {

class EmuTimerCallback
{
public:
	virtual void callback(byte value) = 0;
protected:
	virtual ~EmuTimerCallback() {}
};

template<byte FLAG, unsigned FREQ_NOM, unsigned FREQ_DENOM>
class EmuTimer : private Schedulable
{
public:
	EmuTimer(Scheduler& scheduler, EmuTimerCallback& cb);
	virtual ~EmuTimer();
	void setValue(byte value);
	void setStart(bool start, const EmuTime& time);

private:
	virtual void executeUntil(const EmuTime& time, int userData);
	virtual const std::string& schedName() const;
	void schedule(const EmuTime& time);
	void unschedule();

	int count;
	bool counting;
	EmuTimerCallback& cb;
};

typedef EmuTimer<0x40,  3579545, 72 *  4    > EmuTimerOPL3_1;
typedef EmuTimer<0x20,  3579545, 72 *  4 * 4> EmuTimerOPL3_2;
typedef EmuTimer<0x40, 33868800, 72 * 38    > EmuTimerOPL4_1;
typedef EmuTimer<0x20, 33868800, 72 * 38 * 4> EmuTimerOPL4_2;

} // namespace openmsx

#endif
