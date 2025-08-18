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
#include <deque>
#include <functional>
#include <memory>
#include <qanystringview.h>
#include <qeventloop.h>
#include <qlist.h>
#include <qmap.h>
#include <qmutex.h>
#include <qobject.h>
#include <qqueue.h>
#include <qregularexpression.h>
#include <qsslsocket.h>
#include <qtimer.h>
#include <qtmetamacros.h>
#include <queue>
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
  enum class Status : uint8_t
  {
    DISCONNECT,   /**< Client has been disconnected. */
    CONNECT,      /**< Client has been connected. */
    AUTHENTICATE, /**< Client has been authenticated. */
  };

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

  using ResponseHandler = std::function<
    void(const detail::IMAPResponse&, ErrorCallback, CommandCallback)>;

  constexpr static uint16_t PORT_NO_SSL =
    143; /**< Default port when don't using SSL. */
  constexpr static uint16_t PORT_USE_SSL =
    993; /**< Default port when using SSL. */

  inline static const QString CONNECT_TAG =
    "CONNECT"; /**< Response tag used by connect. */
  inline static const QString DISCONNECT_TAG =
    "DISCONNECT"; /**< Response tag used by disconnect. */

  inline static const QMap<request::Fetch::Field, QString> FETCH_FIELD{
    { request::Fetch::ENVELOPE,
      "BODY.PEEK[HEADER.FIELDS (DATE SUBJECT FROM TO)]" },
    { request::Fetch::MIME,
      "BODY.PEEK[HEADER.FIELDS (CONTENT-TYPE)] BODY.PEEK[1.MIME]" },
    { request::Fetch::TEXT, "BODY[1]" }
  }; /**< Request fetch field to command map. */

  static const QMap<Command, ResponseHandler>
    RESPONSE_HANDLER; /**< Response handler map. */

private:
  QSslSocket _sock;
  std::queue<QVariant> _queue;
  Status _status{ Status::DISCONNECT };

  TagGenerator _tags;
  std::deque<QPair<Command, detail::IMAPResponse>> _resp;
  QMap<QString, QPair<CommandCallback, ErrorCallback>> _resp_cb;

  QMutex _resp_lock;    /**< Lock to ensure thread safe of `_resp`. */
  QMutex _request_lock; /**< Lock to ensure sync enqueue (to `_resp`), because
                           `flush` operation may fail, maybe we need to pop
                           immediately after push. */
  QMutex _read_lock;    /**> Lock to ensure thread safe of `read`. */
  QMutex _cb_lock;      /**> Lock to ensure thread safe of callback map.  */

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
    const CommandCallback& callback = _default_command_handler) override;
  void disconnect_from_host(
    const CommandCallback& callback = _default_command_handler) override;
  bool is_connected() override
  {
    return _status == Status::CONNECT || _status == Status::AUTHENTICATE;
  }
  bool is_disconnected() override { return _status == Status::DISCONNECT; }
  void login(
    const QString& username,
    const QString& password,
    const CommandCallback& callback = _default_command_handler) override;
  void logout(
    const CommandCallback& callback = _default_command_handler) override;
  void list(
    const QString& path,
    const QString& pattern,
    const CommandCallback& callback = _default_command_handler) override;
  void select(
    const QString& path,
    const CommandCallback& callback = _default_command_handler) override;
  void noop(
    const CommandCallback& callback = _default_command_handler) override;
  void search(
    request::Search::Criteria criteria,
    const CommandCallback& callback = _default_command_handler) override;
  void fetch(
    std::size_t id,
    request::Fetch::FieldFlags field,
    std::size_t range = 1,
    const CommandCallback& callback = _default_command_handler) override;
  QVariant read() override;

private:
  /**
   * @brief Helper to send a command.
   *
   * @param type Command type,
   * @param cmd Command content.
   * @param callback Success callback.
   */
  void _request(Command type,
                QAnyStringView cmd,
                const CommandCallback& callback);

  /**
   * @brief Set error for specific command.
   *
   * @param tag Command tag.
   * @param error Error type.
   * @param estr Error string.
   */
  void _tag_error(const QString& tag, ErrorType error, const QString& estr);

  /**
   * @brief Handles success callback.
   *
   * @param tag Command tag.
   * @param data Response data.
   */
  void _handle_success(const QString& tag, const QVariant& data);

  /**
   * @brief Handles error callback.
   *
   * @param tag Command tag.
   * @param error Error type.
   * @param estr Error string.
   */
  void _handle_error(const QString& tag, ErrorType error, const QString& estr);

  /**
   * @brief Add command handlers.
   *
   * @param tag Command tag.
   * @param success Success handler.
   * @param error Error handler.
   */
  void _add_handler(const QString& tag,
                    const CommandCallback& success,
                    const ErrorCallback& error);

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
