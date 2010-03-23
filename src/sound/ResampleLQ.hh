// $Id: ResampleLQ.hh 6476 2007-05-14 18:12:29Z m9710797 $

#ifndef RESAMPLELQ_HH
#define RESAMPLELQ_HH

#include "ResampleAlgo.hh"
#include "FixedPoint.hh"
#include <memory>

namespace openmsx {

class Resample;

template <unsigned CHANNELS>
class ResampleLQ: public ResampleAlgo
{
public:
	static std::auto_ptr<ResampleLQ<CHANNELS> > create(
		Resample& input, double ratio);

protected:
	ResampleLQ(Resample& input, double ratio);
	bool fetchData(unsigned num);

	Resample& input;
	typedef FixedPoint<16> Pos;
	Pos pos;
	const Pos step;
	int lastInput[CHANNELS];
};

template <unsigned CHANNELS>
class ResampleLQDown : public ResampleLQ<CHANNELS>
{
private:
	ResampleLQDown(Resample& input, double ratio);
	virtual bool generateOutput(int* dataOut, unsigned num);
	friend class ResampleLQ<CHANNELS>;
};

template <unsigned CHANNELS>
class ResampleLQUp : public ResampleLQ<CHANNELS>
{
private:
	ResampleLQUp(Resample& input, double ratio);
	virtual bool generateOutput(int* dataOut, unsigned num);
	friend class ResampleLQ<CHANNELS>;
};

} // namespace openmsx

#endif
