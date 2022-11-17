#ifndef ICON_HH
#define ICON_HH

namespace openmsx {

struct OpenMSX_Icon
{
	unsigned    width;
	unsigned    height;
	unsigned    bytes_per_pixel; /* 3:RGB, 4:RGBA */
	const char* pixel_data;
};

extern const OpenMSX_Icon openMSX_icon;

} // namespace openmsx

#endif
