/*
 * Strawberry Music Player
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef TIDALSETTINGSPAGE_H
#define TIDALSETTINGSPAGE_H

#include "config.h"

#include <QObject>
#include <QString>

#include "settings/settingspage.h"

class QEvent;
class TidalService;
class SettingsDialog;
class Ui_TidalSettingsPage;

class TidalSettingsPage : public SettingsPage {
  Q_OBJECT

 public:
  explicit TidalSettingsPage(SettingsDialog *dialog, QWidget *parent = nullptr);
  ~TidalSettingsPage() override;

  static const char *kSettingsGroup;

  enum class StreamUrlMethod {
    StreamUrl,
    UrlPostPaywall,
    PlaybackInfoPostPaywall
  };

  void Load() override;
  void Save() override;

  bool eventFilter(QObject *object, QEvent *event) override;

 signals:
  void Authorize(QString client_id);
  void Login(QString api_token, QString username, QString password);

 private slots:
  void OAuthClicked(const bool enabled);
  void LoginClicked();
  void LogoutClicked();
  void LoginSuccess();
  void LoginFailure(const QString &failure_reason);

 private:
  Ui_TidalSettingsPage *ui_;
  TidalService *service_;
};

#endif  // TIDALSETTINGSPAGE_H
