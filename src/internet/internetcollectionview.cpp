/*
 * Strawberry Music Player
 * This code was part of Clementine
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

#include "config.h"

#include <QWidget>
#include <QTreeView>
#include <QSortFilterProxyModel>
#include <QAbstractItemView>
#include <QItemSelectionModel>
#include <QVariant>
#include <QString>
#include <QPainter>
#include <QRect>
#include <QFont>
#include <QFontMetrics>
#include <QMimeData>
#include <QMenu>
#include <QtEvents>

#include "core/shared_ptr.h"
#include "core/application.h"
#include "core/iconloader.h"
#include "core/mimedata.h"
#include "collection/collectionbackend.h"
#include "collection/collectionmodel.h"
#include "collection/collectionfilterwidget.h"
#include "collection/collectionitem.h"
#include "collection/collectionitemdelegate.h"
#include "internetcollectionview.h"

InternetCollectionView::InternetCollectionView(QWidget *parent)
    : AutoExpandingTreeView(parent),
      app_(nullptr),
      collection_backend_(nullptr),
      collection_model_(nullptr),
      filter_(nullptr),
      favorite_(false),
      total_song_count_(0),
      total_artist_count_(0),
      total_album_count_(0),
      nomusic_(":/pictures/nomusic.png"),
      context_menu_(nullptr),
      load_(nullptr),
      add_to_playlist_(nullptr),
      add_to_playlist_enqueue_(nullptr),
      add_to_playlist_enqueue_next_(nullptr),
      open_in_new_playlist_(nullptr),
      remove_songs_(nullptr),
      is_in_keyboard_search_(false) {

  setItemDelegate(new CollectionItemDelegate(this));
  setAttribute(Qt::WA_MacShowFocusRect, false);
  setHeaderHidden(true);
  setAllColumnsShowFocus(true);
  setDragEnabled(true);
  setDragDropMode(QAbstractItemView::DragOnly);
  setSelectionMode(QAbstractItemView::ExtendedSelection);
  SetAutoOpen(false);

  setStyleSheet("QTreeView::item{padding-top:1px;}");

}

void InternetCollectionView::Init(Application *app, SharedPtr<CollectionBackend> collection_backend, CollectionModel *collection_model, const bool favorite) {

  app_ = app;
  collection_backend_ = collection_backend;
  collection_model_ = collection_model;
  favorite_ = favorite;

  collection_model_->set_pretty_covers(true);
  collection_model_->set_show_dividers(true);

  ReloadSettings();

}

void InternetCollectionView::SetFilter(CollectionFilterWidget *filter) {

  filter_ = filter;

}

void InternetCollectionView::ReloadSettings() {

  if (filter_) filter_->ReloadSettings();

}

void InternetCollectionView::SaveFocus() {

  QModelIndex current = currentIndex();
  QVariant type = model()->data(current, CollectionModel::Role_Type);
  if (!type.isValid() || (type.toInt() != CollectionItem::Type_Song && type.toInt() != CollectionItem::Type_Container && type.toInt() != CollectionItem::Type_Divider)) {
    return;
  }

  last_selected_path_.clear();
  last_selected_song_ = Song();
  last_selected_container_ = QString();

  switch (type.toInt()) {
    case CollectionItem::Type_Song: {
      QModelIndex idx = qobject_cast<QSortFilterProxyModel*>(model())->mapToSource(current);
      SongList songs = collection_model_->GetChildSongs(idx);
      if (!songs.isEmpty()) {
        last_selected_song_ = songs.last();
      }
      break;
    }

    case CollectionItem::Type_Container:
    case CollectionItem::Type_Divider: {
      QString text = model()->data(current, CollectionModel::Role_SortText).toString();
      last_selected_container_ = text;
      break;
    }

    default:
      return;
  }

  SaveContainerPath(current);

}

void InternetCollectionView::SaveContainerPath(const QModelIndex &child) {

  QModelIndex current = model()->parent(child);
  QVariant type = model()->data(current, CollectionModel::Role_Type);
  if (!type.isValid() || (type.toInt() != CollectionItem::Type_Container && type.toInt() != CollectionItem::Type_Divider)) {
    return;
  }

  QString text = model()->data(current, CollectionModel::Role_SortText).toString();
  last_selected_path_ << text;
  SaveContainerPath(current);

}

void InternetCollectionView::RestoreFocus() {

  if (last_selected_container_.isEmpty() && last_selected_song_.url().isEmpty()) {
    return;
  }
  RestoreLevelFocus();

}

bool InternetCollectionView::RestoreLevelFocus(const QModelIndex &parent) {

  if (model()->canFetchMore(parent)) {
    model()->fetchMore(parent);
  }
  int rows = model()->rowCount(parent);
  for (int i = 0; i < rows; i++) {
    QModelIndex current = model()->index(i, 0, parent);
    QVariant type = model()->data(current, CollectionModel::Role_Type);
    switch (type.toInt()) {
      case CollectionItem::Type_Song:
        if (!last_selected_song_.url().isEmpty()) {
          QModelIndex idx = qobject_cast<QSortFilterProxyModel*>(model())->mapToSource(current);
          SongList songs = collection_model_->GetChildSongs(idx);
          for (const Song &song : songs) {
            if (song == last_selected_song_) {
              setCurrentIndex(current);
              return true;
            }
          }
        }
        break;

      case CollectionItem::Type_Container:
      case CollectionItem::Type_Divider: {
        QString text = model()->data(current, CollectionModel::Role_SortText).toString();
        if (!last_selected_container_.isEmpty() && last_selected_container_ == text) {
          expand(current);
          setCurrentIndex(current);
          return true;
        }
        else if (last_selected_path_.contains(text)) {
          expand(current);
          // If a selected container or song were not found, we've got into a wrong subtree (happens with "unknown" all the time)
          if (!RestoreLevelFocus(current)) {
            collapse(current);
          }
          else {
            return true;
          }
        }
        break;
      }
    }
  }
  return false;

}

void InternetCollectionView::TotalSongCountUpdated(int count) {

  int old = total_song_count_;
  total_song_count_ = count;
  if (old != total_song_count_) update();

  if (total_song_count_ == 0) {
    setCursor(Qt::PointingHandCursor);
  }
  else {
    unsetCursor();
  }

  emit TotalSongCountUpdated_();

}

void InternetCollectionView::TotalArtistCountUpdated(int count) {

  int old = total_artist_count_;
  total_artist_count_ = count;
  if (old != total_artist_count_) update();

  if (total_artist_count_ == 0) {
    setCursor(Qt::PointingHandCursor);
  }
  else {
    unsetCursor();
  }

  emit TotalArtistCountUpdated_();

}

void InternetCollectionView::TotalAlbumCountUpdated(int count) {

  int old = total_album_count_;
  total_album_count_ = count;
  if (old != total_album_count_) update();

  if (total_album_count_ == 0) {
    setCursor(Qt::PointingHandCursor);
  }
  else {
    unsetCursor();
  }

  emit TotalAlbumCountUpdated_();

}

void InternetCollectionView::paintEvent(QPaintEvent *event) {

  if (total_song_count_ == 0) {
    QPainter p(viewport());
    QRect rect(viewport()->rect());

    // Draw the confused strawberry
    QRect image_rect((rect.width() - nomusic_.width()) / 2, 50, nomusic_.width(), nomusic_.height());
    p.drawPixmap(image_rect, nomusic_);

    // Draw the title text
    QFont bold_font;
    bold_font.setBold(true);
    p.setFont(bold_font);

    QFontMetrics metrics(bold_font);

    QRect title_rect(0, image_rect.bottom() + 20, rect.width(), metrics.height());
    p.drawText(title_rect, Qt::AlignHCenter, tr("The internet collection is empty!"));

    // Draw the other text
    p.setFont(QFont());

    QRect text_rect(0, title_rect.bottom() + 5, rect.width(), metrics.height());
    p.drawText(text_rect, Qt::AlignHCenter, tr("Click here to retrieve music"));
  }
  else {
    QTreeView::paintEvent(event);
  }

}

void InternetCollectionView::mouseReleaseEvent(QMouseEvent *e) {

  QTreeView::mouseReleaseEvent(e);

  if (total_song_count_ == 0) {
    emit GetSongs();
  }

}

void InternetCollectionView::contextMenuEvent(QContextMenuEvent *e) {

  if (!context_menu_) {
    context_menu_ = new QMenu(this);
    add_to_playlist_ = context_menu_->addAction(IconLoader::Load("media-playback-start"), tr("Append to current playlist"), this, &InternetCollectionView::AddToPlaylist);
    load_ = context_menu_->addAction(IconLoader::Load("media-playback-start"), tr("Replace current playlist"), this, &InternetCollectionView::Load);
    open_in_new_playlist_ = context_menu_->addAction(IconLoader::Load("document-new"), tr("Open in new playlist"), this, &InternetCollectionView::OpenInNewPlaylist);

    context_menu_->addSeparator();
    add_to_playlist_enqueue_ = context_menu_->addAction(IconLoader::Load("go-next"), tr("Queue track"), this, &InternetCollectionView::AddToPlaylistEnqueue);
    add_to_playlist_enqueue_next_ = context_menu_->addAction(IconLoader::Load("go-next"), tr("Queue to play next"), this, &InternetCollectionView::AddToPlaylistEnqueueNext);

    context_menu_->addSeparator();

    if (favorite_) {
      remove_songs_ = context_menu_->addAction(IconLoader::Load("edit-delete"), tr("Remove from favorites"), this, &InternetCollectionView::RemoveSelectedSongs);
      context_menu_->addSeparator();
    }

    if (filter_) context_menu_->addMenu(filter_->menu());

  }

  context_menu_index_ = indexAt(e->pos());
  if (!context_menu_index_.isValid()) return;

  context_menu_index_ = qobject_cast<QSortFilterProxyModel*>(model())->mapToSource(context_menu_index_);
  QModelIndexList selected_indexes = qobject_cast<QSortFilterProxyModel*>(model())->mapSelectionToSource(selectionModel()->selection()).indexes();
  qint64 songs_selected = selected_indexes.count();

  // In all modes
  load_->setEnabled(songs_selected > 0);
  add_to_playlist_->setEnabled(songs_selected > 0);
  open_in_new_playlist_->setEnabled(songs_selected > 0);
  add_to_playlist_enqueue_->setEnabled(songs_selected > 0);
  if (remove_songs_) remove_songs_->setEnabled(songs_selected > 0);

  context_menu_->popup(e->globalPos());

}

void InternetCollectionView::Load() {

  QMimeData *q_mimedata = model()->mimeData(selectedIndexes());
  if (MimeData *mimedata = qobject_cast<MimeData*>(q_mimedata)) {
    mimedata->clear_first_ = true;
  }
  emit AddToPlaylistSignal(q_mimedata);

}

void InternetCollectionView::AddToPlaylist() {

  emit AddToPlaylistSignal(model()->mimeData(selectedIndexes()));

}

void InternetCollectionView::AddToPlaylistEnqueue() {

  QMimeData *q_mimedata = model()->mimeData(selectedIndexes());
  if (MimeData *mimedata = qobject_cast<MimeData*>(q_mimedata)) {
    mimedata->enqueue_now_ = true;
  }
  emit AddToPlaylistSignal(q_mimedata);

}

void InternetCollectionView::AddToPlaylistEnqueueNext() {

  QMimeData *q_mimedata = model()->mimeData(selectedIndexes());
  if (MimeData *mimedata = qobject_cast<MimeData*>(q_mimedata)) {
    mimedata->enqueue_next_now_ = true;
  }
  emit AddToPlaylistSignal(q_mimedata);

}

void InternetCollectionView::OpenInNewPlaylist() {

  QMimeData *q_mimedata = model()->mimeData(selectedIndexes());
  if (MimeData *mimedata = qobject_cast<MimeData*>(q_mimedata)) {
    mimedata->open_in_new_playlist_ = true;
  }
  emit AddToPlaylistSignal(q_mimedata);

}

void InternetCollectionView::RemoveSelectedSongs() {

  emit RemoveSongs(GetSelectedSongs());

}

void InternetCollectionView::keyboardSearch(const QString &search) {

  is_in_keyboard_search_ = true;
  QTreeView::keyboardSearch(search);
  is_in_keyboard_search_ = false;

}

void InternetCollectionView::scrollTo(const QModelIndex &idx, ScrollHint hint) {

  if (is_in_keyboard_search_) {
    QTreeView::scrollTo(idx, QAbstractItemView::PositionAtTop);
  }
  else {
    QTreeView::scrollTo(idx, hint);
  }

}

SongList InternetCollectionView::GetSelectedSongs() const {

  QModelIndexList selected_indexes = qobject_cast<QSortFilterProxyModel*>(model())->mapSelectionToSource(selectionModel()->selection()).indexes();
  return collection_model_->GetChildSongs(selected_indexes);

}

void InternetCollectionView::FilterReturnPressed() {

  if (!currentIndex().isValid()) {
    // Pick the first thing that isn't a divider
    for (int row = 0; row < model()->rowCount(); ++row) {
      QModelIndex idx(model()->index(row, 0));
      if (idx.data(CollectionModel::Role_Type) != CollectionItem::Type_Divider) {
        setCurrentIndex(idx);
        break;
      }
    }
  }

  if (!currentIndex().isValid()) return;

  emit doubleClicked(currentIndex());

}

int InternetCollectionView::TotalSongs() const {
  return total_song_count_;
}
int InternetCollectionView::TotalArtists() const {
  return total_artist_count_;
}
int InternetCollectionView::TotalAlbums() const {
  return total_album_count_;
}
