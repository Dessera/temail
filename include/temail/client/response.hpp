/**
 * @file response.hpp
 * @author Dessera (dessera@qq.com)
 * @brief Temail response types.
 * @version 0.1.0
 * @date 2025-08-02
 *
 * @copyright Copyright (c) 2025 Dessera
 *
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <qdebug.h>
#include <qlist.h>
#include <qstring.h>
#include <qstringlist.h>
#include <qvariant.h>
#include <utility>

#include "temail/common.hpp"

namespace temail::client::response {

/**
 * @brief LOGIN response.
 *
 */
struct Login
{};

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

/**
 * @brief SELECT response.
 *
 */
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

/**
 * @brief NOOP response.
 *
 */
struct Noop
{};

/**
 * @brief Search response.
 *
 */
using Search = QList<std::size_t>;

}

Q_DECLARE_METATYPE(temail::client::response::Login)

TEMAIL_INLINE QDebug&
operator<<(QDebug& dbg, const temail::client::response::Login& /*response*/)
{
  return dbg.noquote() << "Login";
}

TEMAIL_INLINE QDebug&
operator<<(QDebug& dbg, const temail::client::response::ListItem& response)
{
  return dbg.noquote() << QString{ "ListItem[parent: %1, name: %2]" }
                            .arg(response.parent)
                            .arg(response.name);
}

Q_DECLARE_METATYPE(temail::client::response::Select)

TEMAIL_INLINE QDebug&
operator<<(QDebug& dbg, const temail::client::response::Select& response)
{
  return dbg.noquote() << QString{ "Select[exists: %1, recent: %2, unseen: %3, "
                                   "uidvalidity: %4, permission: %5]" }
                            .arg(response.exists)
                            .arg(response.recent)
                            .arg(response.unseen)
                            .arg(response.uidvalidity)
                            .arg(response.permission);
}

Q_DECLARE_METATYPE(temail::client::response::Noop)

TEMAIL_INLINE QDebug&
operator<<(QDebug& dbg, const temail::client::response::Noop& /*response*/)
{
  return dbg.noquote() << "Noop";
}
