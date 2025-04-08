#pragma once

// Check if Sapera SDK is available
#if defined(SAPERA_FOUND)
    #include <SapClassBasic.h>
    #define HAS_SAPERA 1
#else
    #define HAS_SAPERA 0
#endif
