/*
 * Strawberry Music Player
 * Copyright 2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QObject>
#include <QVariant>
#include <QByteArray>
#include <QString>
#include <QIcon>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>

#include "core/logging.h"
#include "core/shared_ptr.h"
#include "core/application.h"
#include "radioservice.h"

RadioService::RadioService(const Song::Source source, const QString &name, const QIcon &icon, Application *app, SharedPtr<NetworkAccessManager> network, QObject *parent)
    : QObject(parent),
      app_(app),
      network_(network),
      source_(source),
      name_(name),
      icon_(icon) {}

QByteArray RadioService::ExtractData(QNetworkReply *reply) {

  if (reply->error() != QNetworkReply::NoError) {
    Error(QString("Failed to retrieve data from %1: %2 (%3)").arg(name_, reply->errorString()).arg(reply->error()));
    if (reply->error() < 200) {
      return QByteArray();
    }
  }
  else if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
    Error(QString("Received HTTP code %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()));
  }

  return reply->readAll();

}

QJsonObject RadioService::ExtractJsonObj(const QByteArray &data) {

  if (data.isEmpty()) {
    return QJsonObject();
  }

  QJsonParseError json_error;
  QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_error);

  if (json_error.error != QJsonParseError::NoError) {
    Error(QString("Failed to parse Json data from %1: %2").arg(name_, json_error.errorString()));
    return QJsonObject();
  }

  if (json_doc.isEmpty()) {
    Error(QString("%1: Received empty Json document.").arg(name_), data);
    return QJsonObject();
  }

  if (!json_doc.isObject()) {
    Error(QString("%1: Json document is not an object.").arg(name_), json_doc);
    return QJsonObject();
  }

  QJsonObject json_obj = json_doc.object();
  if (json_obj.isEmpty()) {
    Error(QString("%1: Received empty Json object.").arg(name_), json_doc);
    return QJsonObject();
  }

  return json_obj;

}

QJsonObject RadioService::ExtractJsonObj(QNetworkReply *reply) {

  return ExtractJsonObj(ExtractData(reply));

}

void RadioService::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << name_ << error;
  if (debug.isValid()) qLog(Debug) << debug;

}
