#ifndef ALTSPACESUPPRESSOR_HH
#define ALTSPACESUPPRESSOR_HH

#ifdef _WIN32
#include <windows.h>

namespace openmsx {

// Utility class that stacks a WindowLongPtr value
class WindowLongPtrStacker
{
public:
	WindowLongPtrStacker(int index, LONG_PTR value);
	void Push(HWND hWndArg);
	void Pop();
	LONG_PTR GetOldValue();
private:
	HWND hWnd;
	int nIndex;
	LONG_PTR oldValue, newValue;
};


// Suppressor of ALT+SPACE windows messages
class AltSpaceSuppressor
{
public:
	static void Start(HWND hWnd);
	static void Stop();
private:
	static WindowLongPtrStacker procStacker;
	static LRESULT CALLBACK InterceptorWndProc(
		HWND hWnd,
		UINT message,
		WPARAM wParam,
		LPARAM lParam);
	static bool SuppressAltSpace(
		HWND hWnd,
		UINT message,
		WPARAM wParam,
		LPARAM lParam,
		LRESULT* outResult);
};

} // namespace openmsx

#endif

#endif
