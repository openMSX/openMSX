
#ifndef __LEDS_HH__
#define __LEDS_HH__

class Leds 
{
	public:
		~Leds();
		static Leds *instance();
		void setLed(int led);
		
		static const int POWER_ON  = 1;
		static const int POWER_OFF = 2;
		static const int CAPS_ON   = 3;
		static const int CAPS_OFF  = 4;
		static const int CODE_ON   = 5;
		static const int CODE_OFF  = 6;
		static const int KANA_ON   = 5;	// same as CODE
		static const int KANA_OFF  = 6;
		static const int PAUSE_ON  = 7;
		static const int PAUSE_OFF = 8;
		static const int TURBO_ON  = 9;
		static const int TURBO_OFF = 10;
		static const int FDD_ON    = 11;
		static const int FDD_OFF   = 12;
		
	private:
		Leds();
		static Leds* oneInstance;
};
#endif
