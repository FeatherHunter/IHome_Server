#ifndef  _H_IDEBUG
#define  _H_IDEBUG
#include <stdio.h>

#define _DEBUG 1
#if _DEBUG
  #define DEBUG(format, ...) printf (format, ##__VA_ARGS__)
#else
  #define DEBUG(x)
#endif

#endif // _H_DEBUG
