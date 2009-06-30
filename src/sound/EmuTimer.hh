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

class EmuTimerBase : public Schedulable
{
public:
	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

protected:
	EmuTimerBase(Scheduler& scheduler, EmuTimerCallback& cb, unsigned count);
	void unschedule();

private:
	virtual const std::string& schedName() const;

protected:
	EmuTimerCallback& cb;
	int count;
	bool counting;
};

template<byte FLAG, unsigned FREQ_NOM, unsigned FREQ_DENOM, unsigned MAXVAL>
class EmuTimer : public EmuTimerBase
{
public:
	EmuTimer(Scheduler& scheduler, EmuTimerCallback& cb);
	void setValue(int value);
	void setStart(bool start, EmuTime::param time);

private:
	virtual void executeUntil(EmuTime::param time, int userData);
	void schedule(EmuTime::param time);
};

typedef EmuTimer<0x40,  3579545, 64 * 2     , 1024> EmuTimerOPM_1;
typedef EmuTimer<0x20,  3579545, 64 * 2 * 16, 256 > EmuTimerOPM_2;
typedef EmuTimer<0x40,  3579545, 72 *  4    , 256 > EmuTimerOPL3_1;
typedef EmuTimer<0x20,  3579545, 72 *  4 * 4, 256 > EmuTimerOPL3_2;
typedef EmuTimer<0x40, 33868800, 72 * 38    , 256 > EmuTimerOPL4_1;
typedef EmuTimer<0x20, 33868800, 72 * 38 * 4, 256 > EmuTimerOPL4_2;

} // namespace openmsx

#endif
