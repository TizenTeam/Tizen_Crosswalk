// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ELF_BLACKLIST_BLACKLIST_H_
#define CHROME_ELF_BLACKLIST_BLACKLIST_H_

namespace blacklist {

// Max size of the DLL blacklist.
const int kTroublesomeDllsMaxCount = 64;

// The DLL blacklist.
extern const wchar_t* g_troublesome_dlls[kTroublesomeDllsMaxCount];

// The registry path of the blacklist beacon.
extern const wchar_t kRegistryBeaconPath[];

// The properties for the blacklist beacon.
extern const wchar_t kBeaconVersion[];
extern const wchar_t kBeaconState[];

// The states for the blacklist setup code.
enum BlacklistState {
  BLACKLIST_DISABLED = 0,
  BLACKLIST_ENABLED,
  // The blacklist setup code is running. If this is still set at startup,
  // it means the last setup crashed.
  BLACKLIST_SETUP_RUNNING,
  // Always keep this at the end.
  BLACKLIST_STATE_MAX,
};

// Attempts to leave a beacon in the current user's registry hive.
// If the blacklist beacon doesn't say it is enabled or there are any other
// errors when creating the beacon, returns false. Otherwise returns true.
// The intent of the beacon is to act as an extra failure mode protection
// whereby if Chrome for some reason fails to start during blacklist setup,
// it will skip blacklisting on the subsequent run.
bool LeaveSetupBeacon();

// Looks for the beacon that LeaveSetupBeacon() creates and resets it to
// to show the setup was successful.
// Returns true if the beacon was successfully set to BLACKLIST_ENABLED.
bool ResetBeacon();

// Return the size of the current blacklist.
int BlacklistSize();

// Adds the given dll name to the blacklist. Returns true if the dll name is in
// the blacklist when this returns, false on error. Note that this will copy
// |dll_name| and will leak it on exit if the string is not subsequently removed
// using RemoveDllFromBlacklist.
extern "C" bool AddDllToBlacklist(const wchar_t* dll_name);

// Removes the given dll name from the blacklist. Returns true if it was
// removed, false on error.
extern "C" bool RemoveDllFromBlacklist(const wchar_t* dll_name);

// Initializes the DLL blacklist in the current process. This should be called
// before any undesirable DLLs might be loaded. If |force| is set to true, then
// initialization will take place even if a beacon is present. This is useful
// for tests.
bool Initialize(bool force);

}  // namespace blacklist

#endif  // CHROME_ELF_BLACKLIST_BLACKLIST_H_
