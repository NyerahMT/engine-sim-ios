#ifndef ENGINE_SIM_MALLOC_COMPAT_H
#define ENGINE_SIM_MALLOC_COMPAT_H

// Piranha's allocator includes the Linux-style <malloc.h>. macOS exposes the
// same APIs through malloc/malloc.h; keep other toolchains on their native
// standard-library declarations.
#if defined(__APPLE__)
#include <malloc/malloc.h>
#elif defined(_MSC_VER)
#include <cstdlib>
#else
#include_next <malloc.h>
#endif

#endif /* ENGINE_SIM_MALLOC_COMPAT_H */
