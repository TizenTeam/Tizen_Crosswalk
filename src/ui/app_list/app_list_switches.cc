// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/app_list_switches.h"

#include "base/command_line.h"

namespace app_list {
namespace switches {

// If set, folder will be enabled in app list UI.
const char kEnableFolderUI[] = "enable-app-list-folder-ui";

// If set, the voice search is disabled in app list UI.
const char kDisableVoiceSearch[] = "disable-app-list-voice-search";

bool IsFolderUIEnabled() {
  return CommandLine::ForCurrentProcess()->HasSwitch(kEnableFolderUI);
}

bool IsVoiceSearchEnabled() {
  // Speech recognition in AppList is only for ChromeOS right now.
#if defined(OS_CHROMEOS)
  return !CommandLine::ForCurrentProcess()->HasSwitch(kDisableVoiceSearch);
#else
  return false;
#endif
}

}  // namespcae switches
}  // namespace app_list
