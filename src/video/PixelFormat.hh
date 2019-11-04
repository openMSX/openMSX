#ifndef PIXELFORMAT_HH
#define PIXELFORMAT_HH

namespace openmsx {

class PixelFormat
{
public:
	PixelFormat() = default;
	virtual ~PixelFormat() = default;

	virtual unsigned map(unsigned r, unsigned g, unsigned b) const = 0;
	virtual unsigned getBpp() const = 0;
	virtual unsigned getBytesPerPixel() const = 0;
	virtual unsigned getRmask() const = 0;
	virtual unsigned getGmask() const = 0;
	virtual unsigned getBmask() const = 0;
	virtual unsigned getAmask() const = 0;
	virtual unsigned getRshift() const = 0;
	virtual unsigned getGshift() const = 0;
	virtual unsigned getBshift() const = 0;
	virtual unsigned getAshift() const = 0;
	virtual unsigned getRloss() const = 0;
	virtual unsigned getGloss() const = 0;
	virtual unsigned getBloss() const = 0;
	virtual unsigned getAloss() const = 0;
};

} // namespace openmsx

#endif
