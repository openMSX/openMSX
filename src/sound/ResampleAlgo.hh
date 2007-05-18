// $Id: ResampleAlgo.hh 6481 2007-05-17 11:13:22Z m9710797 $

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
