#pragma once

#include <cassert>
#include <debug_break.h>

#ifdef NDEBUG
#   define debug_assert(x) ((void)0)
#else
#   define debug_assert(x) do { if (!(x)) { debug_break(); } assert(x); } while(0)
#endif

#define unreachable() debug_assert(!"Unreachable")
