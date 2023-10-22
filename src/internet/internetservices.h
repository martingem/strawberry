/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#ifndef INTERNETSERVICES_H
#define INTERNETSERVICES_H

#include "config.h"

#include <memory>

#include <QObject>
#include <QList>
#include <QMap>
#include <QString>

#include <core/shared_ptr.h>
#include "core/song.h"

class InternetService;

class InternetServices : public QObject {
  Q_OBJECT

 public:
  explicit InternetServices(QObject *parent = nullptr);
  ~InternetServices() override;

  SharedPtr<InternetService> ServiceBySource(const Song::Source source) const;

  template <typename T>
  SharedPtr<T> Service() {
    return std::static_pointer_cast<T>(ServiceBySource(T::kSource));
  }

  void AddService(SharedPtr<InternetService> service);
  void RemoveService(SharedPtr<InternetService> service);
  void ReloadSettings();
  void Exit();

 signals:
  void ExitFinished();

 private slots:
  void ExitReceived();

 private:
  QMap<Song::Source, SharedPtr<InternetService>> services_;
  QList<InternetService*> wait_for_exit_;
};

#endif  // INTERNETSERVICES_H
