#include "temail/client/base.hpp"

namespace temail::client {

bool
Base::wait_for_connected(int msecs)
{
  if (is_connected()) {
    return true;
  }

  _wait_for_event(msecs, &Base::connected);

  return is_connected();
}

bool
Base::wait_for_disconnected(int msecs)
{
  if (is_disconnected()) {
    return true;
  }

  _wait_for_event(msecs, &Base::disconnected);

  return is_disconnected();
}

bool
Base::wait_for_ready_read(int msecs)
{
  _wait_for_event(msecs, &Base::ready_read);

  return _error == E_NOERR;
}

}
