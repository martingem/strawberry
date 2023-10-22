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

#include <QString>

#include "core/shared_ptr.h"
#include "core/taskmanager.h"
#include "collectiontask.h"

CollectionTask::CollectionTask(SharedPtr<TaskManager> task_manager, const QString &message) : task_manager_(task_manager), task_id_(-1) {

  if (task_manager_) task_id_ = task_manager_->StartTask(message);

}

CollectionTask::~CollectionTask() {

  if (task_manager_) task_manager_->SetTaskFinished(task_id_);

}
