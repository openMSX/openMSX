#ifndef OUTPUTRECTANGLE_HH
#define OUTPUTRECTANGLE_HH

#include "gl_vec.hh"

namespace openmsx {

class OutputRectangle
{
public:
	virtual gl::ivec2 getOutputSize() const = 0;

protected:
	~OutputRectangle() {}
};


class DummyOutputRectangle final : public OutputRectangle
{
public:
	explicit DummyOutputRectangle(gl::ivec2 size_)
		: size(size_) {}

	gl::ivec2 getOutputSize() const override { return size; }

private:
	const gl::ivec2 size;
};

} // namespace openmsx

#endif
