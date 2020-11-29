#ifndef EMUTIMER_HH
#define EMUTIMER_HH

#include "Schedulable.hh"
#include "DynamicClock.hh"
#include "openmsx.hh"
#include <memory>

namespace openmsx {

class EmuTimerCallback
{
public:
	virtual void callback(byte value) = 0;

protected:
	~EmuTimerCallback() = default;
};


class EmuTimer final : public Schedulable
{
public:
	EmuTimer(Scheduler& scheduler, EmuTimerCallback& cb,
	         byte flag, unsigned freq_num, unsigned freq_denom,
	         unsigned maxval);

	[[nodiscard]] static std::unique_ptr<EmuTimer> createOPM_1(
		Scheduler& scheduler, EmuTimerCallback& cb);
	[[nodiscard]] static std::unique_ptr<EmuTimer> createOPM_2(
		Scheduler& scheduler, EmuTimerCallback& cb);
	[[nodiscard]] static std::unique_ptr<EmuTimer> createOPL3_1(
		Scheduler& scheduler, EmuTimerCallback& cb);
	[[nodiscard]] static std::unique_ptr<EmuTimer> createOPL3_2(
		Scheduler& scheduler, EmuTimerCallback& cb);
	[[nodiscard]] static std::unique_ptr<EmuTimer> createOPL4_1(
		Scheduler& scheduler, EmuTimerCallback& cb);
	[[nodiscard]] static std::unique_ptr<EmuTimer> createOPL4_2(
		Scheduler& scheduler, EmuTimerCallback& cb);

	void setValue(int value);
	void setStart(bool start, EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void executeUntil(EmuTime::param time) override;
	void schedule(EmuTime::param time);
	void unschedule();

private:
	EmuTimerCallback& cb;
	DynamicClock clock;
	const unsigned maxval;
	int count;
	const byte flag;
	bool counting;
};

} // namespace openmsx

#endif
