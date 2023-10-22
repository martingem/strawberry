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

#include "config.h"

#include <QObject>
#include <QWizard>

#include "core/shared_ptr.h"
#include "smartplaylistwizardplugin.h"

SmartPlaylistWizardPlugin::SmartPlaylistWizardPlugin(Application *app, SharedPtr<CollectionBackend> collection_backend, QObject *parent)
    : QObject(parent),
      app_(app),
      collection_backend_(collection_backend),
      start_page_(-1) {}

void SmartPlaylistWizardPlugin::Init(QWizard *wizard, const int finish_page_id) {
  start_page_ = CreatePages(wizard, finish_page_id);
}
