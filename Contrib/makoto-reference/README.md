# Preserved Makoto reference

This is the patched YM2608 control layer used before the openMSX-owned layer.
It retains Aaron Giles' BSD-3-Clause notice and is used only for regression
comparison. It is not linked into openMSX.

Standalone reference tests in Contrib need ReferenceYM2608.cc alongside
YMFM's ymfm_opn.cpp, ymfm_ssg.cpp and ymfm_adpcm.cpp. Include src, src/sound
and src/3rdparty/ymfm. The owned-core test additionally needs
src/sound/MakotoYM2608.cc. Compile as C++20.
