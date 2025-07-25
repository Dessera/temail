#pragma once

#if defined _WIN32 || defined __CYGWIN__
#ifdef BUILDING_TEMAIL
#define TEMAIL_PUBLIC __declspec(dllexport)
#else
#define TEMAIL_PUBLIC __declspec(dllimport)
#endif
#else
#ifdef BUILDING_TEMAIL
#define TEMAIL_PUBLIC __attribute__((visibility("default")))
#else
#define TEMAIL_PUBLIC
#endif
#endif

#if defined _WIN32 || defined __CYGWIN__
#define TEMAIL_INLINE inline __forceinline
#else
#define TEMAIL_INLINE inline __attribute__((always_inline))
#endif
