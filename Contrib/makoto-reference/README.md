# Preserved Makoto reference

This is the patched YM2608 control layer used before the openMSX-owned layer.
It retains Aaron Giles' BSD-3-Clause notice and is used only for regression
comparison. It is not linked into openMSX.

Standalone reference tests in Contrib need a preserved YMFM tree (not shipped
in openMSX after the engines were folded into src/sound/YM2608.{hh,cc}) plus
ReferenceYM2608.cc. The owned-core path compares against the current
src/sound/YM2608 sources. Compile as C++20.
