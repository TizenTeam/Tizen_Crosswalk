// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/media_galleries_scan_result_dialog_controller.h"

#include <algorithm>
#include <list>

#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media_galleries/media_file_system_registry.h"
#include "chrome/browser/media_galleries/media_galleries_histograms.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/storage_monitor/storage_info.h"
#include "chrome/browser/storage_monitor/storage_monitor.h"
#include "chrome/common/extensions/permissions/media_galleries_permission.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Comparator for sorting OrderedScanResults -- more files first and then sorts
// by absolute path.
bool ScanResultsComparator(
    const MediaGalleriesScanResultDialogController::ScanResult& a,
    const MediaGalleriesScanResultDialogController::ScanResult& b) {
  int a_media_count = a.pref_info.image_count + a.pref_info.music_count +
                      a.pref_info.video_count;
  int b_media_count = b.pref_info.image_count + b.pref_info.music_count +
                      b.pref_info.video_count;
  if (a_media_count == b_media_count)
    return a.pref_info.AbsolutePath() < b.pref_info.AbsolutePath();
  return a_media_count > b_media_count;
}

}  // namespace

MediaGalleriesScanResultDialogController::
MediaGalleriesScanResultDialogController(
    content::WebContents* web_contents,
    const extensions::Extension& extension,
    const base::Closure& on_finish)
      : web_contents_(web_contents),
        extension_(&extension),
        on_finish_(on_finish) {
  // TODO(vandebo): Put this in the intializer list after the cocoa version is
  // done.
#if defined(USE_AURA)
  create_dialog_callback_ = base::Bind(&MediaGalleriesScanResultDialog::Create);
#endif  // USE_AURA
  preferences_ =
      g_browser_process->media_file_system_registry()->GetPreferences(
          GetProfile());
  // Passing unretained pointer is safe, since the dialog controller
  // is self-deleting, and so won't be deleted until it can be shown
  // and then closed.
  preferences_->EnsureInitialized(base::Bind(
        &MediaGalleriesScanResultDialogController::OnPreferencesInitialized,
        base::Unretained(this)));
}

MediaGalleriesScanResultDialogController::
MediaGalleriesScanResultDialogController(
    const extensions::Extension& extension,
    MediaGalleriesPreferences* preferences,
    const CreateDialogCallback& create_dialog_callback,
    const base::Closure& on_finish)
    : web_contents_(NULL),
      extension_(&extension),
      on_finish_(on_finish),
      preferences_(preferences),
      create_dialog_callback_(create_dialog_callback) {
  OnPreferencesInitialized();
}

MediaGalleriesScanResultDialogController::
~MediaGalleriesScanResultDialogController() {
  preferences_->RemoveGalleryChangeObserver(this);
  if (StorageMonitor::GetInstance())
    StorageMonitor::GetInstance()->RemoveObserver(this);
}

base::string16 MediaGalleriesScanResultDialogController::GetHeader() const {
  return l10n_util::GetStringFUTF16(
      IDS_MEDIA_GALLERIES_SCAN_RESULT_DIALOG_HEADER,
      base::UTF8ToUTF16(extension_->name()));
}

base::string16 MediaGalleriesScanResultDialogController::GetSubtext() const {
  extensions::MediaGalleriesPermission::CheckParam copy_to_param(
      extensions::MediaGalleriesPermission::kCopyToPermission);
  extensions::MediaGalleriesPermission::CheckParam delete_param(
      extensions::MediaGalleriesPermission::kDeletePermission);
  bool has_copy_to_permission =
      extensions::PermissionsData::CheckAPIPermissionWithParam(
          extension_, extensions::APIPermission::kMediaGalleries,
          &copy_to_param);
  bool has_delete_permission =
      extensions::PermissionsData::CheckAPIPermissionWithParam(
          extension_, extensions::APIPermission::kMediaGalleries,
          &delete_param);

  int id;
  if (has_copy_to_permission)
    id = IDS_MEDIA_GALLERIES_SCAN_RESULT_DIALOG_SUBTEXT_READ_WRITE;
  else if (has_delete_permission)
    id = IDS_MEDIA_GALLERIES_SCAN_RESULT_DIALOG_SUBTEXT_READ_DELETE;
  else
    id = IDS_MEDIA_GALLERIES_SCAN_RESULT_DIALOG_SUBTEXT_READ_ONLY;

  return l10n_util::GetStringFUTF16(id, base::UTF8ToUTF16(extension_->name()));
}

MediaGalleriesScanResultDialogController::OrderedScanResults
MediaGalleriesScanResultDialogController::GetGalleryList() const {
  OrderedScanResults result;
  result.reserve(scan_results_.size());
  for (ScanResults::const_iterator it = scan_results_.begin();
       it != scan_results_.end();
       ++it) {
    result.push_back(it->second);
  }
  std::sort(result.begin(), result.end(), ScanResultsComparator);
  return result;
}

void MediaGalleriesScanResultDialogController::DidToggleGalleryId(
    MediaGalleryPrefId pref_id, bool selected) {
  DCHECK(ContainsKey(scan_results_, pref_id));
  ScanResults::iterator entry = scan_results_.find(pref_id);
  entry->second.selected = selected;
}

void MediaGalleriesScanResultDialogController::DidClickOpenFolderViewer(
    MediaGalleryPrefId pref_id) const {
  ScanResults::const_iterator entry = scan_results_.find(pref_id);
  if (entry == scan_results_.end()) {
    NOTREACHED();
    return;
  }
  platform_util::OpenItem(GetProfile(), entry->second.pref_info.AbsolutePath());
}

void MediaGalleriesScanResultDialogController::DidForgetGallery(
    MediaGalleryPrefId pref_id) {
  results_to_remove_.insert(pref_id);
  scan_results_.erase(pref_id);
  dialog_->UpdateResults();
}

void MediaGalleriesScanResultDialogController::DialogFinished(bool accepted) {
  // No longer interested in preference updates (and the below code generates
  // some).
  preferences_->RemoveGalleryChangeObserver(this);

  if (accepted) {
    DCHECK(preferences_);
    for (ScanResults::const_iterator it = scan_results_.begin();
         it != scan_results_.end();
         ++it) {
      if (it->second.selected) {
        bool changed = preferences_->SetGalleryPermissionForExtension(
              *extension_, it->first, true);
        DCHECK(changed);
      }
    }
    for (MediaGalleryPrefIdSet::const_iterator it = results_to_remove_.begin();
        it != results_to_remove_.end();
        ++it) {
      preferences_->ForgetGalleryById(*it);
    }
  }

  on_finish_.Run();
  delete this;
}

content::WebContents* MediaGalleriesScanResultDialogController::web_contents() {
  return web_contents_;
}

void MediaGalleriesScanResultDialogController::OnPreferencesInitialized() {
  preferences_->AddGalleryChangeObserver(this);
  StorageMonitor::GetInstance()->AddObserver(this);
  UpdateFromPreferences();

  // TODO(vandebo): Remove the conditional after the cocoa version is done.
  if (!create_dialog_callback_.is_null())
    dialog_.reset(create_dialog_callback_.Run(this));
}

void MediaGalleriesScanResultDialogController::UpdateFromPreferences() {
  const MediaGalleriesPrefInfoMap& galleries = preferences_->known_galleries();
  MediaGalleryPrefIdSet permitted =
      preferences_->GalleriesForExtension(*extension_);

  // Add or update any scan results that the extension doesn't already have
  // access to or isn't in |results_to_remove_|.
  for (MediaGalleriesPrefInfoMap::const_iterator it = galleries.begin();
       it != galleries.end();
       ++it) {
    const MediaGalleryPrefInfo& gallery = it->second;
    if (gallery.type == MediaGalleryPrefInfo::kScanResult &&
        !ContainsKey(permitted, gallery.pref_id) &&
        !ContainsKey(results_to_remove_, gallery.pref_id)) {
      ScanResults::iterator existing = scan_results_.find(gallery.pref_id);
      if (existing == scan_results_.end()) {
        // Default to selected.
        scan_results_[gallery.pref_id] = ScanResult(gallery, true);
      } else {
        // Update pref_info, in case anything has been updated.
        existing->second.pref_info = gallery;
      }
    }
  }

  // Remove anything from |scan_results_| that's no longer valid or the user
  // already has access to.
  std::list<ScanResults::iterator> to_remove;
  for (ScanResults::iterator it = scan_results_.begin();
       it != scan_results_.end();
       ++it) {
    MediaGalleriesPrefInfoMap::const_iterator pref_gallery =
        galleries.find(it->first);
    if (pref_gallery == galleries.end() ||
        pref_gallery->second.type != MediaGalleryPrefInfo::kScanResult ||
        permitted.find(it->first) != permitted.end()) {
      to_remove.push_back(it);
    }
  }
  while (!to_remove.empty()) {
    scan_results_.erase(to_remove.front());
    to_remove.pop_front();
  }
}

void MediaGalleriesScanResultDialogController::OnPreferenceUpdate(
    const std::string& extension_id, MediaGalleryPrefId pref_id) {
  if (extension_id == extension_->id()) {
    const MediaGalleriesPrefInfoMap::const_iterator it =
        preferences_->known_galleries().find(pref_id);
    if (it == preferences_->known_galleries().end() ||
        it->second.type == MediaGalleryPrefInfo::kScanResult ||
        it->second.type == MediaGalleryPrefInfo::kRemovedScan) {
      UpdateFromPreferences();
      dialog_->UpdateResults();
    }
  }
}

void MediaGalleriesScanResultDialogController::OnRemovableDeviceUpdate(
    const std::string device_id) {
  for (ScanResults::const_iterator it = scan_results_.begin();
       it != scan_results_.end();
       ++it) {
    if (it->second.pref_info.device_id == device_id) {
      dialog_->UpdateResults();
      return;
    }
  }
}

Profile* MediaGalleriesScanResultDialogController::GetProfile() const {
  return Profile::FromBrowserContext(web_contents_->GetBrowserContext());
}

void MediaGalleriesScanResultDialogController::OnRemovableStorageAttached(
    const StorageInfo& info) {
  OnRemovableDeviceUpdate(info.device_id());
}

void MediaGalleriesScanResultDialogController::OnRemovableStorageDetached(
    const StorageInfo& info) {
  OnRemovableDeviceUpdate(info.device_id());
}

void MediaGalleriesScanResultDialogController::OnPermissionAdded(
    MediaGalleriesPreferences* /* pref */,
    const std::string& extension_id,
    MediaGalleryPrefId pref_id) {
  OnPreferenceUpdate(extension_id, pref_id);
}

void MediaGalleriesScanResultDialogController::OnPermissionRemoved(
    MediaGalleriesPreferences* /* pref */,
    const std::string& extension_id,
    MediaGalleryPrefId pref_id) {
  OnPreferenceUpdate(extension_id, pref_id);
}

void MediaGalleriesScanResultDialogController::OnGalleryAdded(
    MediaGalleriesPreferences* /* prefs */,
    MediaGalleryPrefId pref_id) {
  OnPreferenceUpdate(extension_->id(), pref_id);
}

void MediaGalleriesScanResultDialogController::OnGalleryRemoved(
    MediaGalleriesPreferences* /* prefs */,
    MediaGalleryPrefId pref_id) {
  OnPreferenceUpdate(extension_->id(), pref_id);
}

void MediaGalleriesScanResultDialogController::OnGalleryInfoUpdated(
    MediaGalleriesPreferences* /* prefs */,
    MediaGalleryPrefId pref_id) {
  OnPreferenceUpdate(extension_->id(), pref_id);
}

// MediaGalleriesScanResultDialog ---------------------------------------------

MediaGalleriesScanResultDialog::~MediaGalleriesScanResultDialog() {}
