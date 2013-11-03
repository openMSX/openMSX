/*
 **
 ** File: ym2151.h - header file for software implementation of YM2151
 **                                            FM Operator Type-M(OPM)
 **
 ** (c) 1997-2002 Jarek Burczynski (s0246@poczta.onet.pl, bujar@mame.net)
 ** Some of the optimizing ideas by Tatsuyuki Satoh
 **
 ** Version 2.150 final beta May, 11th 2002
 **
 **
 ** I would like to thank following people for making this project possible:
 **
 ** Beauty Planets - for making a lot of real YM2151 samples and providing
 ** additional informations about the chip. Also for the time spent making
 ** the samples and the speed of replying to my endless requests.
 **
 ** Shigeharu Isoda - for general help, for taking time to scan his YM2151
 ** Japanese Manual first of all, and answering MANY of my questions.
 **
 ** Nao - for giving me some info about YM2151 and pointing me to Shigeharu.
 ** Also for creating fmemu (which I still use to test the emulator).
 **
 ** Aaron Giles and Chris Hardy - they made some samples of one of my favourite
 ** arcade games so I could compare it to my emulator.
 **
 ** Bryan McPhail and Tim (powerjaw) - for making some samples.
 **
 ** Ishmair - for the datasheet and motivation.
 */

#ifndef YM2151_HH
#define YM2151_HH

#include "EmuTime.hh"
#include "openmsx.hh"
#include <string>
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class DeviceConfig;

class YM2151
{
public:
	YM2151(const std::string& name, const std::string& desc,
	       const DeviceConfig& config, EmuTime::param time);
	~YM2151();

	void reset(EmuTime::param time);
	void writeReg(byte r, byte v, EmuTime::param time);
	byte readStatus() const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	class Impl;
	const std::unique_ptr<Impl> pimpl;
};

} // namespace openmsx

#endif /*YM2151_HH*/
