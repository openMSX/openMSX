#ifndef VIDEOSYSTEMCHANGELISTENER_HH
#define VIDEOSYSTEMCHANGELISTENER_HH

namespace openmsx {

class VideoSystemChangeListener
{
public:
	virtual void preVideoSystemChange() = 0;
	virtual void postVideoSystemChange() = 0;

protected:
	~VideoSystemChangeListener() {}
};

} // namespace openmsx

#endif
