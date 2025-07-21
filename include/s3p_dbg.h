#ifndef _S3P_DBG_H
#define _S3P_DBG_H

// Custom macro for debug. Overload this with an higher priority
// include for custom platforms
#include <stdio.h>
#define DBG(_lvl, ...)  do { if (_lvl<=_dbg_lvl) printf(__VA_ARGS__); } while (0)
static int _dbg_lvl;

#endif // _S3P_DBG_H

