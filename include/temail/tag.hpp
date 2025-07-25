/**
 * @file tag.hpp
 * @author Dessera (dessera@qq.com)
 * @brief Utils to generate IMAP4 tags.
 * @version 0.1.0
 * @date 2025-07-24
 *
 * @copyright Copyright (c) 2025 Dessera
 *
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <qstring.h>
#include <random>

#include "temail/common.hpp"

namespace temail {

/**
 * @brief Tag generator.
 *
 */
class TEMAIL_PUBLIC TagGenerator
{
public:
  // NOLINTNEXTLINE
  inline static std::array<char, 26> ALPHABET{
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
    'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
  }; /**< Alphabet. */

  constexpr static uint16_t MAX_TAG_INDEX = 999; /**< Max index of tag. */
  constexpr static int TAG_BASE = 10;            /**< Tag base. */

private:
  inline static std::mt19937 RND{
    std::random_device{}()
  }; /**< Random number generator for letter selection. */

  inline static std::uniform_int_distribution<std::size_t> RND_SEL{
    0,
    ALPHABET.size() - 1
  }; /**< Letter selector. */

  char _tag;
  uint16_t _idx{ 0 };

public:
  /**
   * @brief Construct a new Tag Generator with specific prefix.
   *
   * @param tag Prefix letter.
   */
  explicit TagGenerator(char tag)
    : _tag{ tag }
  {
  }

  /**
   * @brief Construct a new Tag Generator with random prefix.
   *
   */
  TagGenerator()
    : TagGenerator{ ALPHABET.at(RND_SEL(RND)) }
  {
  }

  /**
   * @brief Generate next tag.
   *
   * @return QString Next tag.
   */
  QString generate();

  /**
   * @brief Get label of current tag (for logging or debug).
   *
   * @return QString Tag Label.
   */
  [[nodiscard]] QString label() const;
};

}
