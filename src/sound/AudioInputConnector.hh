// $Id$

#ifndef __AUDIOINPUTCONNECTOR_HH__
#define __AUDIOINPUTCONNECTOR_HH__

#include "Connector.hh"


namespace openmsx {

class AudioInputConnector : public Connector {
public:
	AudioInputConnector(const string &name);
	virtual ~AudioInputConnector();

	// Connector
	virtual const string &getClass() const;

	short readSample(const EmuTime &time);
};

} // namespace openmsx

#endif // __AUDIOINPUTCONNECTOR_HH__
