// $Id$

#ifndef __FRONTSWITCH_HH__
#define __FRONTSWITCH_HH__

#include "Command.hh"


class FrontSwitch : public Command
{
	public:
		FrontSwitch();
		virtual ~FrontSwitch();

		/**
		 * Get the current position
		 * @result true when switch is in the 'on' position
		 */
		bool isOn() const;
		
		// Command
		virtual void execute(const std::vector<std::string> &tokens,
		                     const EmuTime &time);
		virtual void help(const std::vector<std::string> &tokens) const;

	private:
		bool position;
};

#endif
