#pragma once


#define UNX_MAJOR 0
#define UNX_MINOR 9
#define UNX_BUILD 1
#define UNX_REV   1





#define _A2(a)     #a
#define  _A(a)  _A2(a)
#define _L2(w)  L ## w
#define  _L(w)  _L2(w)


#if (UNX_REV > 0)
#define UNX_VERSION_STR_A    _A(UNX_MAJOR) "." _A(UNX_MINOR) "." _A(UNX_BUILD) "." _A(UNX_REV)
#else
#define UNX_VERSION_STR_A    _A(UNX_MAJOR) "." _A(UNX_MINOR) "." _A(UNX_BUILD)
#endif

#define UNX_VERSION_STR_W _L(UNX_VERSION_STR_A)


#define UNX_FILE_VERSION     UNX_MAJOR,UNX_MINOR,UNX_BUILD,UNX_REV
#define UNX_PRODUCT_VERSION  UNX_MAJOR,UNX_MINOR,UNX_BUILD,UNX_REV
