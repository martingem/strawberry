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

#ifndef PLAYLISTSETTINGSPAGE_H
#define PLAYLISTSETTINGSPAGE_H

#include "config.h"

#include <QObject>
#include <QString>

#include "settingspage.h"

class SettingsDialog;
class Ui_PlaylistSettingsPage;

class PlaylistSettingsPage : public SettingsPage {
  Q_OBJECT

 public:
  explicit PlaylistSettingsPage(SettingsDialog *dialog, QWidget *parent = nullptr);
  ~PlaylistSettingsPage() override;

  static const char *kSettingsGroup;

  enum class PathType {
    Automatic = 0,  // Automatically select path type
    Absolute,       // Always use absolute paths
    Relative,       // Always use relative paths
    Ask_User        // Only used in preferences: to ask user which of the previous values he wants to use.
  };

  void Load() override;
  void Save() override;

 private:
  Ui_PlaylistSettingsPage *ui_;

};

Q_DECLARE_METATYPE(PlaylistSettingsPage::PathType)

#endif  // PLAYLISTSETTINGSPAGE_H
