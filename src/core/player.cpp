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

#include "config.h"

#include "player.h"

#include <algorithm>
#include <memory>

#include <QtGlobal>
#include <QObject>
#include <QMap>
#include <QVariant>
#include <QString>
#include <QUrl>
#include <QDateTime>
#include <QSettings>

#include "core/logging.h"
#include "utilities/timeconstants.h"

#include "scoped_ptr.h"
#include "shared_ptr.h"
#include "song.h"
#include "urlhandler.h"
#include "application.h"

#include "engine/enginebase.h"
#include "engine/enginemetadata.h"

#ifdef HAVE_GSTREAMER
#  include "engine/gstengine.h"
#  include "engine/gststartup.h"
#endif
#ifdef HAVE_VLC
#  include "engine/vlcengine.h"
#endif

#include "collection/collectionbackend.h"
#include "playlist/playlist.h"
#include "playlist/playlistfilter.h"
#include "playlist/playlistitem.h"
#include "playlist/playlistmanager.h"
#include "playlist/playlistsequence.h"
#include "equalizer/equalizer.h"
#include "analyzer/analyzercontainer.h"
#include "settings/backendsettingspage.h"
#include "settings/behavioursettingspage.h"
#include "settings/playlistsettingspage.h"

using std::make_shared;

const char *Player::kSettingsGroup = "Player";

Player::Player(Application *app, QObject *parent)
    : PlayerInterface(parent),
      app_(app),
      engine_(nullptr),
#ifdef HAVE_GSTREAMER
      gst_startup_(new GstStartup(this)),
#endif
      analyzer_(nullptr),
      equalizer_(nullptr),
      stream_change_type_(EngineBase::TrackChangeType::First),
      autoscroll_(Playlist::AutoScroll::Maybe),
      last_state_(EngineBase::State::Empty),
      nb_errors_received_(0),
      volume_(100),
      volume_before_mute_(100),
      last_pressed_previous_(QDateTime::currentDateTime()),
      continue_on_error_(false),
      greyout_(true),
      menu_previousmode_(BehaviourSettingsPage::PreviousBehaviour::DontRestart),
      seek_step_sec_(10),
      play_offset_nanosec_(0) {

  QSettings s;
  s.beginGroup(BackendSettingsPage::kSettingsGroup);
  EngineBase::Type enginetype = EngineBase::TypeFromName(s.value("engine", EngineBase::Name(EngineBase::Type::GStreamer)).toString().toLower());
  s.endGroup();

  CreateEngine(enginetype);

}

EngineBase::Type Player::CreateEngine(EngineBase::Type enginetype) {

  EngineBase::Type use_enginetype = EngineBase::Type::None;

  for (int i = 0; use_enginetype == EngineBase::Type::None; i++) {
    switch (enginetype) {
      case EngineBase::Type::None:
#ifdef HAVE_GSTREAMER
      case EngineBase::Type::GStreamer:{
        use_enginetype=EngineBase::Type::GStreamer;
        ScopedPtr<GstEngine> gst_engine(new GstEngine(app_->task_manager()));
        gst_engine->SetStartup(gst_startup_);
        engine_.reset(gst_engine.release());
        break;
      }
#endif
#ifdef HAVE_VLC
      case EngineBase::Type::VLC:
        use_enginetype = EngineBase::Type::VLC;
        engine_ = make_shared<VLCEngine>(app_->task_manager());
        break;
#endif
      default:
        if (i > 0) {
          qFatal("No engine available!");
        }
        enginetype = EngineBase::Type::None;
        break;
    }
  }

  if (use_enginetype != enginetype) {  // Engine was set to something else. Reset output and device.
    QSettings s;
    s.beginGroup(BackendSettingsPage::kSettingsGroup);
    s.setValue("engine", EngineBase::Name(use_enginetype));
    s.setValue("output", engine_->DefaultOutput());
    s.setValue("device", QVariant());
    s.endGroup();
  }

  if (!engine_) {
    qFatal("Failed to create engine!");
  }

  emit EngineChanged(use_enginetype);

  return use_enginetype;

}

void Player::Init() {

  QSettings s;

  if (!engine_) {
    s.beginGroup(BackendSettingsPage::kSettingsGroup);
    EngineBase::Type enginetype = EngineBase::TypeFromName(s.value("engine", EngineBase::Name(EngineBase::Type::GStreamer)).toString().toLower());
    s.endGroup();
    CreateEngine(enginetype);
  }

  if (!engine_->Init()) {
    qFatal("Error initializing audio engine");
  }

  analyzer_->SetEngine(engine_);

  QObject::connect(&*engine_, &EngineBase::Error, this, &Player::Error);
  QObject::connect(&*engine_, &EngineBase::FatalError, this, &Player::FatalError);
  QObject::connect(&*engine_, &EngineBase::ValidSongRequested, this, &Player::ValidSongRequested);
  QObject::connect(&*engine_, &EngineBase::InvalidSongRequested, this, &Player::InvalidSongRequested);
  QObject::connect(&*engine_, &EngineBase::StateChanged, this, &Player::EngineStateChanged);
  QObject::connect(&*engine_, &EngineBase::TrackAboutToEnd, this, &Player::TrackAboutToEnd);
  QObject::connect(&*engine_, &EngineBase::TrackEnded, this, &Player::TrackEnded);
  QObject::connect(&*engine_, &EngineBase::MetaData, this, &Player::EngineMetadataReceived);
  QObject::connect(&*engine_, &EngineBase::VolumeChanged, this, &Player::SetVolumeFromEngine);

  // Equalizer
  QObject::connect(&*equalizer_, &Equalizer::StereoBalancerEnabledChanged, &*app_->player()->engine(), &EngineBase::SetStereoBalancerEnabled);
  QObject::connect(&*equalizer_, &Equalizer::StereoBalanceChanged, &*app_->player()->engine(), &EngineBase::SetStereoBalance);
  QObject::connect(&*equalizer_, &Equalizer::EqualizerEnabledChanged, &*app_->player()->engine(), &EngineBase::SetEqualizerEnabled);
  QObject::connect(&*equalizer_, &Equalizer::EqualizerParametersChanged, &*app_->player()->engine(), &EngineBase::SetEqualizerParameters);

  engine_->SetStereoBalancerEnabled(equalizer_->is_stereo_balancer_enabled());
  engine_->SetStereoBalance(equalizer_->stereo_balance());
  engine_->SetEqualizerEnabled(equalizer_->is_equalizer_enabled());
  engine_->SetEqualizerParameters(equalizer_->preamp_value(), equalizer_->gain_values());

  ReloadSettings();

  LoadVolume();

}

void Player::ReloadSettings() {

  QSettings s;

  s.beginGroup(PlaylistSettingsPage::kSettingsGroup);
  continue_on_error_ = s.value("continue_on_error", false).toBool();
  greyout_ = s.value("greyout_songs_play", true).toBool();
  s.endGroup();

  s.beginGroup(BehaviourSettingsPage::kSettingsGroup);
  menu_previousmode_ = static_cast<BehaviourSettingsPage::PreviousBehaviour>(s.value("menu_previousmode", static_cast<int>(BehaviourSettingsPage::PreviousBehaviour::DontRestart)).toInt());
  seek_step_sec_ = s.value("seek_step_sec", 10).toInt();
  s.endGroup();

  engine_->ReloadSettings();

}

void Player::LoadVolume() {

  QSettings s;
  s.beginGroup(kSettingsGroup);
  const uint volume = s.value("volume", 100).toInt();
  s.endGroup();

  SetVolume(volume);

}

void Player::SaveVolume() {

  QSettings s;
  s.beginGroup(kSettingsGroup);
  s.setValue("volume", volume_);
  s.endGroup();

}

void Player::HandleLoadResult(const UrlHandler::LoadResult &result) {

  if (loading_async_.contains(result.media_url_)) {
    loading_async_.removeAll(result.media_url_);
  }

  // Might've been an async load, so check we're still on the same item
  PlaylistItemPtr item = app_->playlist_manager()->active()->current_item();
  if (!item) {
    return;
  }
  int next_row = app_->playlist_manager()->active()->next_row();
  const bool has_next_row = next_row != -1;
  PlaylistItemPtr next_item;
  if (has_next_row) {
    next_item = app_->playlist_manager()->active()->item_at(next_row);
  }

  bool is_current(false);
  bool is_next(false);

  if (result.media_url_ == item->Url()) {
    is_current = true;
  }
  else if (has_next_row && next_item->Url() == result.media_url_) {
    is_next = true;
  }
  else {
    return;
  }

  switch (result.type_) {
    case UrlHandler::LoadResult::Type::Error:
      if (is_current) {
        InvalidSongRequested(result.media_url_);
      }
      emit Error(result.error_);
      break;

    case UrlHandler::LoadResult::Type::NoMoreTracks:
      qLog(Debug) << "URL handler for" << result.media_url_ << "said no more tracks" << is_current;
      if (is_current) NextItem(stream_change_type_, autoscroll_);
      break;

    case UrlHandler::LoadResult::Type::TrackAvailable: {

      qLog(Debug) << "URL handler for" << result.media_url_ << "returned" << result.stream_url_;

      Song song;
      if (is_current) song = item->Metadata();
      else if (is_next) song = next_item->Metadata();

      bool update(false);

      // Set the stream url in the temporary metadata.
      if (
        (result.stream_url_.isValid())
        &&
        (result.stream_url_ != song.url())
         )
      {
        song.set_stream_url(result.stream_url_);
        update = true;
      }

      // If there was no filetype in the song's metadata, use the one provided by URL handler, if there is one.
      if (
        (song.filetype() == Song::FileType::Unknown && result.filetype_ != Song::FileType::Unknown)
        ||
        (song.filetype() == Song::FileType::Stream && result.filetype_ != Song::FileType::Stream)
         )
      {
        song.set_filetype(result.filetype_);
        update = true;
      }

      // If there was no samplerate info in song's metadata, use the one provided by URL handler, if there is one.
      if (song.samplerate() <= 0 && result.samplerate_ > 0) {
        song.set_samplerate(result.samplerate_);
        update = true;
      }

      // If there was no bit depth info in song's metadata, use the one provided by URL handler, if there is one.
      if (song.bitdepth() <= 0 && result.bit_depth_ > 0) {
        song.set_bitdepth(result.bit_depth_);
        update = true;
      }

      // If there was no length info in song's metadata, use the one provided by URL handler, if there is one.
      if (song.length_nanosec() <= 0 && result.length_nanosec_ != -1) {
        song.set_length_nanosec(result.length_nanosec_);
        update = true;
      }

      if (update) {
        if (is_current) {
          item->SetTemporaryMetadata(song);
          app_->playlist_manager()->active()->InformOfCurrentSongChange(autoscroll_, true);
          app_->playlist_manager()->active()->UpdateScrobblePoint();
        }
        else if (is_next) {
          next_item->SetTemporaryMetadata(song);
          app_->playlist_manager()->active()->ItemChanged(next_row);
        }
      }

      if (is_current) {
        qLog(Debug) << "Playing song" << item->Metadata().title() << result.stream_url_ << "position" << play_offset_nanosec_;
        engine_->Play(result.media_url_, result.stream_url_, stream_change_type_, song.has_cue(), song.beginning_nanosec(), song.end_nanosec(), play_offset_nanosec_, song.ebur128_integrated_loudness_lufs());
        current_item_ = item;
        play_offset_nanosec_ = 0;
      }
      else if (is_next && !item->Metadata().is_module_music()) {
        qLog(Debug) << "Preloading next song" << next_item->Metadata().title() << result.stream_url_;
        engine_->StartPreloading(next_item->Url(), result.stream_url_, song.has_cue(), song.beginning_nanosec(), song.end_nanosec());
      }

      break;
    }

    case UrlHandler::LoadResult::Type::WillLoadAsynchronously:
      qLog(Debug) << "URL handler for" << result.media_url_ << "is loading asynchronously";

      // We'll get called again later with either NoMoreTracks or TrackAvailable
      loading_async_ << result.media_url_;
      break;
  }

}

void Player::Next() { NextInternal(EngineBase::TrackChangeType::Manual, Playlist::AutoScroll::Always); }

void Player::NextInternal(const EngineBase::TrackChangeFlags change, const Playlist::AutoScroll autoscroll) {

  pause_time_ = QDateTime();
  play_offset_nanosec_ = 0;

  if (HandleStopAfter(autoscroll)) return;

  NextItem(change, autoscroll);

}

void Player::NextItem(const EngineBase::TrackChangeFlags change, const Playlist::AutoScroll autoscroll) {

  pause_time_ = QDateTime();
  play_offset_nanosec_ = 0;

  Playlist *active_playlist = app_->playlist_manager()->active();

  // If we received too many errors in auto change, with repeat enabled, we stop
  if (change & EngineBase::TrackChangeType::Auto) {
    const PlaylistSequence::RepeatMode repeat_mode = active_playlist->sequence()->repeat_mode();
    if (repeat_mode != PlaylistSequence::RepeatMode::Off) {
      if ((repeat_mode == PlaylistSequence::RepeatMode::Track && nb_errors_received_ >= 3) || (nb_errors_received_ >= app_->playlist_manager()->active()->filter()->rowCount())) {
        // We received too many "Error" state changes: probably looping over a playlist which contains only unavailable elements: stop now.
        nb_errors_received_ = 0;
        Stop();
        return;
      }
    }
  }

  if (nb_errors_received_ >= 100) {
    Stop();
    return;
  }

  // Manual track changes override "Repeat track"
  const bool ignore_repeat_track = change & EngineBase::TrackChangeType::Manual;

  int i = active_playlist->next_row(ignore_repeat_track);
  if (i == -1) {
    app_->playlist_manager()->active()->set_current_row(i);
    app_->playlist_manager()->active()->reset_last_played();
    emit PlaylistFinished();
    Stop();
    return;
  }

  PlayAt(i, 0, change, autoscroll, false, true);

}

void Player::PlayPlaylist(const QString &playlist_name) {
  PlayPlaylistInternal(EngineBase::TrackChangeType::Manual, Playlist::AutoScroll::Always, playlist_name);
}

void Player::PlayPlaylistInternal(const EngineBase::TrackChangeFlags change, const Playlist::AutoScroll autoscroll, const QString &playlist_name) {

  pause_time_ = QDateTime();
  play_offset_nanosec_ = 0;

  Playlist *playlist = nullptr;
  for (Playlist *p : app_->playlist_manager()->GetAllPlaylists()) {
    if (playlist_name == app_->playlist_manager()->GetPlaylistName(p->id())) {
      playlist = p;
      break;
    }
  }

  if (playlist == nullptr) {
    qLog(Warning) << "Playlist '" << playlist_name << "' not found.";
    return;
  }

  app_->playlist_manager()->SetActivePlaylist(playlist->id());
  app_->playlist_manager()->SetCurrentPlaylist(playlist->id());
  if (playlist->rowCount() == 0) return;

  int i = app_->playlist_manager()->active()->current_row();
  if (i == -1) i = app_->playlist_manager()->active()->last_played_row();
  if (i == -1) i = 0;

  PlayAt(i, 0, change, autoscroll, true);

}


bool Player::HandleStopAfter(const Playlist::AutoScroll autoscroll) {

  if (app_->playlist_manager()->active()->stop_after_current()) {
    // Find what the next track would've been, and mark that one as current, so it plays next time the user presses Play.
    const int next_row = app_->playlist_manager()->active()->next_row();
    if (next_row != -1) {
      app_->playlist_manager()->active()->set_current_row(next_row, autoscroll, true);
    }

    app_->playlist_manager()->active()->StopAfter(-1);

    Stop(true);
    return true;
  }

  return false;

}

void Player::TrackEnded() {

  if (current_item_ && current_item_->IsLocalCollectionItem() && current_item_->Metadata().id() != -1) {
    app_->playlist_manager()->collection_backend()->IncrementPlayCountAsync(current_item_->Metadata().id());
  }

  if (HandleStopAfter(Playlist::AutoScroll::Maybe)) return;

  NextInternal(EngineBase::TrackChangeType::Auto, Playlist::AutoScroll::Maybe);

}

void Player::PlayPause(const quint64 offset_nanosec, const Playlist::AutoScroll autoscroll) {

  switch (engine_->state()) {
    case EngineBase::State::Paused:
      UnPause();
      emit Resumed();
      break;

    case EngineBase::State::Playing: {
      if (current_item_->options() & PlaylistItem::Option::PauseDisabled) {
        Stop();
      }
      else {
        pause_time_ = QDateTime::currentDateTime();
        play_offset_nanosec_ = engine_->position_nanosec();
        engine_->Pause();
      }
      break;
    }

    case EngineBase::State::Empty:
    case EngineBase::State::Error:
    case EngineBase::State::Idle: {
      pause_time_ = QDateTime();
      play_offset_nanosec_ = offset_nanosec;
      app_->playlist_manager()->SetActivePlaylist(app_->playlist_manager()->current_id());
      if (app_->playlist_manager()->active()->rowCount() == 0) break;
      int i = app_->playlist_manager()->active()->current_row();
      if (i == -1) i = app_->playlist_manager()->active()->last_played_row();
      if (i == -1) i = 0;
      PlayAt(i, offset_nanosec, EngineBase::TrackChangeType::First, autoscroll, true);
      break;
    }
  }

}

void Player::UnPause() {

  if (current_item_ && pause_time_.isValid()) {
    const Song &song = current_item_->Metadata();
    if (url_handlers_.contains(song.url().scheme()) && song.stream_url_can_expire()) {
      const quint64 time = QDateTime::currentDateTime().toSecsSinceEpoch() - pause_time_.toSecsSinceEpoch();
      if (time >= 30) {  // Stream URL might be expired.
        qLog(Debug) << "Re-requesting stream URL for" << song.url();
        play_offset_nanosec_ = engine_->position_nanosec();
        HandleLoadResult(url_handlers_[song.url().scheme()]->StartLoading(song.url()));
        return;
      }
    }
  }

  pause_time_ = QDateTime();
  play_offset_nanosec_ = 0;

  engine_->Unpause();

}

void Player::RestartOrPrevious() {

  pause_time_ = QDateTime();
  play_offset_nanosec_ = 0;

  if (engine_->position_nanosec() < 8 * kNsecPerSec) {
    Previous();
    return;
  }

  SeekTo(0);

}

void Player::Stop(const bool stop_after) {

  engine_->Stop(stop_after);
  app_->playlist_manager()->active()->set_current_row(-1);
  current_item_.reset();
  pause_time_ = QDateTime();
  play_offset_nanosec_ = 0;

}

void Player::StopAfterCurrent() {
  app_->playlist_manager()->active()->StopAfter(app_->playlist_manager()->active()->current_row());
}

bool Player::PreviousWouldRestartTrack() const {

  // Check if it has been over two seconds since previous button was pressed
  return menu_previousmode_ == BehaviourSettingsPage::PreviousBehaviour::Restart && last_pressed_previous_.isValid() && last_pressed_previous_.secsTo(QDateTime::currentDateTime()) >= 2;

}

void Player::Previous() { PreviousItem(EngineBase::TrackChangeType::Manual); }

void Player::PreviousItem(const EngineBase::TrackChangeFlags change) {

  pause_time_ = QDateTime();
  play_offset_nanosec_ = 0;

  const bool ignore_repeat_track = change & EngineBase::TrackChangeType::Manual;

  if (menu_previousmode_ == BehaviourSettingsPage::PreviousBehaviour::Restart) {
    // Check if it has been over two seconds since previous button was pressed
    QDateTime now = QDateTime::currentDateTime();
    if (last_pressed_previous_.isValid() && last_pressed_previous_.secsTo(now) >= 2) {
      last_pressed_previous_ = now;
      PlayAt(app_->playlist_manager()->active()->current_row(), 0, change, Playlist::AutoScroll::Always, false, true);
      return;
    }
    last_pressed_previous_ = now;
  }

  int i = app_->playlist_manager()->active()->previous_row(ignore_repeat_track);
  app_->playlist_manager()->active()->set_current_row(i, Playlist::AutoScroll::Always, false);
  if (i == -1) {
    Stop();
    PlayAt(i, 0, change, Playlist::AutoScroll::Always, true);
    return;
  }

  PlayAt(i, 0, change, Playlist::AutoScroll::Always, false);

}

void Player::EngineStateChanged(const EngineBase::State state) {

  if (state == EngineBase::State::Error) {
    nb_errors_received_++;
  }
  else {
    nb_errors_received_ = 0;
  }

  switch (state) {
    case EngineBase::State::Paused:
      pause_time_ = QDateTime::currentDateTime();
      play_offset_nanosec_ = engine_->position_nanosec();
      emit Paused();
      break;
    case EngineBase::State::Playing:
      pause_time_ = QDateTime();
      play_offset_nanosec_ = 0;
      emit Playing();
      break;
    case EngineBase::State::Error:
      emit Error();
      [[fallthrough]];
    case EngineBase::State::Empty:
    case EngineBase::State::Idle:
      pause_time_ = QDateTime();
      play_offset_nanosec_ = 0;
      emit Stopped();
      break;
  }

  last_state_ = state;

}

uint Player::GetVolume() const {

  return engine_->volume();

}

void Player::SetVolumeFromSlider(const int value) {

  const uint volume = static_cast<uint>(qBound(0, value, 100));
  if (volume != volume_) {
    volume_ = volume;
    engine_->SetVolume(volume);
    emit VolumeChanged(volume);
  }

}

void Player::SetVolumeFromEngine(const uint volume) {

  const uint new_volume = qBound(0U, volume, 100U);
  if (new_volume != volume_) {
    volume_ = new_volume;
    emit VolumeChanged(new_volume);
  }

}

void Player::SetVolume(const uint volume) {

  const uint new_volume = qBound(0U, volume, 100U);
  if (new_volume != volume_) {
    volume_ = new_volume;
    engine_->SetVolume(new_volume);
    emit VolumeChanged(new_volume);
  }

}

void Player::VolumeUp() {

  uint old_volume = GetVolume();
  uint new_volume = std::min(old_volume + 5, static_cast<uint>(100));
  if (new_volume == old_volume) return;
  SetVolume(new_volume);

}

void Player::VolumeDown() {

  uint old_volume = GetVolume();
  uint new_volume = static_cast<uint>(std::max(static_cast<int>(old_volume) - 5, 0));
  if (new_volume == old_volume) return;
  SetVolume(new_volume);

}

void Player::PlayAt(const int index, const quint64 offset_nanosec, EngineBase::TrackChangeFlags change, const Playlist::AutoScroll autoscroll, const bool reshuffle, const bool force_inform) {

  pause_time_ = QDateTime();
  play_offset_nanosec_ = offset_nanosec;

  if (current_item_ && change & EngineBase::TrackChangeType::Manual && engine_->position_nanosec() != engine_->length_nanosec()) {
    emit TrackSkipped(current_item_);
  }

  if (current_item_ && app_->playlist_manager()->active()->has_item_at(index) && current_item_->Metadata().IsOnSameAlbum(app_->playlist_manager()->active()->item_at(index)->Metadata())) {
    change |= EngineBase::TrackChangeType::SameAlbum;
  }

  if (reshuffle) app_->playlist_manager()->active()->ReshuffleIndices();

  app_->playlist_manager()->active()->set_current_row(index, autoscroll, false, force_inform);
  if (app_->playlist_manager()->active()->current_row() == -1) {
    // Maybe index didn't exist in the playlist.
    return;
  }

  current_item_ = app_->playlist_manager()->active()->current_item();
  const QUrl url = current_item_->StreamUrl();

  if (url_handlers_.contains(url.scheme())) {
    // It's already loading
    if (loading_async_.contains(url)) {
      return;
    }

    stream_change_type_ = change;
    autoscroll_ = autoscroll;
    HandleLoadResult(url_handlers_[url.scheme()]->StartLoading(url));
  }
  else {
    qLog(Debug) << "Playing song" << current_item_->Metadata().title() << url << "position" << offset_nanosec;
    engine_->Play(current_item_->Url(), url, change, current_item_->Metadata().has_cue(), current_item_->effective_beginning_nanosec(), current_item_->effective_end_nanosec(), offset_nanosec, current_item_->effective_ebur128_integrated_loudness_lufs());
  }

}

void Player::CurrentMetadataChanged(const Song &metadata) {

  // Those things might have changed (especially when a previously invalid song was reloaded) so we push the latest version into Engine
  engine_->RefreshMarkers(metadata.beginning_nanosec(), metadata.end_nanosec());

}

void Player::SeekTo(const quint64 seconds) {

  const qint64 length_nanosec = engine_->length_nanosec();

  // If the length is 0 then either there is no song playing, or the song isn't seekable.
  if (length_nanosec <= 0) {
    return;
  }

  const qint64 nanosec = qBound(0LL, static_cast<qint64>(seconds) * kNsecPerSec, length_nanosec);
  engine_->Seek(nanosec);

  qLog(Debug) << "Track seeked to" << nanosec << "ns - updating scrobble point";
  app_->playlist_manager()->active()->UpdateScrobblePoint(nanosec);

  emit Seeked(nanosec / 1000);

  if (seconds == 0) {
    app_->playlist_manager()->active()->InformOfCurrentSongChange(Playlist::AutoScroll::Maybe, false);
  }

}

void Player::SeekForward() {
  SeekTo(engine()->position_nanosec() / kNsecPerSec + seek_step_sec_);
}

void Player::SeekBackward() {
  SeekTo(engine()->position_nanosec() / kNsecPerSec - seek_step_sec_);
}

void Player::EngineMetadataReceived(const EngineMetadata &engine_metadata) {

  if (engine_metadata.type == EngineMetadata::Type::Any || engine_metadata.type == EngineMetadata::Type::Current) {
    PlaylistItemPtr item = app_->playlist_manager()->active()->current_item();
    if (item && engine_metadata.media_url == item->Url()) {
      Song song = item->Metadata();
      bool minor = song.MergeFromEngineMetadata(engine_metadata);
      app_->playlist_manager()->active()->SetStreamMetadata(item->Url(), song, minor);
      return;
    }
  }

  if (engine_metadata.type == EngineMetadata::Type::Any || engine_metadata.type == EngineMetadata::Type::Next) {
    int next_row = app_->playlist_manager()->active()->next_row();
    if (next_row != -1) {
      PlaylistItemPtr next_item = app_->playlist_manager()->active()->item_at(next_row);
      if (engine_metadata.media_url == next_item->Url()) {
        Song song = next_item->Metadata();
        song.MergeFromEngineMetadata(engine_metadata);
        next_item->SetTemporaryMetadata(song);
        app_->playlist_manager()->active()->ItemChanged(next_row);
      }
    }
  }

}

PlaylistItemPtr Player::GetItemAt(const int pos) const {

  if (pos < 0 || pos >= app_->playlist_manager()->active()->rowCount())
    return PlaylistItemPtr();
  return app_->playlist_manager()->active()->item_at(pos);

}

void Player::Mute() {

  const uint current_volume = engine_->volume();

  if (current_volume == 0) {
    SetVolume(volume_before_mute_);
  }
  else {
    volume_before_mute_ = current_volume;
    SetVolume(0);
  }

}

void Player::Pause() { engine_->Pause(); }

void Player::Play(const quint64 offset_nanosec) {

  switch (GetState()) {
    case EngineBase::State::Playing:
      SeekTo(offset_nanosec);
      break;
    case EngineBase::State::Paused:
      UnPause();
      break;
    default:
      PlayPause(offset_nanosec);
      break;
  }

}

void Player::ShowOSD() {
  if (current_item_) emit ForceShowOSD(current_item_->Metadata(), false);
}

void Player::TogglePrettyOSD() {
  if (current_item_) emit ForceShowOSD(current_item_->Metadata(), true);
}

void Player::TrackAboutToEnd() {

  const bool has_next_row = app_->playlist_manager()->active()->next_row() != -1;
  PlaylistItemPtr next_item;

  if (has_next_row) {
    next_item = app_->playlist_manager()->active()->item_at(app_->playlist_manager()->active()->next_row());
  }

  if (engine_->is_autocrossfade_enabled()) {
    // Crossfade is on, so just start playing the next track.  The current one will fade out, and the new one will fade in

    // But, if there's no next track, and we don't want to fade out, then do nothing and just let the track finish to completion.
    if (!engine_->is_fadeout_enabled() && !has_next_row) return;

    // If the next track is on the same album (or same cue file),
    // and the user doesn't want to crossfade between tracks on the same album, then don't do this automatic crossfading.
    if (engine_->crossfade_same_album() || !has_next_row || !next_item || !current_item_->Metadata().IsOnSameAlbum(next_item->Metadata())) {
      TrackEnded();
      return;
    }
  }

  // Crossfade is off, so start preloading the next track, so we don't get a gap between songs.
  if (!has_next_row || !next_item) return;

  QUrl url = next_item->StreamUrl();

  // Get the actual track URL rather than the stream URL.
  if (url_handlers_.contains(url.scheme())) {
    if (loading_async_.contains(url)) return;
    autoscroll_ = Playlist::AutoScroll::Maybe;
    UrlHandler::LoadResult result = url_handlers_[url.scheme()]->StartLoading(url);
    switch (result.type_) {
      case UrlHandler::LoadResult::Type::Error:
        emit Error(result.error_);
        return;
      case UrlHandler::LoadResult::Type::NoMoreTracks:
        return;
      case UrlHandler::LoadResult::Type::WillLoadAsynchronously:
        loading_async_ << url;
        return;
      case UrlHandler::LoadResult::Type::TrackAvailable:
        qLog(Debug) << "URL handler for" << result.media_url_ << "returned" << result.stream_url_;
        url = result.stream_url_;
        Song song = next_item->Metadata();
        song.set_stream_url(url);
        next_item->SetTemporaryMetadata(song);
        break;
    }
  }

  // Preloading any format while currently playing module music is broken in GStreamer.
  // See: https://gitlab.freedesktop.org/gstreamer/gstreamer/-/issues/769
  if (current_item_ && current_item_->Metadata().is_module_music()) {
    return;
  }

  engine_->StartPreloading(next_item->Url(), url, next_item->Metadata().has_cue(), next_item->effective_beginning_nanosec(), next_item->effective_end_nanosec());

}

void Player::FatalError() {
  nb_errors_received_ = 0;
  Stop();
}

void Player::ValidSongRequested(const QUrl &url) {
  emit SongChangeRequestProcessed(url, true);
}

void Player::InvalidSongRequested(const QUrl &url) {

  if (greyout_) emit SongChangeRequestProcessed(url, false);

  if (!continue_on_error_) {
    FatalError();
    return;
  }

  NextItem(EngineBase::TrackChangeType::Auto, Playlist::AutoScroll::Maybe);

}

void Player::RegisterUrlHandler(UrlHandler *handler) {

  const QString scheme = handler->scheme();

  if (url_handlers_.contains(scheme)) {
    qLog(Warning) << "Tried to register a URL handler for" << scheme << "but one was already registered";
    return;
  }

  qLog(Info) << "Registered URL handler for" << scheme;
  url_handlers_.insert(scheme, handler);
  QObject::connect(handler, &UrlHandler::destroyed, this, &Player::UrlHandlerDestroyed);
  QObject::connect(handler, &UrlHandler::AsyncLoadComplete, this, &Player::HandleLoadResult);

}

void Player::UnregisterUrlHandler(UrlHandler *handler) {

  const QString scheme = url_handlers_.key(handler);
  if (scheme.isEmpty()) {
    qLog(Warning) << "Tried to unregister a URL handler for" << handler->scheme() << "that wasn't registered";
    return;
  }

  qLog(Info) << "Unregistered URL handler for" << scheme;
  url_handlers_.remove(scheme);
  QObject::disconnect(handler, &UrlHandler::destroyed, this, &Player::UrlHandlerDestroyed);
  QObject::disconnect(handler, &UrlHandler::AsyncLoadComplete, this, &Player::HandleLoadResult);

}

const UrlHandler *Player::HandlerForUrl(const QUrl &url) const {

  QMap<QString, UrlHandler*>::const_iterator it = url_handlers_.constFind(url.scheme());
  if (it == url_handlers_.constEnd()) {
    return nullptr;
  }
  return *it;

}

void Player::UrlHandlerDestroyed(QObject *object) {

  UrlHandler *handler = static_cast<UrlHandler*>(object);
  const QString scheme = url_handlers_.key(handler);
  if (!scheme.isEmpty()) {
    url_handlers_.remove(scheme);
  }

}

void Player::HandleAuthentication() {
  emit Authenticated();
}
