// $Id$

#ifndef __CONSOLERENDERER_HH__
#define __CONSOLERENDERER_HH__


namespace openmsx {

class ConsoleRenderer
{
	public:
		ConsoleRenderer();
		virtual ~ConsoleRenderer();

		virtual void updateConsole() = 0;
};

} // namespace openmsx

#endif
