// $Id$

#ifndef __LEDS_HH__
#define __LEDS_HH__

class Leds 
{
	public:
		enum LEDCommand {
			POWER_ON, POWER_OFF,
			CAPS_ON,  CAPS_OFF,
			KANA_ON,  KANA_OFF,	// same as CODE LED
			PAUSE_ON, PAUSE_OFF,
			TURBO_ON, TURBO_OFF,
			FDD_ON,   FDD_OFF
		};
		
		~Leds();
		static Leds *instance();
		void setLed(LEDCommand led);
		
	private:
		Leds();
		static Leds* oneInstance;
};
#endif
