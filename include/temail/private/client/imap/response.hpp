/**
 * @file response.hpp
 * @author Dessera (dessera@qq.com)
 * @brief IMAP4 response parser.
 * @version 0.1.0
 * @date 2025-08-01
 *
 * @copyright Copyright (c) 2025 Dessera
 *
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <qbytearray.h>
#include <qdatastream.h>
#include <qlist.h>
#include <qmap.h>
#include <qpair.h>
#include <qregularexpression.h>
#include <qsslsocket.h>
#include <qstring.h>
#include <qtypes.h>
#include <utility>

#include "temail/client/imap.hpp"
#include "temail/common.hpp"

namespace temail::client::detail {

/**
 * @brief IMAP4 response parser.
 *
 */
class IMAPResponse
{
public:
  inline static QRegularExpression TAGGED_REG{
    R"REGEX((?P<tag>[A-Z]\d+) (?P<type>[A-Z]+) (?P<data>.*))REGEX"
  }; /**< Regex to parse tagged response. */

  inline static QRegularExpression UNTAGGED_REG{
    R"REGEX(\* (?P<type>[A-Z-]+)( (?P<data>.*))?)REGEX"
  }; /**< Regex to parse untagged response. */

  inline static QRegularExpression UNTAGGED_TRAILING_REG{
    R"REGEX(\* (?P<data>.*) (?P<type>[A-Z-]+))REGEX"
  }; /**< Regex to parse untagged trailing response. */

  inline static QRegularExpression UNTAGGED_FETCH_REG{
    R"REGEX(\* (?P<id>[0-9]+) FETCH \((?P<data>.*)(\))?)REGEX"
  }; /**< Regex to parse first FETCH response. */

  /**
   * @brief Regex to parse FETCH paired response.
   *
   * @note - BODY[1.MIME] NIL => (field BODY[1.MIME])
   *       - BODY[HEADER.FIELDS (CONTENT-TYPE)] {12} => (field
   * BODY[HEADER.FIELDS (CONTENT-TYPE)]) (size 12)
   */
  inline static QRegularExpression PAIRED_FETCH_REG{
    R"REGEX(\s?(?P<field>[A-Za-z0-9-\[\]\(\)\.\s]+?) (NIL|{(?P<size>[0-9]+)}(\s(?P<data>[\s\S]*))?)\s?)REGEX"
  };

private:
  QString _tag;

  bool _raw_mode{ false };
  QByteArray _buffer{ "\r\n" };

  std::size_t _id{ 0 };
  qint64 _bytes_to_read{ 0 };
  QString _field;

  bool _error{ false };

  QList<QPair<IMAP::Response, QString>> _tagged;
  QList<QPair<IMAP::Response, QString>> _untagged;
  QList<QPair<IMAP::Response, QString>> _untagged_trailing;
  QMap<std::size_t, QMap<QString, QByteArray>> _raw;

public:
  /**
   * @brief Construct a new IMAPResponse object.
   *
   * @param sock Tcp socket.
   * @param tag Request tag.
   */
  IMAPResponse(QString tag)
    : _tag{ std::move(tag) }
  {
  }

  /**
   * @brief Digest input data.
   *
   * @return true Succesfully parsed data.
   * @return false Need more input or error occurred.
   */
  bool digest(const QByteArray& data);

  /**
   * @brief Get error flag.
   *
   * @return true Error occurred.
   * @return false No error.
   */
  [[nodiscard]] TEMAIL_INLINE auto error() const { return _error; }

  /**
   * @brief Get tagged response.
   *
   * @return const QList<QPair<IMAP::Response, QString>>& Tagged response with
   * code and data pair.
   */
  [[nodiscard]] TEMAIL_INLINE auto& tagged() const { return _tagged; }

  /**
   * @brief Get untagged response.
   *
   * @return const QList<QPair<IMAP::Response, QString>>& Untagged response
   * with code and data pair.
   */
  [[nodiscard]] TEMAIL_INLINE auto& untagged() const { return _untagged; }

  /**
   * @brief Get untagged trailing response (such as EXISTS or RECENT).
   *
   * @return const QList<QPair<IMAP::Response, QString>>& Untagged trailing
   * response with code and data pair.
   */
  [[nodiscard]] TEMAIL_INLINE auto& untagged_trailing() const
  {
    return _untagged_trailing;
  }

  /**
   * @brief Get raw response.
   *
   * @return const QMap<std::size_t, QMap<QString, QByteArray>>& Map of mail
   * id and a submap, which contains fetch field and data.
   */
  [[nodiscard]] TEMAIL_INLINE auto& raw() const { return _raw; }

  /**
   * @brief Get response tag.
   *
   * @return const QString& response tag.
   */
  [[nodiscard]] TEMAIL_INLINE auto& tag() const { return _tag; }

private:
  /**
   * @brief Handles command input data.
   *
   * @param stream Data stream.
   * @return true Successfully parsed command data.
   * @return false Need more input or error occurred.
   */
  bool _handle_command(const QDataStream& stream);

  /**
   * @brief Handles raw input data.
   *
   * @param stream Data stream.
   * @return true Successfully parsed raw data.
   * @return false Need more input or error occurred.
   */
  bool _handle_raw(const QDataStream& stream);

  /**
   * @brief Handles tagged input data.
   *
   * @param data Data string.
   * @return true Successfully parsed untagged data.
   * @return false Error occurred.
   */
  bool _handle_tagged(const QString& data);

  /**
   * @brief Handles untagged input data.
   *
   * @param data Data string.
   * @param stream Data stream.
   * @return true Successfully parsed untagged data.
   * @return false Need more input or error occurred.
   */
  bool _handle_untagged(const QString& data, const QDataStream& stream);

  /**
   * @brief Handles raw paired input data.
   *
   * @param data Data string.
   * @return true Successfully parsed raw data.
   * @return false Error occurred.
   */
  bool _handle_raw_meta(const QString& data);

  /**
   * @brief Try to Read a line into buffer.
   *
   * @param stream Data stream.
   * @return true Read successfully.
   * @return false Failed to read a line (EOF or error).
   */
  bool _read_line_to_buffer(const QDataStream& stream);
};

}
