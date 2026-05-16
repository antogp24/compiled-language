#pragma once

#include <cassert>
#include <debug_break.h>

#ifdef NDEBUG
#   define Assert(x) ((void)0)
#else
#   define Assert(x) do { if (!(x)) { debug_break(); } assert(x); } while(0)
#endif

#define unreachable() Assert(!"Unreachable")
