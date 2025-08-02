/**
 * @file common.hpp
 * @author Dessera (dessera@qq.com)
 * @brief Temail common utils.
 * @version 0.1.0
 * @date 2025-08-02
 *
 * @copyright Copyright (c) 2025 Dessera
 *
 */

#pragma once

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

/**
 * @brief Convert string to qt enum.
 *
 * @tparam Et Enum type.
 * @param name Enum name.
 * @return Et Enum value.
 */
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

/**
 * @brief Convert qt enum to string.
 *
 * @tparam Et Enum type.
 * @param value Enum value.
 * @return const char* Enum name.
 */
template<typename Et>
TEMAIL_INLINE const char*
enum_name(Et value)
{
  static auto enum_meta = QMetaEnum::fromType<Et>();

  return enum_meta.valueToKey(value);
}

}
