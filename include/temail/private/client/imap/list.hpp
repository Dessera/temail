/**
 * @file search.hpp
 * @author Dessera (dessera@qq.com)
 * @brief IMAP4 LIST response parser.
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

#include "temail/client/imap.hpp"
#include "temail/private/client/imap/response.hpp"

namespace temail::client::detail {

/**
 * @brief Handles IMAP4 LIST response.
 *
 * @param resp Response data.
 * @param error_handler Emitted on error.
 * @param success_handler Emitted on success with value.
 */
void
imap_handle_list(const detail::IMAPResponse& resp,
                 const IMAP::ErrorCallback& error_handler,
                 const IMAP::CommandCallback& success_handler);
}
