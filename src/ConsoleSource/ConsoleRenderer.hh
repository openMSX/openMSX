// $Id$

#ifndef __CONSOLERENDERER_HH__
#define __CONSOLERENDERER_HH__


class ConsoleRenderer
{
	public:
		ConsoleRenderer();
		virtual ~ConsoleRenderer();

		virtual void updateConsole() = 0;
};

#endif
