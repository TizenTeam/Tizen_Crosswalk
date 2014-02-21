// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string16.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_impl.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/test_utils.h"

namespace {

// An observer that returns back to test code after a new profile is
// initialized.
void OnUnblockOnProfileCreation(const base::Closure& callback,
                                Profile* profile,
                                Profile::CreateStatus status) {
  switch (status) {
    case Profile::CREATE_STATUS_CREATED:
      // Wait for CREATE_STATUS_INITIALIZED.
      break;
    case Profile::CREATE_STATUS_INITIALIZED:
      callback.Run();
      break;
    default:
      ADD_FAILURE() << "Unexpected Profile::CreateStatus: " << status;
      callback.Run();
      break;
  }
}

// Finds a profile path corresponding to a profile that has not been loaded yet.
base::FilePath GetUnloadedProfilePath() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  const ProfileInfoCache& cache = profile_manager->GetProfileInfoCache();
  const std::vector<Profile*> loaded_profiles =
      profile_manager->GetLoadedProfiles();
  std::set<base::FilePath> profile_paths;
  for (size_t i = 0; i < cache.GetNumberOfProfiles(); ++i)
    profile_paths.insert(cache.GetPathOfProfileAtIndex(i));
  for (size_t i = 0; i < loaded_profiles.size(); ++i)
    EXPECT_EQ(1U, profile_paths.erase(loaded_profiles[i]->GetPath()));
  if (profile_paths.size())
    return *profile_paths.begin();
  return base::FilePath();
}

}  // namespace

typedef InProcessBrowserTest PrefHashBrowserTest;

IN_PROC_BROWSER_TEST_F(PrefHashBrowserTest,
                       PRE_PRE_InitializeUnloadedProfiles) {
  if (!profiles::IsMultipleProfilesEnabled())
    return;
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Create an additional profile.
  const base::FilePath new_path =
      profile_manager->GenerateNextProfileDirectoryPath();
  const scoped_refptr<content::MessageLoopRunner> runner(
      new content::MessageLoopRunner);
  profile_manager->CreateProfileAsync(
      new_path,
      base::Bind(&OnUnblockOnProfileCreation, runner->QuitClosure()),
      base::string16(),
      base::string16(),
      std::string());

  // Spin to allow profile creation to take place, loop is terminated
  // by OnUnblockOnProfileCreation when the profile is created.
  runner->Run();
}

IN_PROC_BROWSER_TEST_F(PrefHashBrowserTest,
                       PRE_InitializeUnloadedProfiles) {
  if (!profiles::IsMultipleProfilesEnabled())
    return;

  // Creating the profile would have initialized its hash store. Also, we don't
  // know whether the newly created or original profile will be launched (does
  // creating a profile cause it to be the most recently used?).

  // So we will find the profile that isn't loaded, reset its hash store, and
  // then verify in the _next_ launch that it is, indeed, restored despite not
  // having been loaded.

  const base::DictionaryValue* hashes =
      g_browser_process->local_state()->GetDictionary(
          prefs::kProfilePreferenceHashes);

  // 3 is for hash_of_hashes, default profile, and new profile.
  ASSERT_EQ(3U, hashes->size());

  // One of the two profiles should not have been loaded. Reset its hash store.
  const base::FilePath unloaded_profile_path = GetUnloadedProfilePath();
  ProfileImpl::ResetPrefHashStore(unloaded_profile_path);

  // One of the profile hash collections should be gone.
  ASSERT_EQ(2U, hashes->size());
}

IN_PROC_BROWSER_TEST_F(PrefHashBrowserTest,
                       InitializeUnloadedProfiles) {
  if (!profiles::IsMultipleProfilesEnabled())
    return;

  const base::DictionaryValue* hashes =
      g_browser_process->local_state()->GetDictionary(
          prefs::kProfilePreferenceHashes);

  // The deleted hash collection should be restored.
  ASSERT_EQ(3U, hashes->size());

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  const std::vector<Profile*> loaded_profiles =
      profile_manager->GetLoadedProfiles();

  // Verify that only one profile was loaded. We assume that the unloaded
  // profile is the same one that wasn't loaded in the last launch (i.e., it's
  // the one whose hash store we reset, and the fact that it is now restored is
  // evidence that we restored the hashes of an unloaded profile.).
  ASSERT_EQ(1U, loaded_profiles.size());
}
