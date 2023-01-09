/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#ifndef COLLECTIONDIRECTORY_H
#define COLLECTIONDIRECTORY_H

#include "config.h"

#include <QMetaType>
#include <QList>
#include <QString>

struct CollectionDirectory {
  CollectionDirectory() : id(-1) {}

  bool operator==(const CollectionDirectory &other) const {
    return path == other.path && id == other.id;
  }

  QString path;
  int id;
};
Q_DECLARE_METATYPE(CollectionDirectory)

using CollectionDirectoryList = QList<CollectionDirectory>;
Q_DECLARE_METATYPE(CollectionDirectoryList)

struct CollectionSubdirectory {
  CollectionSubdirectory() : directory_id(-1), mtime(0) {}

  int directory_id;
  QString path;
  qint64 mtime;
};
Q_DECLARE_METATYPE(CollectionSubdirectory)

using CollectionSubdirectoryList = QList<CollectionSubdirectory>;
Q_DECLARE_METATYPE(CollectionSubdirectoryList)

#endif  // COLLECTIONDIRECTORY_H
