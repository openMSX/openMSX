#ifndef OSDTOPWIDGET_HH
#define OSDTOPWIDGET_HH

#include "OSDWidget.hh"
#include <vector>
#include <string>

namespace openmsx {

class OSDGUI;

class OSDTopWidget : public OSDWidget
{
public:
	OSDTopWidget(OSDGUI& gui);
	virtual string_ref getType() const;
	virtual void getWidthHeight(const OutputRectangle& output,
	                            double& width, double& height) const;

	void queueError(std::string message);
	void showAllErrors();

protected:
	virtual void invalidateLocal();
	virtual void paintSDL(OutputSurface& output);
	virtual void paintGL (OutputSurface& output);

private:
	OSDGUI& gui;
	std::vector<std::string> errors;
};

} // namespace openmsx

#endif
