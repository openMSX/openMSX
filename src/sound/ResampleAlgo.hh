// $Id$

#ifndef RESAMPLEALGO_HH
#define RESAMPLEALGO_HH

namespace openmsx {

class ResampleAlgo
{
public:
	virtual ~ResampleAlgo() {}
	virtual bool generateOutput(int* dataOut, unsigned num) = 0;
};

} // namespace openmsx

#endif
