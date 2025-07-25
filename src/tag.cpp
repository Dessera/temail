#include <qstring.h>
#include <random>

#include "temail/tag.hpp"

namespace temail {

QString
TagGenerator::generate()
{
  auto index = _idx;
  ++_idx;

  if (_idx > MAX_TAG_INDEX) {
    _idx = 0;
  }

  return QString{ "%1%2" }.arg(_tag).arg(index, 3, TAG_BASE, '0');
}

QString
TagGenerator::label() const
{
  return QString{ "%1XXX" }.arg(_tag);
}

}
