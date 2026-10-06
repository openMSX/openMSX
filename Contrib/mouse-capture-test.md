# Mouse capture regression test

`mouse-capture-test.py` runs an isolated C-BIOS MSX2+ profile and sends SDL
events through the actual input generator, ImGui, and MSX event distributor.
The accompanying header adds a command only to a separate test executable;
the distributed emulator does not contain it.

First build the normal Release/x64 MSVC project. Then run:

```text
python Contrib/mouse-capture-test.py --prepare-msvc
```

Build the printed project using the same configuration, toolset, and
third-party dependency properties as the production build, plus the four
directory properties printed by the script. This project replaces `main.cc`
with an instrumented copy and links the production object files. Rebuild
the production objects before rebuilding this executable after source edits.
The helper copies `ogg.dll` from the production install directory if present.

```text
python Contrib/mouse-capture-test.py --openmsx derived/mouse-capture-test/install/openmsx.exe
```

The test needs an SDL/OpenGL display. On a noninteractive Windows build
desktop, add `--synthetic-focus`: this uses the statically linked SDL
internal focus setter, alongside the synthetic mouse and keyboard events.
That mode checks focus-event handling, but does **not** validate native OS
focus transitions. Neither mode is a substitute for checking physical
mouse motion, cursor appearance, or multi-monitor behavior manually.

Fifteen groups cover released pointer visibility, menu interaction, consumed
activation clicks, movement routing, middle-click release, no default Ctrl+F10
release, held and delayed button cleanup, an optional user binding, console/debugger and
pause/focus release, legacy mode, undocked menu/status bar, fullscreen,
`escape_grab`, machine switching, and mouse unplugging. Each run writes its
command log and passed groups below `derived/mouse-capture-*`. It creates its
own user profile and leaves existing profiles untouched.
