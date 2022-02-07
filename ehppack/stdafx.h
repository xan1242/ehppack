// Windows defines go here...

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <strsafe.h>
#include <ctype.h>

#define path_separator "\\"
