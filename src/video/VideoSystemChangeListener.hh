#ifndef VIDEOSYSTEMCHANGELISTENER_HH
#define VIDEOSYSTEMCHANGELISTENER_HH

namespace openmsx {

class VideoSystemChangeListener
{
public:
	virtual void preVideoSystemChange() noexcept = 0;
	virtual void postVideoSystemChange() noexcept = 0;

protected:
	~VideoSystemChangeListener() = default;
};

} // namespace openmsx

#endif
