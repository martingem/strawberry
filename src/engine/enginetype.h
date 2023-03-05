/*
 * Strawberry Music Player
 * Copyright 2013-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef ENGINETYPE_H
#define ENGINETYPE_H

#include "config.h"

#include <QMetaType>
#include <QString>

namespace Engine {

enum class EngineType {
  None,
  GStreamer,
  VLC,
  Xine
};

Engine::EngineType EngineTypeFromName(const QString &enginename);
QString EngineName(const Engine::EngineType enginetype);
QString EngineDescription(const Engine::EngineType enginetype);

}  // namespace Engine

Q_DECLARE_METATYPE(Engine::EngineType)

#endif  // ENGINETYPE_H
