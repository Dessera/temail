/**
 * @file imap.hpp
 * @author Dessera (dessera@qq.com)
 * @brief Temail IMAP4 client.
 * @version 0.1.0
 * @date 2025-08-01
 *
 * @copyright Copyright (c) 2025 Dessera
 *
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <qanystringview.h>
#include <qeventloop.h>
#include <qlist.h>
#include <qmap.h>
#include <qobject.h>
#include <qqueue.h>
#include <qregularexpression.h>
#include <qsslsocket.h>
#include <qtimer.h>
#include <qtmetamacros.h>
#include <qvariant.h>
#include <utility>
#include <vector>

#include "temail/client/base.hpp"
#include "temail/client/request.hpp"
#include "temail/common.hpp"
#include "temail/tag.hpp"

namespace temail::client {

namespace detail {

class IMAPResponse;

}

/**
 * @brief IMAP4 client.
 *
 */
class TEMAIL_PUBLIC IMAP : public Base
{
  Q_OBJECT

public:
  /**
   * @brief Client status types.
   *
   */
  enum Status : uint8_t
  {
    S_DISCONNECT,   /**< Client has been disconnected. */
    S_CONNECT,      /**< Client has been connected. */
    S_AUTHENTICATE, /**< Client has been authenticated. */
  };

  Q_ENUM(Status)

  /**
   * @brief IMAP4 response types.
   *
   */
  enum class Response : uint8_t
  {
    OK,         /**< OK response. */
    NO,         /**< NO response. */
    BAD,        /**< BAD response. */
    PREAUTH,    /**< PREAUTH response. */
    BYE,        /**< BYE response. */
    CAPABILITY, /**< CAPABILITY response. */
    LIST,       /**< LIST response. */
    LSUB,       /**< LSUB response. */
    SEARCH,     /**< SEARCH response. */
    FLAGS,      /**< FLAGS response. */
    EXISTS,     /**< EXISTS response. */
    RECENT,     /**< RECENT response. */
    EXPUNGE,    /**< EXPUNGE response. */
    FETCH,      /**< FETCH response. */
    MAILBOX,    /**< MAILBOX response. */
    COPY,       /**< COPY response. */
    STORE,      /**< STORE response. */
  };

  Q_ENUM(Response)

  /**
   * @brief IMAP4 command types.
   *
   */
  enum class Command : uint8_t
  {
    LOGIN,  /**< LOGIN command. */
    LOGOUT, /**< LOGOUT command. */
    LIST,   /**< LIST command. */
    SELECT, /**< SELECT command. */
    NOOP,   /**< NOOP command. */
    SEARCH, /**< SEARCH command. */
    FETCH,  /**< FETCH command. */
    NOCMD,  /**< No command. */
  };

  Q_ENUM(Command)

  using ResponseHandler =
    std::function<void(detail::IMAPResponse*,
                       std::function<void(ErrorType, const QString&)>,
                       std::function<void(const QVariant&)>)>;

  constexpr static uint16_t PORT_NO_SSL =
    143; /**< Default port when don't using SSL. */
  constexpr static uint16_t PORT_USE_SSL =
    993; /**< Default port when using SSL. */

  constexpr static const char* CRLF = "\r\n"; /**< Crlf. */

  inline static const QMap<request::Fetch::Field, QString> FETCH_FIELD{
    { request::Fetch::SIMPLE, "BODY.PEEK[HEADER]" },
    { request::Fetch::TEXT, "(BODY[HEADER.FIELDS (CONTENT-TYPE)] BODY[1])" }
  };

  static const QMap<Command, ResponseHandler>
    RESPONSE_HANDLER; /**< Response handler map. */

private:
  QSslSocket* _sock;
  QQueue<QVariant> _queue;

  std::unique_ptr<TagGenerator> _tags{ nullptr };

  Status _status{ S_DISCONNECT };

  std::vector<QPair<Command, std::unique_ptr<detail::IMAPResponse>>> _resp;
  QMap<QString, CommandCallback> _resp_callback;

public:
  /**
   * @brief Construct a new IMAP object.
   *
   * @param parent Parent object.
   */
  explicit IMAP(QObject* parent = nullptr);

  ~IMAP() override;

  void connect_to_host(
    const QString& url,
    uint16_t port = 0,
    SslOption ssl = USE_SSL,
    const CommandCallback& callback = _default_read_handler) override;
  void disconnect_from_host(
    const CommandCallback& callback = _default_read_handler) override;
  bool is_connected() override
  {
    return _status == S_CONNECT || _status == S_AUTHENTICATE;
  }
  bool is_disconnected() override { return _status == S_DISCONNECT; }
  void login(const QString& username,
             const QString& password,
             const CommandCallback& callback = _default_read_handler) override;
  void logout(const CommandCallback& callback = _default_read_handler) override;
  void list(const QString& path,
            const QString& pattern,
            const CommandCallback& callback = _default_read_handler) override;
  void select(const QString& path,
              const CommandCallback& callback = _default_read_handler) override;
  void noop(const CommandCallback& callback = _default_read_handler) override;
  void search(request::Search::Criteria criteria,
              const CommandCallback& callback = _default_read_handler) override;
  void fetch(std::size_t id,
             request::Fetch::Field field,
             std::size_t range = 1,
             const CommandCallback& callback = _default_read_handler) override;
  QVariant read() override;

private:
  /**
   * @brief Helper to trig an error.
   *
   * @param err Error type.
   * @param estr Error string.
   */
  TEMAIL_INLINE void _trig_error(ErrorType err, const QString& estr)
  {
    _set_error(err, estr);
    emit error_occurred();
  }

  /**
   * @brief Helper to trig an error.
   *
   * @param err Error type.
   * @param estr Error string.
   */
  TEMAIL_INLINE void _trig_error(ErrorType err, QString&& estr)
  {
    _set_error(err, std::move(estr));
    emit error_occurred();
  }

  /**
   * @brief Helper to send a command.
   *
   * @param type Command type,
   * @param cmd Command content.
   */
  void _request(Command type,
                QAnyStringView cmd,
                const CommandCallback& callback);

private slots: // NOLINT
  /**
   * @brief Handles the tcp socket `connected` signal.
   *
   */
  void _on_connected();

  /**
   * @brief Handles the tcp socket `disconnected` signal.
   *
   */
  void _on_disconnected();

  /**
   * @brief Handles the tcp socket `errorOccurred` signal.
   *
   */
  void _on_error_occurred(QSslSocket::SocketError error);

  /**
   * @brief Handles the tcp socket `readyRead` signal.
   *
   */
  void _on_ready_read();
};

}
