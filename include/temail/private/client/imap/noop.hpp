/**
 * @file search.hpp
 * @author Dessera (dessera@qq.com)
 * @brief IMAP4 NOOP response parser.
 * @version 0.1.0
 * @date 2025-08-02
 *
 * @copyright Copyright (c) 2025 Dessera
 *
 */

#pragma once

#include <functional>
#include <qstring.h>
#include <qvariant.h>

#include "temail/private/client/imap/response.hpp"

namespace temail::client::detail {

/**
 * @brief Handles IMAP4 NOOP response.
 *
 * @param resp Response data.
 * @param error_handler Emitted on error.
 * @param success_handler Emitted on success with value.
 */
void
imap_handle_noop(
  IMAPResponse* resp,
  const std::function<void(IMAP::ErrorType, const QString&)>& error_handler,
  const std::function<void(const QVariant&)>& success_handler);

}
