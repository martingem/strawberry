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

#ifndef PLAYLISTUNDOCOMMANDS_H
#define PLAYLISTUNDOCOMMANDS_H

#include "config.h"

#include <QCoreApplication>
#include <QList>
#include <QUndoStack>

#include "playlistitem.h"

class Playlist;

namespace PlaylistUndoCommands {

  enum Types {
    Type_RemoveItems = 0,
  };

  class Base : public QUndoCommand {
    Q_DECLARE_TR_FUNCTIONS(PlaylistUndoCommands)

   public:
    explicit Base(Playlist *playlist);

   protected:
    Playlist *playlist_;
  };

  class InsertItems : public Base {
   public:
    explicit InsertItems(Playlist *playlist, const PlaylistItemPtrList &items, int pos, bool enqueue = false, bool enqueue_next = false);

    void undo() override;
    void redo() override;
    // When load is async, items have already been pushed, so we need to update them.
    // This function try to find the equivalent item, and replace it with the new (completely loaded) one.
    // Return true if the was found (and updated), false otherwise
    bool UpdateItem(const PlaylistItemPtr &updated_item);

   private:
    PlaylistItemPtrList items_;
    int pos_;
    bool enqueue_;
    bool enqueue_next_;
  };

  class RemoveItems : public Base {
   public:
    explicit RemoveItems(Playlist *playlist, int pos, int count);

    int id() const override { return Type_RemoveItems; }

    void undo() override;
    void redo() override;
    bool mergeWith(const QUndoCommand *other) override;

   private:
    struct Range {
      Range(int pos, int count) : pos_(pos), count_(count) {}
      int pos_;
      int count_;
      PlaylistItemPtrList items_;
    };

    QList<Range> ranges_;
  };

  class MoveItems : public Base {
   public:
    explicit MoveItems(Playlist *playlist, const QList<int> &source_rows, int pos);

    void undo() override;
    void redo() override;

   private:
    QList<int> source_rows_;
    int pos_;
  };

  class ReOrderItems : public Base {
   public:
    explicit ReOrderItems(Playlist *playlist, const PlaylistItemPtrList &new_items);

    void undo() override;
    void redo() override;

   private:
    PlaylistItemPtrList old_items_;
    PlaylistItemPtrList new_items_;
  };

  class SortItems : public ReOrderItems {
   public:
    explicit SortItems(Playlist *playlist, int column, Qt::SortOrder order, const PlaylistItemPtrList &new_items);

  };

  class ShuffleItems : public ReOrderItems {
   public:
    explicit ShuffleItems(Playlist *playlist, const PlaylistItemPtrList &new_items);
  };

}  // namespace

#endif // PLAYLISTUNDOCOMMANDS_H
