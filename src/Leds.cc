
#include <iostream>
#include <assert.h>

#include "Leds.hh"
#include "openmsx.hh"


Leds* Leds::oneInstance = NULL;

Leds* Leds::instance()
{
	if (oneInstance == NULL) {
		oneInstance = new Leds();
	}
	return oneInstance;
};

Leds::Leds()
{
};

Leds::~Leds()
{
};

void Leds::setLed(int led)
{
	switch (led) {
	case POWER_ON:
		PRT_INFO ("Power LED ON");
		break;
	case POWER_OFF:
		PRT_INFO ("Power LED OFF");
		break;
	case CAPS_ON:
		PRT_INFO ("Caps LED ON");
		break;
	case CAPS_OFF:
		PRT_INFO ("Caps LED OFF");
		break;
	case KANA_ON:
		PRT_INFO ("Kana LED ON");
		break;
	case KANA_OFF:
		PRT_INFO ("Kana LED OFF");
		break;
	case PAUSE_ON:
		PRT_INFO ("Pause LED ON");
		break;
	case PAUSE_OFF:
		PRT_INFO ("Pause LED OFF");
		break;
	case TURBO_ON:
		PRT_INFO ("Turbo LED ON");
		break;
	case TURBO_OFF:
		PRT_INFO ("Turbo LED OFF");
		break;
	case FDD_ON:
		PRT_INFO ("FDD LED ON");
		break;
	case FDD_OFF:
		PRT_INFO ("FDD LED OFF");
		break;
	default:
		assert (false);
	}
};
