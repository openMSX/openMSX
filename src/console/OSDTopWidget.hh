#ifndef OSDTOPWIDGET_HH
#define OSDTOPWIDGET_HH

#include "OSDWidget.hh"
#include <vector>
#include <string>

namespace openmsx {

class OSDGUI;

class OSDTopWidget final : public OSDWidget
{
public:
	OSDTopWidget(OSDGUI& gui);
	string_ref getType() const override;
	gl::vec2 getSize(const OutputRectangle& output) const override;

	void queueError(std::string message);
	void showAllErrors();

protected:
	void invalidateLocal() override;
	void paintSDL(OutputSurface& output) override;
	void paintGL (OutputSurface& output) override;

private:
	OSDGUI& gui;
	std::vector<std::string> errors;
};

} // namespace openmsx

#endif
