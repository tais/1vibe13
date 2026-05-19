#include "types.h"

// JA2 ships no .BIK files; every cinematic is .SMK and is now played
// via libsmacker (see Utils/Cinematics.cpp). The legacy Win32 Bink
// path used SMACK.H / ddraw.h / Mss.h / DirectDraw Calls.h / etc. --
// all deleted by the SDL3 port. Provide no-op stubs so Intro.cpp's
// SMK/BINK dispatch state machine still links on every platform.

#include "Cinematics Bink.h"

void     BinkInitialize(void*, UINT32, UINT32) {}
BOOLEAN  BinkPollFlics(void)                       { return FALSE; }
void     BinkCloseFlic(BINKFLIC*)                  {}
void     BinkShutdownVideo(void)                   {}
BINKFLIC* BinkPlayFlic(const CHAR8*, UINT32, UINT32, UINT32) { return nullptr; }
