/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, David Sansome <me@davidsansome.com>
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
#include "version.h"

#include <cstdlib>
#include <getopt.h>
#include <iostream>

#include <QtGlobal>
#include <QObject>
#include <QIODevice>
#include <QDataStream>
#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QByteArray>
#include <QString>
#include <QUrl>

#include "commandlineoptions.h"
#include "core/logging.h"

const char *CommandlineOptions::kHelpText =
    "%1: strawberry [%2] [%3]\n"
    "\n"
    "%4:\n"
    "  -p, --play                 %5\n"
    "  -t, --play-pause           %6\n"
    "  -u, --pause                %7\n"
    "  -s, --stop                 %8\n"
    "  -q, --stop-after-current   %9\n"
    "  -r, --previous             %10\n"
    "  -f, --next                 %11\n"
    "  -v, --volume <value>       %12\n"
    "  --volume-up                %13\n"
    "  --volume-down              %14\n"
    "  --volume-increase-by       %15\n"
    "  --volume-decrease-by       %16\n"
    "  --seek-to <seconds>        %17\n"
    "  --seek-by <seconds>        %18\n"
    "  --restart-or-previous      %19\n"
    "\n"
    "%20:\n"
    "  -c, --create <name>        %21\n"
    "  -a, --append               %22\n"
    "  -l, --load                 %23\n"
    "  -k, --play-track <n>       %24\n"
    "  -i, --play-playlist <name> %25\n"
    "\n"
    "%26:\n"
    "  -o, --show-osd             %27\n"
    "  -y, --toggle-pretty-osd    %28\n"
    "  -g, --language <lang>      %29\n"
    "  -w, --resize-window <WxH>  %30\n"
    "      --quiet                %31\n"
    "      --verbose              %32\n"
    "      --log-levels <levels>  %33\n"
    "      --version              %34\n";

const char *CommandlineOptions::kVersionText = "Strawberry %1";

CommandlineOptions::CommandlineOptions(int argc, char **argv)
    : argc_(argc),
      argv_(argv),
      url_list_action_(UrlListAction::None),
      player_action_(PlayerAction::None),
      set_volume_(-1),
      volume_modifier_(0),
      seek_to_(-1),
      seek_by_(0),
      play_track_at_(-1),
      show_osd_(false),
      toggle_pretty_osd_(false),
      log_levels_(logging::kDefaultLogLevels) {

#ifdef Q_OS_MACOS
  // Remove -psn_xxx option that Mac passes when opened from Finder.
  RemoveArg("-psn", 1);
#endif

  // Remove the -session option that KDE passes
  RemoveArg("-session", 2);
}

void CommandlineOptions::RemoveArg(const QString &starts_with, int count) {

  for (int i = 0; i < argc_; ++i) {
    QString opt(argv_[i]);
    if (opt.startsWith(starts_with)) {
      for (int j = i; j < argc_ - count + 1; ++j) {
        argv_[j] = argv_[j + count];
      }
      argc_ -= count;
      break;
    }
  }

}

bool CommandlineOptions::Parse() {

  static const struct option kOptions[] = {
      {"help", no_argument, nullptr, 'h'},
      {"play", no_argument, nullptr, 'p'},
      {"play-pause", no_argument, nullptr, 't'},
      {"pause", no_argument, nullptr, 'u'},
      {"stop", no_argument, nullptr, 's'},
      {"stop-after-current", no_argument, nullptr, 'q'},
      {"previous", no_argument, nullptr, 'r'},
      {"next", no_argument, nullptr, 'f'},
      {"volume", required_argument, nullptr, 'v'},
      {"volume-up", no_argument, nullptr, LongOptions::VolumeUp},
      {"volume-down", no_argument, nullptr, LongOptions::VolumeDown},
      {"volume-increase-by", required_argument, nullptr, LongOptions::VolumeIncreaseBy},
      {"volume-decrease-by", required_argument, nullptr, LongOptions::VolumeDecreaseBy},
      {"seek-to", required_argument, nullptr, LongOptions::SeekTo},
      {"seek-by", required_argument, nullptr, LongOptions::SeekBy},
      {"restart-or-previous", no_argument, nullptr, LongOptions::RestartOrPrevious},
      {"create", required_argument, nullptr, 'c'},
      {"append", no_argument, nullptr, 'a'},
      {"load", no_argument, nullptr, 'l'},
      {"play-track", required_argument, nullptr, 'k'},
      {"play-playlist", required_argument, nullptr, 'i'},
      {"show-osd", no_argument, nullptr, 'o'},
      {"toggle-pretty-osd", no_argument, nullptr, 'y'},
      {"language", required_argument, nullptr, 'g'},
      {"resize-window", required_argument, nullptr, 'w'},
      {"quiet", no_argument, nullptr, LongOptions::Quiet},
      {"verbose", no_argument, nullptr, LongOptions::Verbose},
      {"log-levels", required_argument, nullptr, LongOptions::LogLevels},
      {"version", no_argument, nullptr, LongOptions::Version},
      {nullptr, 0, nullptr, 0}};

  // Parse the arguments
  bool ok = false;
  forever {
    int c = getopt_long(argc_, argv_, "hptusqrfv:c:alk:i:oyg:w:", kOptions, nullptr);

    // End of the options
    if (c == -1) break;

    switch (c) {
      case 'h': {
        QString translated_help_text =
            QString(kHelpText)
                .arg(tr("Usage"), tr("options"), tr("URL(s)"),
                     tr("Player options"),
                     tr("Start the playlist currently playing"),
                     tr("Play if stopped, pause if playing"),
                     tr("Pause playback"), tr("Stop playback"),
                     tr("Stop playback after current track"))
                .arg(tr("Skip backwards in playlist"),
                     tr("Skip forwards in playlist"),
                     tr("Set the volume to <value> percent"),
                     tr("Increase the volume by 4 percent"),
                     tr("Decrease the volume by 4 percent"),
                     tr("Increase the volume by <value> percent"),
                     tr("Decrease the volume by <value> percent"))
                .arg(tr("Seek the currently playing track to an absolute position"),
                     tr("Seek the currently playing track by a relative amount"),
                     tr("Restart the track, or play the previous track if within 8 seconds of start."),
                     tr("Playlist options"),
                     tr("Create a new playlist with files"),
                     tr("Append files/URLs to the playlist"),
                     tr("Loads files/URLs, replacing current playlist"),
                     tr("Play the <n>th track in the playlist"),
                     tr("Play given playlist"))
                .arg(tr("Other options"), tr("Display the on-screen-display"),
                     tr("Toggle visibility for the pretty on-screen-display"),
                     tr("Change the language"),
                     tr("Resize the window"),
                     tr("Equivalent to --log-levels *:1"),
                     tr("Equivalent to --log-levels *:3"),
                     tr("Comma separated list of class:level, level is 0-3"))
                .arg(tr("Print out version information"));

        std::cout << translated_help_text.toLocal8Bit().constData();
        return false;
      }

      case 'p':
        player_action_ = PlayerAction::Play;
        break;
      case 't':
        player_action_ = PlayerAction::PlayPause;
        break;
      case 'u':
        player_action_ = PlayerAction::Pause;
        break;
      case 's':
        player_action_ = PlayerAction::Stop;
        break;
      case 'q':
        player_action_ = PlayerAction::StopAfterCurrent;
        break;
      case 'r':
        player_action_ = PlayerAction::Previous;
        break;
      case 'f':
        player_action_ = PlayerAction::Next;
        break;
      case 'i':
        player_action_ = PlayerAction::PlayPlaylist;
        playlist_name_ = QString(optarg);
        break;
      case 'c':
        url_list_action_ = UrlListAction::CreateNew;
        playlist_name_ = QString(optarg);
        break;
      case 'a':
        url_list_action_ = UrlListAction::Append;
        break;
      case 'l':
        url_list_action_ = UrlListAction::Load;
        break;
      case 'o':
        show_osd_ = true;
        break;
      case 'y':
        toggle_pretty_osd_ = true;
        break;
      case 'g':
        language_ = QString(optarg);
        break;
      case LongOptions::VolumeUp:
        volume_modifier_ = +4;
        break;
      case LongOptions::VolumeDown:
        volume_modifier_ = -4;
        break;
      case LongOptions::Quiet:
        log_levels_ = "1";
        break;
      case LongOptions::Verbose:
        log_levels_ = "3";
        break;
      case LongOptions::LogLevels:
        log_levels_ = QString(optarg);
        break;
      case LongOptions::Version: {
        QString version_text = QString(kVersionText).arg(STRAWBERRY_VERSION_DISPLAY);
        std::cout << version_text.toLocal8Bit().constData() << std::endl;
        std::exit(0);
      }
      case 'v':
        set_volume_ = QString(optarg).toInt(&ok);
        if (!ok) set_volume_ = -1;
        break;

      case LongOptions::VolumeIncreaseBy:
        volume_modifier_ = QString(optarg).toInt(&ok);
        if (!ok) volume_modifier_ = 0;
        break;

      case LongOptions::VolumeDecreaseBy:
        volume_modifier_ = -QString(optarg).toInt(&ok);
        if (!ok) volume_modifier_ = 0;
        break;

      case LongOptions::SeekTo:
        seek_to_ = QString(optarg).toInt(&ok);
        if (!ok) seek_to_ = -1;
        break;

      case LongOptions::SeekBy:
        seek_by_ = QString(optarg).toInt(&ok);
        if (!ok) seek_by_ = 0;
        break;

      case LongOptions::RestartOrPrevious:
        player_action_ = PlayerAction::RestartOrPrevious;
        break;

      case 'k':
        play_track_at_ = QString(optarg).toInt(&ok);
        if (!ok) play_track_at_ = -1;
        break;

      case 'w':
        window_size_ = QString(optarg);
        player_action_ = PlayerAction::ResizeWindow;
        break;

      case '?':
      default:
        return false;
    }
  }

  // Get any filenames or URLs following the arguments
  for (int i = optind; i < argc_; ++i) {
    QString value = QFile::decodeName(argv_[i]);
    QFileInfo file_info(value);
    if (file_info.exists()) {
      urls_ << QUrl::fromLocalFile(file_info.canonicalFilePath());
    }
    else {
      urls_ << QUrl::fromUserInput(value);
    }
  }

  return true;

}

bool CommandlineOptions::is_empty() const {
  return player_action_ == PlayerAction::None &&
         set_volume_ == -1 &&
         volume_modifier_ == 0 &&
         seek_to_ == -1 &&
         seek_by_ == 0 &&
         play_track_at_ == -1 &&
         !show_osd_ &&
         !toggle_pretty_osd_ &&
         urls_.isEmpty();
}

bool CommandlineOptions::contains_play_options() const {
  return player_action_ != PlayerAction::None || play_track_at_ != -1 || !urls_.isEmpty();
}

QByteArray CommandlineOptions::Serialize() const {

  QBuffer buf;
  if (buf.open(QIODevice::WriteOnly)) {
    QDataStream s(&buf);
    s << *this;
    buf.close();
  }

  return buf.data().toBase64();

}

void CommandlineOptions::Load(const QByteArray &serialized) {

  QByteArray copy = QByteArray::fromBase64(serialized);
  QBuffer buf(&copy);
  if (buf.open(QIODevice::ReadOnly)) {
    QDataStream s(&buf);
    s >> *this;
  }

}

QString CommandlineOptions::tr(const char *source_text) {
  return QObject::tr(source_text);  // clazy:exclude=tr-non-literal
}

QDataStream &operator<<(QDataStream &s, const CommandlineOptions &a) {

  s << static_cast<quint32>(a.player_action_)
    << static_cast<quint32>(a.url_list_action_)
    << a.set_volume_
    << a.volume_modifier_
    << a.seek_to_
    << a.seek_by_
    << a.play_track_at_
    << a.show_osd_
    << a.urls_
    << a.log_levels_
    << a.toggle_pretty_osd_
    << a.playlist_name_
    << a.window_size_;

  return s;

}

QDataStream &operator>>(QDataStream &s, CommandlineOptions &a) {

  quint32 player_action = 0;
  quint32 url_list_action = 0;

  s >> player_action
    >> url_list_action
    >> a.set_volume_
    >> a.volume_modifier_
    >> a.seek_to_
    >> a.seek_by_
    >> a.play_track_at_
    >> a.show_osd_
    >> a.urls_
    >> a.log_levels_
    >> a.toggle_pretty_osd_
    >> a.playlist_name_
    >> a.window_size_;

  a.player_action_ = static_cast<CommandlineOptions::PlayerAction>(player_action);
  a.url_list_action_ = static_cast<CommandlineOptions::UrlListAction>(url_list_action);

  return s;

}
