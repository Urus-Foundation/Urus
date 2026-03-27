#ifndef URUS_COMMON_H
#define URUS_COMMON_H

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

// --  Path Separator  --
#ifdef _WIN32
#define URUSC_PATHSEP '\\'
#else
#define URUSC_PATHSEP '/'
#endif

// --  Path Maximum  --
#ifndef PATH_MAX
#ifdef MAX_PATH
#define PATH_MAX MAX_PATH
#else
#define PATH_MAX 4096 /* safe default value */
#endif
#endif

#endif
