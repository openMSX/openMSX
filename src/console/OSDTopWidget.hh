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
	virtual void getWidthHeight(const OutputSurface& output,
	                            int& width, int& height) const;

protected:
	virtual void invalidateLocal();
	virtual void paintSDL(OutputSurface& output);
	virtual void paintGL (OutputSurface& output);
};

} // namespace openmsx

#endif
