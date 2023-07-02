/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2014, David Sansome <me@davidsansome.com>
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

#ifdef INTERFACE
#  undef INTERFACE
#endif

#include "config.h"

#include <dsound.h>

#include <QVariant>
#include <QString>
#include <QUuid>

#include "directsounddevicefinder.h"
#include "enginedevice.h"
#include "core/logging.h"

DirectSoundDeviceFinder::DirectSoundDeviceFinder() : DeviceFinder("directsound", { "directsound", "dsound", "directsoundsink", "directx", "directx2" }) {}

EngineDeviceList DirectSoundDeviceFinder::ListDevices() {
  State state;
  DirectSoundEnumerateA(&DirectSoundDeviceFinder::EnumerateCallback, &state);
  return state.devices;
}

BOOL CALLBACK DirectSoundDeviceFinder::EnumerateCallback(LPGUID guid, LPCSTR description, LPCSTR module, LPVOID state_voidptr) {

  Q_UNUSED(module);

  State *state = reinterpret_cast<State*>(state_voidptr);

  EngineDevice device;
  device.description = QString::fromLatin1(description);
  if (guid) device.value = QUuid(*guid).toString();
  device.iconname = device.GuessIconName();
  state->devices.append(device);

  return 1;

}
