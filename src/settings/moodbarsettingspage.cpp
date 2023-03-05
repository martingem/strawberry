/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, David Sansome <me@davidsansome.com>
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

#include "config.h"

#include <QIODevice>
#include <QFile>
#include <QByteArray>
#include <QPixmap>
#include <QPainter>
#include <QSettings>
#include <QCheckBox>
#include <QComboBox>
#include <QSize>

#include "core/iconloader.h"
#include "core/logging.h"

#include "settingsdialog.h"
#include "settingspage.h"

#ifdef HAVE_MOODBAR
#  include "moodbar/moodbarrenderer.h"
#endif

#include "moodbarsettingspage.h"
#include "ui_moodbarsettingspage.h"

const char *MoodbarSettingsPage::kSettingsGroup = "Moodbar";
const int MoodbarSettingsPage::kMoodbarPreviewWidth = 150;
const int MoodbarSettingsPage::kMoodbarPreviewHeight = 18;

MoodbarSettingsPage::MoodbarSettingsPage(SettingsDialog *dialog, QWidget *parent)
    : SettingsPage(dialog, parent),
      ui_(new Ui_MoodbarSettingsPage),
      initialized_(false) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load("moodbar", true, 0, 32));

  MoodbarSettingsPage::Load();

}

MoodbarSettingsPage::~MoodbarSettingsPage() { delete ui_; }

void MoodbarSettingsPage::Load() {

  QSettings s;
  s.beginGroup(kSettingsGroup);
  ui_->moodbar_enabled->setChecked(s.value("enabled", false).toBool());
  ui_->moodbar_show->setChecked(s.value("show", false).toBool());
  ui_->moodbar_style->setCurrentIndex(s.value("style", 0).toInt());
  ui_->moodbar_save->setChecked(s.value("save", false).toBool());
  s.endGroup();

  InitMoodbarPreviews();

  Init(ui_->layout_moodbarsettingspage->parentWidget());

  if (!QSettings().childGroups().contains(kSettingsGroup)) set_changed();

}

void MoodbarSettingsPage::Save() {

  QSettings s;
  s.beginGroup(kSettingsGroup);
  s.setValue("enabled", ui_->moodbar_enabled->isChecked());
  s.setValue("show", ui_->moodbar_show->isChecked());
  s.setValue("style", ui_->moodbar_style->currentIndex());
  s.setValue("save", ui_->moodbar_save->isChecked());
  s.endGroup();
}

void MoodbarSettingsPage::Cancel() {}

void MoodbarSettingsPage::InitMoodbarPreviews() {

  if (initialized_) return;
  initialized_ = true;

  const QSize preview_size(kMoodbarPreviewWidth, kMoodbarPreviewHeight);
  ui_->moodbar_style->setIconSize(preview_size);

  // Read the sample data
  QFile file(":/mood/sample.mood");
  if (!file.open(QIODevice::ReadOnly)) {
    qLog(Warning) << "Failed to open moodbar sample file" << file.fileName() << "for reading:" << file.errorString();
    return;
  }
  QByteArray file_data = file.readAll();
  file.close();

  // Render and set each preview
  for (int i = 0; i < static_cast<int>(MoodbarRenderer::MoodbarStyle::StyleCount); ++i) {

    const MoodbarRenderer::MoodbarStyle style = static_cast<MoodbarRenderer::MoodbarStyle>(i);
    const ColorVector colors = MoodbarRenderer::Colors(file_data, style, palette());

    QPixmap pixmap(preview_size);
    QPainter p(&pixmap);
    MoodbarRenderer::Render(colors, &p, pixmap.rect());
    p.end();

    ui_->moodbar_style->addItem(MoodbarRenderer::StyleName(style));
    ui_->moodbar_style->setItemData(i, pixmap, Qt::DecorationRole);

  }

}
