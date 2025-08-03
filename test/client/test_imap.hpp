#pragma once

#include <qobject.h>
#include <qtest.h>
#include <temail/client/base.hpp>
#include <temail/client/imap.hpp>
#include <temail/common.hpp>

using namespace temail;

class IMAPTest : public QObject
{
  Q_OBJECT

private:
  client::Base* _client{ new client::IMAP{ this } };

private slots: // NOLINT
  void test_interface();
};
