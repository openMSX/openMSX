// $Id$

#ifndef __CONSOLE_HH__
#define __CONSOLE_HH__

#include <list>
#include <string>

namespace openmsx {

class ConsoleRenderer;

using std::list;
using std::string;


class Console
{
	public:
		Console();
		virtual ~Console();

		/** Add a renderer for this console.
		  */
		void registerConsole(ConsoleRenderer *console);
		
		/** Remove a renderer for this console.
		  */
		void unregisterConsole(ConsoleRenderer *console);

		virtual unsigned getScrollBack() const = 0;
		virtual const string& getLine(unsigned line) const = 0;
		virtual bool isVisible() const = 0;
		virtual void getCursorPosition(unsigned& xPosition, unsigned& yPosition) const = 0;
		virtual void setCursorPosition(unsigned xPosition, unsigned yPosition) = 0;
		virtual void setConsoleDimensions(unsigned columns, unsigned rows) = 0;
		virtual const string& getId() const = 0;

	protected:
		void updateConsole();

	private:
		list<ConsoleRenderer*> renderers;
};

} // namespace openmsx

#endif
