// $Id:$

#ifndef DIRECTSCALEROUTPUT_HH
#define DIRECTSCALEROUTPUT_HH

#include "ScalerOutput.hh"

namespace openmsx {

class OutputSurface;

template<typename Pixel>
class DirectScalerOutput : public ScalerOutput<Pixel>
{
public:
	DirectScalerOutput(OutputSurface& output);

	virtual unsigned getWidth()  const;
	virtual unsigned getHeight() const;
	virtual Pixel* acquireLine(unsigned y);
	virtual void   releaseLine(unsigned y, Pixel* buf);
	virtual void   fillLine   (unsigned y, Pixel color);

private:
	OutputSurface& output;
};

} // namespace openmsx

#endif
