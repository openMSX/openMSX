// $Id$

#include <iostream>
#include "Leds.hh"
#include "openmsx.hh"
#include "CliCommOutput.hh"


namespace openmsx {

Leds::Leds()
	: output(CliCommOutput::instance())
{
	pwrLed = capsLed = kanaLed = turboLed = true;
	pauseLedCounter = 1;
	fddLedCounter = 1;
	setLed(POWER_OFF);
	setLed(CAPS_OFF);
	setLed(KANA_OFF);
	setLed(PAUSE_OFF);
	setLed(TURBO_OFF);
	setLed(FDD_OFF);
}

Leds::~Leds()
{
	setLed(POWER_OFF);
	setLed(CAPS_OFF);
	setLed(KANA_OFF);
	setLed(TURBO_OFF);
	if (pauseLedCounter > 0) {
		setLed(PAUSE_OFF);
	}
	if (fddLedCounter > 0) {
		setLed(FDD_OFF);
	}
}

Leds& Leds::instance()
{
	static Leds oneInstance;
	return oneInstance;
}


void Leds::setLed(LEDCommand led)
{
	switch (led) {
	case POWER_ON:
		if (!pwrLed) {
			output.update(CliCommOutput::LED, "power", "on");
			pwrLed = true;
		}
		break;
	case POWER_OFF:
		if (pwrLed) {
			output.update(CliCommOutput::LED, "power", "off");
			pwrLed = false;
		}
		break;
	case CAPS_ON:
		if (!capsLed) {
			output.update(CliCommOutput::LED, "caps", "on");
			capsLed = true;
		}
		break;
	case CAPS_OFF:
		if (capsLed) {
			output.update(CliCommOutput::LED, "caps", "off");
			capsLed = false;
		}
		break;
	case KANA_ON:
		if (!kanaLed) {
			output.update(CliCommOutput::LED, "kana", "on");
			kanaLed = true;
		}
		break;
	case KANA_OFF:
		if (kanaLed) {
			output.update(CliCommOutput::LED, "kana", "off");
			kanaLed = false;
		}
		break;
	case PAUSE_ON:
		if (pauseLedCounter == 0) {
			// turn on if it was off
			output.update(CliCommOutput::LED, "pause", "on");
		}
		pauseLedCounter++;
		break;
	case PAUSE_OFF:
		if (pauseLedCounter == 1) {
			// only turn off when it is the last one
			output.update(CliCommOutput::LED, "pause", "off");
		}
		pauseLedCounter--;
		break;
	case TURBO_ON:
		if (!turboLed) {
			output.update(CliCommOutput::LED, "turbo", "on");
			turboLed = true;
		}
		break;
	case TURBO_OFF:
		if (turboLed) {
			output.update(CliCommOutput::LED, "turbo", "off");
			turboLed = false;
		}
		break;
	case FDD_ON:
		if (fddLedCounter == 0) {
			// turn on if it was off
			output.update(CliCommOutput::LED, "FDD", "on");
		}
		fddLedCounter++;
		break;
	case FDD_OFF:
		if (fddLedCounter == 1) {
			// only turn off when it is the last one
			output.update(CliCommOutput::LED, "FDD", "off");
		}
		fddLedCounter--;
		break;
	}
}

} // namespace openmsx
