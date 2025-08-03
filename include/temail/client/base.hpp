/**
 * @file base.hpp
 * @author Dessera (dessera@qq.com)
 * @brief Base mail client.
 * @version 0.1.0
 * @date 2025-08-02
 *
 * @copyright Copyright (c) 2025 Dessera
 *
 */

#pragma once

#include <cstddef>
#include <functional>
#include <qeventloop.h>
#include <qobject.h>
#include <qstring.h>
#include <qtimer.h>
#include <qvariant.h>

#include "temail/client/request.hpp"
#include "temail/common.hpp"

namespace temail::client {

/**
 * @brief Base mail client.
 *
 */
class TEMAIL_PUBLIC Base : public QObject
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
   * @brief Client error types.
   *
   */
  enum ErrorType : uint8_t
  {
    E_NOERR,     /**< No error. */
    E_UNKNOWN,   /**< Unknown error. */
    E_DUPLICATE, /**< Duplicate operation. */
    E_INTERNAL,  /**< Transport layer error, always means that the connection is
                    unavailable. */
    E_UNEXPECTED,   /**< Unexpected status for unknown reason. */
    E_NOTCONNECTED, /**< Server not connected. */
    E_BADCOMMAND,   /**< Invalid command or params mismatched. */
    E_LOGIN,        /**< Failed to login for any reason. */
    E_REFERENCE,    /**< Failed to inspect reference or name. */
    E_PARSE,        /**< Failed to parse response. */
  };

  Q_ENUM(ErrorType)

  using CommandCallback = std::function<void(const QVariant&)>;

  constexpr static int TIMEOUT_MSECS = 30000; /**< Default timeout. */

private:
  ErrorType _error{ E_NOERR };
  QString _estr;

public:
  explicit Base(QObject* parent = nullptr)
    : QObject{ parent }
  {
  }

  ~Base() override = default;

  /**
   * @brief Connect to server.
   *
   * @param url Remote url.
   * @param port Remote port.
   * @param ssl SSL option.
   * @param callback Success callback.
   */
  virtual void connect_to_host(
    const QString& url,
    uint16_t port = 0,
    SslOption ssl = USE_SSL,
    const CommandCallback& callback = _default_command_handler) = 0;

  /**
   * @brief Connect to IMAP4 host.
   *
   * @param url Remote url.
   * @param ssl SSL option.
   * @param callback Success callback.
   */
  TEMAIL_INLINE void connect_to_host(
    const QString& url,
    SslOption ssl,
    const CommandCallback& callback = _default_command_handler)
  {
    connect_to_host(url, 0, ssl, callback);
  }

  /**
   * @brief Disconnect from IMAP4 host.
   *
   * @param callback Success callback.
   */
  virtual void disconnect_from_host(
    const CommandCallback& callback = _default_command_handler) = 0;

  /**
   * @brief Check if connection is established.
   *
   * @return true Client has connected.
   * @return false Client has not connected.
   */
  virtual bool is_connected() = 0;

  /**
   * @brief Check if client has disconnected from the host.
   *
   * @return true Client has disconnected.
   * @return false Client has not disconnected.
   */
  virtual bool is_disconnected() = 0;

  /**
   * @brief Login to server.
   *
   * @param username Username.
   * @param password Password.
   * @param callback Success callback.
   */
  virtual void login(
    const QString& username,
    const QString& password,
    const CommandCallback& callback = _default_command_handler) = 0;

  /**
   * @brief Logout from server.
   *
   * @param callback Success callback.
   */
  virtual void logout(
    const CommandCallback& callback = _default_command_handler) = 0;

  /**
   * @brief List folders.
   *
   * @param path Parent path.
   * @param pattern Filter pattern.
   * @param callback Success callback.
   */
  virtual void list(
    const QString& path,
    const QString& pattern,
    const CommandCallback& callback = _default_command_handler) = 0;

  /**
   * @brief Select folder.
   *
   * @param path Folder path.
   * @param callback Success callback.
   */
  virtual void select(
    const QString& path,
    const CommandCallback& callback = _default_command_handler) = 0;

  /**
   * @brief No op.
   *
   * @param callback Success callback.
   */
  virtual void noop(
    const CommandCallback& callback = _default_command_handler) = 0;

  /**
   * @brief Search mails from server.
   *
   * @param criteria Search criteria.
   * @param callback Success callback.
   */
  virtual void search(
    request::Search::Criteria criteria,
    const CommandCallback& callback = _default_command_handler) = 0;

  /**
   * @brief Fetch mails from server.
   *
   * @param id Mail start id.
   * @param field Mail field.
   * @param range Id range.
   * @param callback Success callback.
   */
  virtual void fetch(
    std::size_t id,
    request::Fetch::Field field,
    std::size_t range = 1,
    const CommandCallback& callback = _default_command_handler) = 0;

  /**
   * @brief Read response.
   *
   * @return QVariant Response.
   */
  virtual QVariant read() = 0;

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
  [[nodiscard]] TEMAIL_INLINE auto& error_string() const { return _estr; }

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

protected:
  static void _default_command_handler(const QVariant& /*data*/) {}

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
    connect(this, &Base::error_occurred, &loop, &QEventLoop::quit);

    if (msecs > 0) {
      auto* timer = new QTimer{ &loop };
      timer->setSingleShot(true);
      connect(timer, &QTimer::timeout, &loop, &QEventLoop::quit);

      timer->start(msecs);
    }

    loop.exec();
  }

  /**
   * @brief Set error status.
   *
   * @param type Error status.
   * @param estr Error string.
   */
  TEMAIL_INLINE void _set_error(ErrorType type, const QString& estr)
  {
    _error = type;
    _estr = estr;
  }

  /**
   * @brief Set error status.
   *
   * @param type Error status.
   * @param estr Error string.
   */
  TEMAIL_INLINE void _set_error(ErrorType type, QString&& estr)
  {
    _error = type;
    _estr = std::move(estr);
  }

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
};

}
