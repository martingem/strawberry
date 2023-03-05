/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2019-2021, Jonas Kvinge <jonas@jkvinge.net>
 *
 * Strawberry is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Strawberry is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Strawberry.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include <QVariant>
#include <QSettings>
#include <QNetworkProxy>
#include <QComboBox>
#include <QGroupBox>
#include <QLineEdit>
#include <QRadioButton>
#include <QSpinBox>

#include "core/iconloader.h"
#include "core/networkproxyfactory.h"
#include "networkproxysettingspage.h"
#include "settings/settingspage.h"
#include "ui_networkproxysettingspage.h"

class SettingsDialog;

const char *NetworkProxySettingsPage::kSettingsGroup = "NetworkProxy";

NetworkProxySettingsPage::NetworkProxySettingsPage(SettingsDialog *dialog, QWidget *parent)
    : SettingsPage(dialog, parent),
      ui_(new Ui_NetworkProxySettingsPage) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load("applications-internet", true, 0, 32));

}

NetworkProxySettingsPage::~NetworkProxySettingsPage() { delete ui_; }

void NetworkProxySettingsPage::Load() {

  QSettings s;

  s.beginGroup(NetworkProxyFactory::kSettingsGroup);
  const NetworkProxyFactory::Mode mode = static_cast<NetworkProxyFactory::Mode>(s.value("mode", static_cast<int>(NetworkProxyFactory::Mode::System)).toInt());
  switch (mode) {
    case NetworkProxyFactory::Mode::Manual:
      ui_->proxy_manual->setChecked(true);
      break;

    case NetworkProxyFactory::Mode::Direct:
      ui_->proxy_direct->setChecked(true);
      break;

    case NetworkProxyFactory::Mode::System:
      ui_->proxy_system->setChecked(true);
      break;
  }

  ui_->proxy_type->setCurrentIndex(s.value("type", QNetworkProxy::HttpProxy).toInt() == QNetworkProxy::HttpProxy ? 0 : 1);
  ui_->proxy_hostname->setText(s.value("hostname").toString());
  ui_->proxy_port->setValue(s.value("port").toInt());
  ui_->proxy_auth->setChecked(s.value("use_authentication", false).toBool());
  ui_->proxy_username->setText(s.value("username").toString());
  ui_->proxy_password->setText(s.value("password").toString());
  ui_->proxy_engine->setChecked(s.value("engine", true).toBool());
  s.endGroup();

  Init(ui_->layout_networkproxysettingspage->parentWidget());

  if (!QSettings().childGroups().contains(kSettingsGroup)) set_changed();

}

void NetworkProxySettingsPage::Save() {

  QSettings s;

  NetworkProxyFactory::Mode mode = NetworkProxyFactory::Mode::System;
  if (ui_->proxy_direct->isChecked()) mode = NetworkProxyFactory::Mode::Direct;
  else if (ui_->proxy_system->isChecked()) mode = NetworkProxyFactory::Mode::System;
  else if (ui_->proxy_manual->isChecked()) mode = NetworkProxyFactory::Mode::Manual;

  s.beginGroup(NetworkProxyFactory::kSettingsGroup);
  s.setValue("mode", static_cast<int>(mode));
  s.setValue("type", ui_->proxy_type->currentIndex() == 0 ? QNetworkProxy::HttpProxy : QNetworkProxy::Socks5Proxy);
  s.setValue("hostname", ui_->proxy_hostname->text());
  s.setValue("port", ui_->proxy_port->value());
  s.setValue("use_authentication", ui_->proxy_auth->isChecked());
  s.setValue("username", ui_->proxy_username->text());
  s.setValue("password", ui_->proxy_password->text());
  s.setValue("engine", ui_->proxy_engine->isChecked());
  s.endGroup();

  NetworkProxyFactory::Instance()->ReloadSettings();

}
