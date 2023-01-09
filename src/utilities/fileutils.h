/*
 * Strawberry Music Player
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

#ifndef FILEUTILS_H
#define FILEUTILS_H

#include <QString>

class QIODevice;

namespace Utilities {

QByteArray ReadDataFromFile(const QString &filename);
bool Copy(QIODevice *source, QIODevice *destination);
bool CopyRecursive(const QString &source, const QString &destination);
bool RemoveRecursive(const QString &path);

}  // namespace Utilities

#endif  // FILEUTILS_H
