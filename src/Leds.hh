
#ifndef __LEDS_HH__
#define __LEDS_HH__

#define POWER_ON  1
#define POWER_OFF 2
#define CAPS_ON   3
#define CAPS_OFF  4
#define CODE_ON   5
#define CODE_OFF  6
#define KANA_ON   5	// same as CODE
#define KANA_OFF  6
#define PAUSE_ON  7
#define PAUSE_OFF 8
#define TURBO_ON  9
#define TURBO_OFF 10
#define FDD_ON    11
#define FDD_OFF   12

class Leds 
{
	public:
		~Leds(); 
		static Leds *instance();
		void setLed(int led);
	private:
		Leds();
		static Leds* oneInstance; 
};
#endif
