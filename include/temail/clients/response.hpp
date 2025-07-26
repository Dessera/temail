#pragma once

#include <cstdint>
#include <qlist.h>
#include <qstring.h>
#include <qstringlist.h>
#include <qvariant.h>
#include <utility>

#include "temail/common.hpp"

namespace temail::clients::response {

class TEMAIL_PUBLIC Login
{
private:
  QString _msg;

public:
  explicit Login(QString msg)
    : _msg{ std::move(msg) }
  {
  }

  [[nodiscard]] TEMAIL_INLINE auto& message() const { return _msg; }
};

class TEMAIL_PUBLIC ListItem
{
private:
  QString _parent;
  QString _name;

public:
  ListItem(QString parent, QString name)
    : _parent{ std::move(parent) }
    , _name{ std::move(name) }
  {
  }
  [[nodiscard]] TEMAIL_INLINE auto& parent() const { return _parent; }

  [[nodiscard]] TEMAIL_INLINE auto& name() const { return _name; }
};

using List = QList<ListItem>;

}

Q_DECLARE_METATYPE(temail::clients::response::Login)
