#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#define STRSAFE_NO_DEPRECATE
#include <Strsafe.h>
#include <GdiPlus.h>
#include <shlobj.h>
using namespace Gdiplus;

extern Bitmap* CurrentCover;
extern bool fCoverLoaded;
