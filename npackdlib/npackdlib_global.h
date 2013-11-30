#ifndef NPACKDLIB_GLOBAL_H
#define NPACKDLIB_GLOBAL_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef NPACKDLIB_LIBRARY
#define NPACKDLIBSHARED_EXPORT __declspec(dllexport)
#else
#define NPACKDLIBSHARED_EXPORT __declspec(dllimport)
#endif

#ifdef __cplusplus
}
#endif

#endif // NPACKDLIB_GLOBAL_H
