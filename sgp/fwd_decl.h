#ifndef __FWD_DECL_H
#define __FWD_DECL_H

// Lightweight forward declarations for the heaviest, most-included tactical
// types. A header that only uses POINTERS (or references) to these types can
// include this in place of the full, transitively enormous Overhead.h, which
// cuts the rebuild fan-out: touching Soldier Control.h / Overhead.h no longer
// forces a recompile of every translation unit that merely passes a pointer.
//
// Anything that DEREFERENCES one of these types, accesses a member, or uses it
// by value still needs the full definition (Soldier Control.h / Overhead.h).
//
// NB: the tags here must match the real definitions exactly --
//   SOLDIERTYPE       is `class`  (Tactical/Soldier Control.h)
//   TacticalStatusType is `struct` (Tactical/Overhead.h)
// Mixing struct/class tags compiles on clang but warns (MSVC C4099), so keep
// them in sync if a definition ever changes.
//
// MAP_ELEMENT is intentionally absent: it is an anonymous `typedef struct {..}
// MAP_ELEMENT;` (TileEngine/worlddef.h) and therefore cannot be forward
// declared. Give it a tag name first if it ever needs to live here.

class SOLDIERTYPE;
struct TacticalStatusType;

#endif
