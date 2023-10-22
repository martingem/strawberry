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

#ifndef MUSICBRAINZCOVERPROVIDER_H
#define MUSICBRAINZCOVERPROVIDER_H

#include "config.h"

#include <QObject>
#include <QList>
#include <QQueue>
#include <QByteArray>
#include <QVariant>
#include <QString>
#include <QJsonObject>

#include "core/shared_ptr.h"
#include "jsoncoverprovider.h"

class QNetworkReply;
class QTimer;
class Application;
class NetworkAccessManager;

class MusicbrainzCoverProvider : public JsonCoverProvider {
  Q_OBJECT

 public:
  explicit MusicbrainzCoverProvider(Application *app, SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);
  ~MusicbrainzCoverProvider() override;

  bool StartSearch(const QString &artist, const QString &album, const QString &title, const int id) override;

 private slots:
  void FlushRequests();
  void HandleSearchReply(QNetworkReply *reply, const int search_id);

 private:
  struct SearchRequest {
    explicit SearchRequest(const int _id, const QString &_artist, const QString &_album) : id(_id), artist(_artist), album(_album) {}
    int id;
    QString artist;
    QString album;
  };

  void SendSearchRequest(const SearchRequest &request);
  QByteArray GetReplyData(QNetworkReply *reply);
  void Error(const QString &error, const QVariant &debug = QVariant()) override;

 private:
  static const char *kReleaseSearchUrl;
  static const char *kAlbumCoverUrl;
  static const int kLimit;
  static const int kRequestsDelay;

  QTimer *timer_flush_requests_;
  QQueue<SearchRequest> queue_search_requests_;
  QList<QNetworkReply*> replies_;
};

#endif  // MUSICBRAINZCOVERPROVIDER_H
