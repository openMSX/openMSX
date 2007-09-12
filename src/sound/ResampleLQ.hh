// $Id: ResampleLQ.hh 6476 2007-05-14 18:12:29Z m9710797 $

#ifndef RESAMPLELQ_HH
#define RESAMPLELQ_HH

#include "ResampleAlgo.hh"
#include "FixedPoint.hh"

namespace openmsx {

class Resample;

template <unsigned CHANNELS>
class ResampleLQ : public ResampleAlgo
{
public:
	ResampleLQ(Resample& input, double ratio);

	virtual bool generateOutput(int* dataOut, unsigned num);

private:
	typedef FixedPoint<16> Pos;
	static const unsigned BUF_LEN = 16384;
	static const unsigned BUF_MASK = BUF_LEN - 1;

	void prepareData(unsigned missing);
	void fillBuffer(unsigned pos, unsigned num);

        Resample& input;
	Pos pos;
	Pos step;
	unsigned bufStart;
	unsigned bufEnd;
	int buffer[BUF_LEN * CHANNELS + 3];
};

} // namespace openmsx

#endif
