// $Id$

#ifndef RESAMPLELQ_HH
#define RESAMPLELQ_HH

#include "ResampleAlgo.hh"
#include "FixedPoint.hh"
#include <memory>

namespace openmsx {

class ResampledSoundDevice;

template <unsigned CHANNELS>
class ResampleLQ: public ResampleAlgo
{
public:
	static std::auto_ptr<ResampleLQ<CHANNELS> > create(
		ResampledSoundDevice& input, double ratio);

protected:
	ResampleLQ(ResampledSoundDevice& input, double ratio);
	bool fetchData(unsigned num);

	ResampledSoundDevice& input;
	typedef FixedPoint<16> Pos;
	Pos pos;
	const Pos step;
	int lastInput[CHANNELS];
};

template <unsigned CHANNELS>
class ResampleLQDown : public ResampleLQ<CHANNELS>
{
private:
	ResampleLQDown(ResampledSoundDevice& input, double ratio);
	virtual bool generateOutput(int* dataOut, unsigned num);
	friend class ResampleLQ<CHANNELS>;
};

template <unsigned CHANNELS>
class ResampleLQUp : public ResampleLQ<CHANNELS>
{
private:
	ResampleLQUp(ResampledSoundDevice& input, double ratio);
	virtual bool generateOutput(int* dataOut, unsigned num);
	friend class ResampleLQ<CHANNELS>;
};

} // namespace openmsx

#endif
