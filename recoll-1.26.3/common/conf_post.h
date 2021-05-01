
#ifdef _WIN32
#include "safewindows.h"


#ifdef _MSC_VER
// gmtime is supposedly thread-safe on windows
#define gmtime_r(A, B) gmtime(A)
#define localtime_r(A,B) localtime(A)
typedef int mode_t;
#define fseeko _fseeki64
#define ftello (off_t)_ftelli64
#define ftruncate _chsize_s
#define PATH_MAX MAX_PATH
#define RCL_ICONV_INBUF_CONST 1
#define HAVE_STRUCT_TIMESPEC
#define strdup _strdup
#define timegm _mkgmtime

#else // End _MSC_VER -> Gminw

#undef RCL_ICONV_INBUF_CONST
#define timegm portable_timegm

#endif // GMinw only

typedef int pid_t;
inline int readlink(const char *a, void *b, int c)
{
    a = a; b = b; c = c;
    return -1;
}

#define MAXPATHLEN PATH_MAX
typedef DWORD32 u_int32_t;
typedef DWORD64 u_int64_t;
typedef unsigned __int8 u_int8_t;
typedef int ssize_t;
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#define chdir _chdir

#define R_OK 4
#define W_OK 2
#ifndef X_OK
#define X_OK 4
#endif

#define S_ISLNK(X) false
#define lstat stat

#endif // _WIN32
