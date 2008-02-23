// $Id$

#ifndef OSDWIDGET_HH
#define OSDWIDGET_HH

#include <string>
#include <set>

namespace openmsx {

class OSDGUI;
class OutputSurface;

class OSDWidget
{
public:
	virtual ~OSDWidget();

	std::string getName() const;
	int getZ() const;

	virtual void getProperties(std::set<std::string>& result) const;
	virtual void setProperty(const std::string& name, const std::string& value);
	virtual std::string getProperty(const std::string& name) const;
	virtual std::string getType() const = 0;

	virtual void paintSDL(OutputSurface& output) = 0;
	virtual void paintGL (OutputSurface& output) = 0;
	virtual void invalidate() = 0;

protected:
	OSDWidget(OSDGUI& gui);

private:
	OSDGUI& gui;
	unsigned id;
	int z;
};

} // namespace openmsx

#endif
