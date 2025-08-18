#include <cstddef>
#include <cstdint>
#include <qbytearray.h>
#include <qdatastream.h>
#include <qdebug.h>
#include <qlist.h>
#include <qlogging.h>
#include <qmap.h>
#include <qpair.h>
#include <qregularexpression.h>
#include <qsslsocket.h>
#include <qstring.h>
#include <qstringview.h>

#include "temail/client/imap.hpp"
#include "temail/common.hpp"
#include "temail/private/client/imap/response.hpp"

#define _forward_false(expr)                                                   \
  {                                                                            \
    if (!(expr)) {                                                             \
      return false;                                                            \
    }                                                                          \
  }

#define _emit_error()                                                          \
  {                                                                            \
    _error = true;                                                             \
    return false;                                                              \
  }

namespace temail::client::detail {

bool
IMAPResponse::digest(const QByteArray& data)
{
  auto stream = QDataStream{ data };

  if (_raw_mode) {
    _forward_false(_handle_raw(stream));
    _raw_mode = false;
  }

  return _handle_command(stream);
}

bool
IMAPResponse::_handle_tagged(const QString& data)
{
  if (auto parsed = TAGGED_REG.match(data); parsed.hasMatch()) {
    _tagged.emplace_back(common::enum_value<IMAP::Response>(
                           parsed.captured("type").toLocal8Bit().constData()),
                         parsed.captured("data"));
    return true;
  }

  qWarning() << "IMAP4 Client| Unhandled response line: " << data;
  _emit_error();
}

bool
IMAPResponse::_handle_untagged(const QString& data, const QDataStream& stream)
{
  if (auto parsed = UNTAGGED_FETCH_REG.match(data); parsed.hasMatch()) {
    bool ok = false;
    _id = parsed.captured("id").toULongLong(&ok);
    if (!ok) {
      qWarning() << "IMAP4 Client| Failed to parse FETCH id: Not a number.";
      _emit_error();
    }

    _forward_false(_handle_raw_meta(parsed.captured("data")));

    if (_bytes_to_read != 0) {
      _raw_mode = true;
      _forward_false(_handle_raw(stream));
      _raw_mode = false;
    }

    return true;
  }

  if (auto parsed = UNTAGGED_REG.match(data); parsed.hasMatch()) {
    _untagged.emplace_back(common::enum_value<IMAP::Response>(
                             parsed.captured("type").toLocal8Bit().constData()),
                           parsed.captured("data"));
    return true;
  }

  if (auto parsed = UNTAGGED_TRAILING_REG.match(data); parsed.hasMatch()) {
    _untagged_trailing.emplace_back(
      common::enum_value<IMAP::Response>(
        parsed.captured("type").toLocal8Bit().constData()),
      parsed.captured("data"));
    return true;
  }

  qWarning() << "IMAP4 Client| Unhandled response line: " << data;
  _emit_error();
}

bool
IMAPResponse::_handle_command(const QDataStream& stream)
{
  while (true) {
    _forward_false(_read_line_to_buffer(stream));

    if (_buffer.startsWith('*')) {
      _forward_false(_handle_untagged(QString{ _buffer }.trimmed(), stream));

      // `connect` returns only an untagged response.
      if (_tag == IMAP::CONNECT_TAG) {
        return true;
      }
    } else if (_buffer.startsWith(_tag.toLocal8Bit())) {
      // all command should ends with a tagged response.
      return _handle_tagged(QString{ _buffer }.trimmed());
    } else {
      qWarning() << "IMAP4 Client| Unhandled response line: " << _buffer;
      _emit_error();
    }
  }
}

bool
IMAPResponse::_handle_raw(const QDataStream& stream)
{
  while (true) {
    if (_bytes_to_read <= 0) {
      _forward_false(_read_line_to_buffer(stream));

      if (_buffer.startsWith(')')) {
        return true;
      }

      _forward_false(_handle_raw_meta(QString{ _buffer }.trimmed()));
    }

    auto nbuf = stream.device()->read(_bytes_to_read);

    _bytes_to_read -= nbuf.size();
    _raw[_id][_field].append(nbuf);

    if (_bytes_to_read > 0) {
      return false;
    }
  }
}

bool
IMAPResponse::_handle_raw_meta(const QString& data)
{
  for (const auto& parsed : PAIRED_FETCH_REG.globalMatch(data)) {
    // skip NIL
    if (!parsed.hasCaptured("size")) {
      continue;
    }

    bool ok = false;
    auto bsize = parsed.captured("size").toLongLong(&ok);
    if (!ok) {
      qWarning() << "IMAP4 Client| Failed to parse FETCH size: Not a number.";
      _emit_error();
    }

    // multiline data: return to _handle_raw
    if (!parsed.hasCaptured("data")) {
      _bytes_to_read = bsize;
      _field = parsed.captured("field");
      return true;
    }

    // inline data: save
    _raw[_id][parsed.captured("field")] = parsed.captured("data").toLocal8Bit();
  }

  return true;
}

bool
IMAPResponse::_read_line_to_buffer(const QDataStream& stream)
{
  if (!_buffer.endsWith("\r\n")) {
    _buffer.append(stream.device()->readLine());
  } else {
    _buffer = stream.device()->readLine();
  }

  if (_buffer.isEmpty()) {
    qWarning() << "IMAP4 Client| Failed to parse response: Unexpected EOF.";
    _emit_error();
  }

  if (!_buffer.endsWith("\r\n")) {
    return false;
  }

  return true;
}

}
