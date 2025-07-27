#pragma once

#include <cstddef>
#include <cstdint>
#include <qlist.h>
#include <qstring.h>
#include <qstringlist.h>
#include <qvariant.h>
#include <utility>

namespace temail::clients::response {

/**
 * @brief LOGIN response.
 *
 */
struct Login
{
  QString message;
};

/**
 * @brief LIST response item.
 *
 */
struct ListItem
{
  QString parent;
  QString name;
  QStringList attrs;
};

/**
 * @brief LIST response.
 *
 */
using List = QList<ListItem>;

struct Select
{
  std::size_t exists{ 0 };
  std::size_t recent{ 0 };
  std::size_t unseen{ 0 };
  std::size_t uidvalidity{ 0 };
  QStringList flags;
  QStringList permanent_flags;
  QString permission;
};

}

Q_DECLARE_METATYPE(temail::clients::response::Login)
Q_DECLARE_METATYPE(temail::clients::response::Select)
