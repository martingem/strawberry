/*
 * Strawberry Music Player
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

#ifndef JSONLYRICSPROVIDER_H
#define JSONLYRICSPROVIDER_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QList>
#include <QVariant>
#include <QString>
#include <QJsonObject>

#include "core/shared_ptr.h"
#include "lyricsprovider.h"

class NetworkAccessManager;
class QNetworkReply;

class JsonLyricsProvider : public LyricsProvider {
  Q_OBJECT

 public:
  explicit JsonLyricsProvider(const QString &name, const bool enabled, const bool authentication_required, SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);

 protected:
  QByteArray ExtractData(QNetworkReply *reply);
  QJsonObject ExtractJsonObj(const QByteArray &data);
  QJsonObject ExtractJsonObj(QNetworkReply *reply);
};

#endif  // JSONLYRICSPROVIDER_H
