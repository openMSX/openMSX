#ifndef EMUTIMER_HH
#define EMUTIMER_HH

#include "DynamicClock.hh"
#include "Schedulable.hh"

#include <cstdint>
#include <memory>

namespace openmsx {

class EmuTimerCallback
{
public:
	virtual void callback(uint8_t value) = 0;

protected:
	~EmuTimerCallback() = default;
};


class EmuTimer final : public Schedulable
{
public:
	EmuTimer(Scheduler& scheduler, EmuTimerCallback& cb,
	         uint8_t flag, unsigned freq_num, unsigned freq_denom,
	         int maxVal);

	[[nodiscard]] static std::unique_ptr<EmuTimer> createOPM_1(
		Scheduler& scheduler, EmuTimerCallback& cb);
	[[nodiscard]] static std::unique_ptr<EmuTimer> createOPM_2(
		Scheduler& scheduler, EmuTimerCallback& cb);
	[[nodiscard]] static std::unique_ptr<EmuTimer> createOPP_2(
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
	void setStart(bool start, EmuTime time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void executeUntil(EmuTime time) override;
	void schedule(EmuTime time);
	void unschedule();

private:
	EmuTimerCallback& cb;
	DynamicClock clock{EmuTime::zero()};
	const int maxVal;
	int count;
	const uint8_t flag;
	bool counting = false;
};

} // namespace openmsx

#endif
