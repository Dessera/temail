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
#include <qdatetime.h>
#include <qdebug.h>
#include <qlist.h>
#include <qmap.h>
#include <qstring.h>
#include <qstringlist.h>
#include <qvariant.h>
#include <utility>

#include "temail/client/request.hpp"
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

struct FetchEnvelope
{
  QDateTime date;
  QString from;
  QString to;
  QString subject;
};

struct FetchContentType
{
  QString content_type;
  QString charset;
};

/**
 * @brief Fetch response.
 *
 */
using Fetch = QList<QMap<request::Fetch::Field, QVariant>>;

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

Q_DECLARE_METATYPE(temail::client::response::FetchEnvelope)

TEMAIL_INLINE QDebug&
operator<<(QDebug& dbg, const temail::client::response::FetchEnvelope& response)
{
  return dbg.noquote()
         << QString{ "FetchEnvelope[date: %1, from: %2, to: %3, subject: %4]" }
              .arg(response.date.toString())
              .arg(response.from)
              .arg(response.to)
              .arg(response.subject);
}

Q_DECLARE_METATYPE(temail::client::response::FetchContentType)

TEMAIL_INLINE QDebug&
operator<<(QDebug& dbg,
           const temail::client::response::FetchContentType& response)
{
  return dbg.noquote()
         << QString{ "FetchContentType[content_type: %1, charset: %2]" }
              .arg(response.content_type)
              .arg(response.charset);
}
