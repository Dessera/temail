#pragma once

#include <cstdint>
#include <memory>
#include <qeventloop.h>
#include <qobject.h>
#include <qregularexpression.h>
#include <qsslsocket.h>
#include <qtextstream.h>
#include <qtimer.h>
#include <qtmetamacros.h>
#include <utility>

#include "temail/common.hpp"
#include "temail/tag.hpp"

namespace temail::clients {

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
  enum SslFlags : uint8_t
  {
    NO_SSL, /**< Do not use SSL. */
    USE_SSL /**< Use SSL. */
  };

  Q_ENUM(SslFlags)

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
    E_UNKNOWN, /**< Unknown error, means no error or some confusing errors. */

    E_TCPINTERNAL,       /**< TCP error, always means that the connection is
                           unavailable. */
    E_UNEXPECTED_STATUS, /**< Unexpected status for unknown reason. */
    E_BADPARAMS,         /**< IMAP params mismatched. */
    E_LOGINFAIL,         /**< Fail to login. */
  };

  Q_ENUM(ErrorType)

  /**
   * @brief IMAP4 response types.
   *
   */
  enum ResponseType : uint8_t
  {
    OK,      /**< OK response. */
    NO,      /**< NO response. */
    BAD,     /**< BAD response. */
    BLANK,   /**< BLANK response. */
    PREAUTH, /**< PREAUTH response. */
  };

  Q_ENUM(ResponseType)

  /**
   * @brief IMAP4 command types.
   *
   */
  enum CommandType : uint8_t
  {
    NOOP,   /**< NOOP command. */
    LOGIN,  /**< LOGIN command. */
    LOGOUT, /**< LOGOUT command. */
    LIST,   /**< LIST command. */
  };

  Q_ENUM(CommandType)

  constexpr static uint16_t PORT_NO_SSL =
    143; /**< Default port when don't using SSL. */
  constexpr static uint16_t PORT_USE_SSL =
    993; /**< Default port when using SSL. */

  constexpr static int TIMEOUT_MSECS = 30000; /**< Default timeout. */

  constexpr static const char* CRLF = "\r\n"; /**< Crlf. */

private:
  QSslSocket* _sock;
  QTextStream _stream;

  std::unique_ptr<TagGenerator> _tags{ nullptr };

  Status _status{ S_DISCONNECT };
  CommandType _last_cmd{ NOOP };

  ErrorType _error{ E_UNKNOWN };
  QString _estr;

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
                       SslFlags ssl = USE_SSL);

  /**
   * @brief Connect to IMAP4 host, will emit `connected` after connection
   * established.
   *
   * @param url Remote url.
   * @param ssl SSL option.
   */
  void connect_to_host(const QString& url, SslFlags ssl);

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
   * @brief Get error flag.
   *
   * @return ErrorFlags Error flag.
   */
  [[nodiscard]] TEMAIL_INLINE auto error() const { return _error; }

  /**
   * @brief Login to IMAP4 server.
   *
   * @param username Username.
   * @param password Password.
   */
  void login(const QString& username, const QString& password);

  /**
   * @brief List folders.
   *
   * @param path Parent path.
   * @param pattern Filter pattern.
   */
  void list(const QString& path, const QString& pattern);

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
  void _send_command(CommandType type, const QString& cmd);

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
