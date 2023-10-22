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

#ifndef MTPDEVICE_H
#define MTPDEVICE_H

#include "config.h"

#include <QObject>
#include <QMutex>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

#include "core/scoped_ptr.h"
#include "core/shared_ptr.h"
#include "core/song.h"
#include "connecteddevice.h"

class QThread;
class Application;
class DeviceLister;
class DeviceManager;
class MtpLoader;
class MtpConnection;
struct LIBMTP_mtpdevice_struct;

class MtpDevice : public ConnectedDevice {
  Q_OBJECT

 public:
  Q_INVOKABLE MtpDevice(const QUrl &url, DeviceLister *lister, const QString &unique_id, SharedPtr<DeviceManager> manager, Application *app, const int database_id, const bool first_time, QObject *parent = nullptr);
  ~MtpDevice() override;

  static QStringList url_schemes() { return QStringList() << "mtp"; }

  bool Init() override;
  void ConnectAsync() override;
  void Close() override;
  bool IsLoading() override { return loader_; }

  bool GetSupportedFiletypes(QList<Song::FileType> *ret) override;
  int GetFreeSpace();
  int GetCapacity();

  bool StartCopy(QList<Song::FileType> *supported_types) override;
  bool CopyToStorage(const CopyJob &job) override;
  void FinishCopy(const bool success) override;

  void StartDelete() override;
  bool DeleteFromStorage(const DeleteJob &job) override;
  void FinishDelete(const bool success) override;

 private slots:
  void LoadFinished(bool success, MtpConnection *connection);
  void LoaderError(const QString &message);

 private:
  bool GetSupportedFiletypes(QList<Song::FileType> *ret, LIBMTP_mtpdevice_struct *device);
  int GetFreeSpace(LIBMTP_mtpdevice_struct *device);
  int GetCapacity(LIBMTP_mtpdevice_struct *device);

 private:
  static bool sInitializedLibMTP;

  MtpLoader *loader_;
  QThread *loader_thread_;
  bool closing_;

  QMutex db_busy_;
  SongList songs_to_add_;
  SongList songs_to_remove_;

  ScopedPtr<MtpConnection> connection_;
};

#endif  // MTPDEVICE_H
