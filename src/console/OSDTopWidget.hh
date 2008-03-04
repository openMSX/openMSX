// $Id$

#ifndef OSDTOPWIDGET_HH
#define OSDTOPWIDGET_HH

#include "OSDWidget.hh"

namespace openmsx {

class OSDTopWidget : public OSDWidget
{
public:
	OSDTopWidget();
	virtual std::string getType() const;
	virtual void transformXY(const OutputSurface& output,
	                         int x, int y, double relx, double rely,
	                         int& outx, int& outy) const;

protected:
	virtual void invalidateLocal();
	virtual void paintSDL(OutputSurface& output);
	virtual void paintGL (OutputSurface& output);
};

} // namespace openmsx

#endif
