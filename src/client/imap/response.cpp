#include <cstddef>
#include <cstdint>
#include <qbytearray.h>
#include <qdebug.h>
#include <qlist.h>
#include <qlogging.h>
#include <qmap.h>
#include <qpair.h>
#include <qregularexpression.h>
#include <qsslsocket.h>
#include <qstring.h>
#include <qtypes.h>

#include "temail/client/imap.hpp"
#include "temail/common.hpp"
#include "temail/private/client/imap/response.hpp"

namespace temail::client::detail {

bool
IMAPResponse::digest()
{
  if (_raw_mode) {
    if (!_handle_raw()) {
      return false;
    }

    _raw_mode = false;
  }

  return _handle_command();
}

bool
IMAPResponse::_handle_tagged()
{
  auto bufstr = QString{ _buffer };

  if (auto parsed = TAGGED_REG.match(bufstr); parsed.hasMatch()) {
    _tagged.emplace_back(common::enum_value<IMAP::Response>(
                           parsed.captured("type").toLocal8Bit().constData()),
                         parsed.captured("data"));
    return true;
  }

  qWarning() << "IMAP4 Client: Unhandled response line: " << bufstr;

  _error = true;
  return false;
}

bool
IMAPResponse::_handle_untagged()
{
  auto bufstr = QString{ _buffer };

  if (auto parsed = UNTAGGED_FETCH_REG.match(bufstr); parsed.hasMatch()) {
    bool ok = false;
    _id = parsed.captured("id").toULongLong(&ok);
    if (!ok) {
      qWarning() << "IMAP4 Client: Failed to parse FETCH id: Not a number.";

      _error = true;
      return false;
    }

    _bytes_to_read = parsed.captured("size").toLongLong(&ok);
    if (!ok) {
      qWarning() << "IMAP4 Client: Failed to parse FETCH size: Not a number.";

      _error = true;
      return false;
    }

    _field = parsed.captured("field");

    _raw_mode = true;
    if (!_handle_raw()) {
      return false;
    }

    _raw_mode = false;
    return true;
  }

  if (auto parsed = UNTAGGED_REG.match(bufstr); parsed.hasMatch()) {
    _untagged.emplace_back(common::enum_value<IMAP::Response>(
                             parsed.captured("type").toLocal8Bit().constData()),
                           parsed.captured("data"));
    return true;
  }

  if (auto parsed = UNTAGGED_TRAILING_REG.match(bufstr); parsed.hasMatch()) {
    _untagged_trailing.emplace_back(
      common::enum_value<IMAP::Response>(
        parsed.captured("type").toLocal8Bit().constData()),
      parsed.captured("data"));
    return true;
  }

  qWarning() << "IMAP4 Client: Unhandled response line: " << bufstr;

  _error = true;
  return false;
}

bool
IMAPResponse::_handle_command()
{
  while (true) {
    if (!_buffer.endsWith("\r\n")) {
      _buffer.append(_sock->readLine());
    } else {
      _buffer = _sock->readLine();
    }

    if (_buffer.isEmpty()) {
      return true;
    }

    if (!_buffer.endsWith("\r\n")) {
      return false;
    }

    if (_buffer.startsWith('*')) {
      if (!_handle_untagged()) {
        return false;
      }

      continue;
    }

    if (_buffer.startsWith(_tag.toLocal8Bit())) {
      if (!_handle_tagged()) {
        return false;
      }

      continue;
    }

    qWarning() << "IMAP4 Client: Unhandled response line: " << _buffer;

    _error = true;
    return false;
  }
}

bool
IMAPResponse::_handle_raw() // NOLINT
{
  while (true) {
    // fetch next field header.
    if (_bytes_to_read <= 0) {
      // read next line.
      if (!_buffer.endsWith("\r\n")) {
        _buffer.append(_sock->readLine());
      } else {
        _buffer = _sock->readLine();
      }

      // empty, should not happened.
      if (_buffer.isEmpty()) {
        _error = true;
        return false;
      }

      // need more data.
      if (!_buffer.endsWith("\r\n")) {
        return false;
      }

      // `)` means end of fetch.
      if (_buffer.startsWith(')')) {
        return true;
      }

      // update raw info, or refuse to parse.
      if (auto parsed = MULTI_FETCH_REG.match(QString{ _buffer });
          parsed.hasMatch()) {
        bool ok = false;
        _bytes_to_read = parsed.captured("size").toLongLong(&ok);
        if (!ok) {
          qWarning() << "IMAP4 Client: Failed to parse MULTI FETCH response "
                        "size: Not a number.";
          _error = true;
          return false;
        }

        _field = parsed.captured("field");
      } else {
        qWarning() << "IMAP4 Client: Unhandled response line: " << _buffer;

        _error = true;
        return false;
      }
    }

    // read next data.

    auto nbuf = _sock->read(_bytes_to_read);

    _bytes_to_read -= nbuf.size();
    _raw[_id][_field].append(nbuf);

    if (_bytes_to_read > 0) {
      return false;
    }
  }
}

}
