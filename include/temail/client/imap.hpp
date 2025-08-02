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
#include <qobject.h>
#include <qqueue.h>
#include <qregularexpression.h>
#include <qsslsocket.h>
#include <qtimer.h>
#include <qtmetamacros.h>
#include <qvariant.h>
#include <utility>
#include <vector>

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
class TEMAIL_PUBLIC IMAP : public QObject
{
  Q_OBJECT

public:
  /**
   * @brief SSL option.
   *
   */
  enum SslOption : uint8_t
  {
    NO_SSL, /**< Do not use SSL. */
    USE_SSL /**< Use SSL. */
  };

  Q_ENUM(SslOption)

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
   * @brief IMAP4 error types.
   *
   */
  enum ErrorType : uint8_t
  {
    E_NOERR,        /**< No error. */
    E_UNKNOWN,      /**< Unknown error. */
    E_TCPINTERNAL,  /**< TCP error, always means that the connection is
                      unavailable. */
    E_UNEXPECTED,   /**< Unexpected status for unknown reason. */
    E_NOTCONNECTED, /**< IMAP4 host not connected. */
    E_BADCOMMAND,   /**< IMAP4 invalid command or params mismatched. */
    E_LOGIN,        /**< Failed to login for any reason. */
    E_REFERENCE,    /**< Failed to inspect reference or name. */
    E_PARSE,        /**< Failed to parse response. */
  };

  Q_ENUM(ErrorType)

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

  constexpr static int TIMEOUT_MSECS = 30000; /**< Default timeout. */

  constexpr static const char* CRLF = "\r\n"; /**< Crlf. */

  static const QMap<Command, ResponseHandler>
    RESPONSE_HANDLER; /**< Response handler map. */

private:
  QSslSocket* _sock;
  QQueue<QVariant> _queue;

  std::unique_ptr<TagGenerator> _tags{ nullptr };

  Status _status{ S_DISCONNECT };

  ErrorType _error{ E_NOERR };
  QString _estr;
  std::vector<QPair<Command, std::unique_ptr<detail::IMAPResponse>>> _resp;

public:
  /**
   * @brief Construct a new IMAP object.
   *
   * @param parent Parent object.
   */
  explicit IMAP(QObject* parent = nullptr);

  ~IMAP() override;

  /**
   * @brief Connect to IMAP4 host, will emit `connected` after connection
   * established.
   *
   * @param url Remote url.
   * @param port Remote port.
   * @param ssl SSL option.
   */
  void connect_to_host(const QString& url,
                       uint16_t port = 0,
                       SslOption ssl = USE_SSL);

  /**
   * @brief Connect to IMAP4 host, will emit `connected` after connection
   * established.
   *
   * @param url Remote url.
   * @param ssl SSL option.
   */
  void connect_to_host(const QString& url, SslOption ssl);

  /**
   * @brief Disconnect from IMAP4 host.
   *
   */
  void disconnect_from_host();

  /**
   * @brief Check if connection is established.
   *
   * @return true Client has connected.
   * @return false Client has not connected.
   */
  [[nodiscard]] TEMAIL_INLINE bool is_connected() const
  {
    return _status == S_CONNECT || _status == S_AUTHENTICATE;
  }

  /**
   * @brief Check if client has disconnected from the host.
   *
   * @return true Client has disconnected.
   * @return false Client has not disconnected.
   */
  [[nodiscard]] TEMAIL_INLINE bool is_disconnected() const
  {
    return _status == S_DISCONNECT;
  }

  /**
   * @brief Wait for `connected` signal.
   *
   * @param msecs Timeout.
   * @return true Signal emitted.
   * @return false Error occurred when waiting for signal.
   */
  bool wait_for_connected(int msecs = TIMEOUT_MSECS);

  /**
   * @brief Wait for `disconnected` signal.
   *
   * @param msecs Timeout.
   * @return true Signal emitted.
   * @return false Error occurred when waiting for signal.
   */
  bool wait_for_disconnected(int msecs = TIMEOUT_MSECS);

  /**
   * @brief Wait for `ready_read` signal.
   *
   * @param msecs Timeout.
   * @return true Signal emitted.
   * @return false Error occurred when waiting for signal.
   */
  bool wait_for_ready_read(int msecs = TIMEOUT_MSECS);

  /**
   * @brief Get error string.
   *
   * @return QString Error string.
   */
  [[nodiscard]] QString error_string() const;

  /**
   * @brief Get error status.
   *
   * @return ErrorType Error status.
   */
  [[nodiscard]] TEMAIL_INLINE auto error() const { return _error; }

  /**
   * @brief Reset error status.
   *
   */
  TEMAIL_INLINE void reset_error()
  {
    _error = E_NOERR;
    _estr.clear();
  }

  /**
   * @brief Login to IMAP4 server.
   *
   * @param username Username.
   * @param password Password.
   */
  void login(const QString& username, const QString& password);

  /**
   * @brief Logout from IMAP4 server.
   *
   */
  void logout();

  /**
   * @brief List folders.
   *
   * @param path Parent path.
   * @param pattern Filter pattern.
   */
  void list(const QString& path, const QString& pattern);

  /**
   * @brief Select folder.
   *
   * @param path Folder path.
   */
  void select(const QString& path);

  /**
   * @brief No op.
   *
   */
  void noop();

  /**
   * @brief Search mails.
   *
   * @param criteria Search criteria.
   * @note This method only implements a subset of IMAP4 SEARCH, including any
   * criteria with no params.
   */
  void search(request::Search::Criteria criteria);

  /**
   * @brief Fetch mail info.
   *
   * @param id Mail id.
   * @param field Fetch field.
   * @note This method only fetch simple parts (such as BODY[HEADER] and
   * BODY[1]).
   */
  void fetch(std::size_t id, request::Fetch::Field field);

  /**
   * @brief Read response.
   *
   * @return QVariant Response.
   */
  QVariant read();

private:
  /**
   * @brief Wait for specific signal.
   *
   * @tparam Ef Signal type.
   * @param msecs Timeout.
   * @param func Signal function.
   */
  template<typename Ef>
  void _wait_for_event(int msecs, Ef&& func)
  {
    auto loop = QEventLoop{};
    connect(this, std::forward<Ef>(func), &loop, &QEventLoop::quit);
    connect(this, &IMAP::error_occurred, &loop, &QEventLoop::quit);

    if (msecs > 0) {
      auto* timer = new QTimer{ &loop };
      timer->setSingleShot(true);
      connect(timer, &QTimer::timeout, &loop, &QEventLoop::quit);

      timer->start(msecs);
    }

    loop.exec();
  }

  /**
   * @brief Helper to trig an error.
   *
   * @param err Error type.
   */
  TEMAIL_INLINE void _trig_error(ErrorType err)
  {
    _error = err;
    emit error_occurred();
  }

  /**
   * @brief Helper to trig an error.
   *
   * @param err Error type.
   */
  TEMAIL_INLINE void _trig_error(ErrorType err, QString estr)
  {
    _estr = std::move(estr);
    _trig_error(err);
  }

  /**
   * @brief Helper to send a command.
   *
   * @param type Command type,
   * @param cmd Command content.
   */
  void _send_command(Command type, QAnyStringView cmd);

signals:
  /**
   * @brief Emiteed when client has connected to the host.
   *
   */
  void connected();

  /**
   * @brief Emiteed when client has disconnected to the host.
   *
   */
  void disconnected();

  /**
   * @brief Emitted when ready to read.
   *
   */
  void ready_read();

  /**
   * @brief Emiteed when error occurred in client.
   *
   */
  void error_occurred();

private slots:
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
