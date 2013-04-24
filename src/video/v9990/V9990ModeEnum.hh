#ifndef V9990MODEENUM_HH
#define V9990MODEENUM_HH

namespace openmsx {

enum V9990DisplayMode {
	INVALID_DISPLAY_MODE = -1,
	P1, P2, B0, B1, B2, B3, B4, B5, B6, B7
};

enum V9990ColorMode {
	INVALID_COLOR_MODE = -1,
	PP, BYUV, BYUVP, BYJK, BYJKP, BD16, BD8, BP6, BP4, BP2
};

} // namespace openmsx

#endif
