#pragma once

#include <cstdint>
#include <qobject.h>
#include <qtmetamacros.h>

#include "temail/common.hpp"

namespace temail::client::request {

/**
 * @brief Search criteria, all descriptions are from RFC1730.
 *
 */
class TEMAIL_PUBLIC Search : public QObject
{
  Q_OBJECT

public:
  /**
   * @brief Search criteria.
   *
   */
  enum Criteria : uint8_t
  {
    ALL,      /**< All messages in the mailbox. */
    ANSWERED, /**< Messages with the \Answered flag set. */
    DELETED,  /**< Messages with the \Deleted flag set. */
    DRAFT,    /**< Messages with the \Draft flag set. */
    FLAGGED,  /**< Messages with the \Flagged flag set. */
    NEW, /**< Messages that have the \Recent flag set but not the \Seen flag. */
    OLD, /**< Messages that do not have the \Recent flag set. */
    RECENT,     /**< Messages that have the \Recent flag set. */
    SEEN,       /**< Messages that have the \Seen flag set. */
    UNANSWERED, /**< Messages that do not have the \Answered flag set. */
    UNDELETED,  /**< Messages that do not have the \Deleted flag set. */
    UNDRAFT,    /**< Messages that do not have the \Draft flag set. */
    UNFLAGGED,  /**< Messages that do not have the \Flagged flag set. */
    UNSEEN,     /**< Messages that do not have the \Seen flag set. */
  };

  Q_ENUM(Criteria)
};

}
