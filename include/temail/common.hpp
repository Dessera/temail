#pragma once

#include <qanystringview.h>
#include <qlogging.h>
#include <qmetaobject.h>

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

namespace temail::common {

template<typename Et>
Et
enum_value(const char* name)
{
  static auto enum_meta = QMetaEnum::fromType<Et>();

  bool ok = false;
  auto value = enum_meta.keysToValue(name, &ok);

  if (!ok) {
    qCritical("**TEMAIL INTERNAL**: Failed to convert %s to enum %s !",
              name,
              enum_meta.enumName());
    return static_cast<Et>(0);
  }

  return static_cast<Et>(value);
}

template<typename Et>
TEMAIL_INLINE const char*
enum_name(Et value)
{
  static auto enum_meta = QMetaEnum::fromType<Et>();

  return enum_meta.valueToKey(value);
}

}
