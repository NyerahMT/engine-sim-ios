#pragma once

// The retained upstream solver uses memcpy/memset without including <cstring>
// in every translation unit. Preinclude the missing standard header without
// modifying the pinned submodule checkout.
#include <cstring>
#include "matrix.h"
