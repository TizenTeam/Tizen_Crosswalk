// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_GALLERIES_SCAN_RESULT_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_GALLERIES_SCAN_RESULT_DIALOG_CONTROLLER_H_

#include <map>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/media_galleries/media_galleries_preferences.h"
#include "chrome/browser/storage_monitor/removable_storage_observer.h"

namespace content {
class WebContents;
}

namespace extensions {
class Extension;
}

class MediaGalleriesScanResultDialogController;
class Profile;

// The view.
class MediaGalleriesScanResultDialog {
 public:
  virtual ~MediaGalleriesScanResultDialog();

  // Tell the dialog to update its display list of scan results.
  virtual void UpdateResults() = 0;

  // Constructs a platform-specific dialog owned and controlled by |controller|.
  static MediaGalleriesScanResultDialog* Create(
      MediaGalleriesScanResultDialogController* controller);
};

// The controller is responsible for handling the logic of the dialog and
// interfacing with the model (i.e., MediaGalleriesPreferences). It shows
// the dialog and owns itself.
class MediaGalleriesScanResultDialogController
    : public RemovableStorageObserver,
      public MediaGalleriesPreferences::GalleryChangeObserver {
 public:
  struct ScanResult {
    ScanResult(const MediaGalleryPrefInfo& pref_info, bool selected)
        : pref_info(pref_info),
          selected(selected) {
    }
    ScanResult() {}

    MediaGalleryPrefInfo pref_info;
    bool selected;
  };
  typedef std::vector<ScanResult> OrderedScanResults;

  // The constructor creates a dialog controller which owns itself.
  MediaGalleriesScanResultDialogController(
      content::WebContents* web_contents,
      const extensions::Extension& extension,
      const base::Closure& on_finish);

  // The title of the dialog view.
  base::string16 GetHeader() const;

  // Explanatory text directly below the title.
  base::string16 GetSubtext() const;

  // Get the scan results and their current selection state.
  virtual OrderedScanResults GetGalleryList() const;

  // A checkbox beside a scan result was toggled.
  virtual void DidToggleGalleryId(MediaGalleryPrefId pref_id, bool selected);

  // A folder viewer icon was clicked.
  virtual void DidClickOpenFolderViewer(MediaGalleryPrefId pref_id) const;

  // The forget command in the context menu was selected.
  virtual void DidForgetGallery(MediaGalleryPrefId pref_id);

  // The dialog is being deleted.
  virtual void DialogFinished(bool accepted);

  virtual content::WebContents* web_contents();

 private:
  friend class MediaGalleriesScanResultDialogControllerTest;

  typedef std::map<MediaGalleryPrefId, ScanResult> ScanResults;
  typedef base::Callback<MediaGalleriesScanResultDialog* (
      MediaGalleriesScanResultDialogController*)> CreateDialogCallback;

  // Used for unit tests.
  MediaGalleriesScanResultDialogController(
      const extensions::Extension& extension,
      MediaGalleriesPreferences* preferences_,
      const CreateDialogCallback& create_dialog_callback,
      const base::Closure& on_finish);

  virtual ~MediaGalleriesScanResultDialogController();

  // Bottom half of constructor -- called when |preferences_| is initialized.
  void OnPreferencesInitialized();

  // Update the controller state from preferences.
  void UpdateFromPreferences();

  // Used to keep the dialog in sync with the preferences.
  void OnPreferenceUpdate(const std::string& extension_id,
                          MediaGalleryPrefId pref_id);

  // Used to keep the dialog in sync with attached and detached devices.
  void OnRemovableDeviceUpdate(const std::string device_id);

  Profile* GetProfile() const;

  // RemovableStorageObserver implementation.
  // Used to keep dialog in sync with removable device status.
  virtual void OnRemovableStorageAttached(const StorageInfo& info) OVERRIDE;
  virtual void OnRemovableStorageDetached(const StorageInfo& info) OVERRIDE;

  // MediaGalleriesPreferences::GalleryChangeObserver implementations.
  // Used to keep the dialog in sync when the preferences change.
  virtual void OnPermissionAdded(MediaGalleriesPreferences* pref,
                                 const std::string& extension_id,
                                 MediaGalleryPrefId pref_id) OVERRIDE;
  virtual void OnPermissionRemoved(MediaGalleriesPreferences* pref,
                                   const std::string& extension_id,
                                   MediaGalleryPrefId pref_id) OVERRIDE;
  virtual void OnGalleryAdded(MediaGalleriesPreferences* pref,
                              MediaGalleryPrefId pref_id) OVERRIDE;
  virtual void OnGalleryRemoved(MediaGalleriesPreferences* pref,
                                MediaGalleryPrefId pref_id) OVERRIDE;
  virtual void OnGalleryInfoUpdated(MediaGalleriesPreferences* pref,
                                    MediaGalleryPrefId pref_id) OVERRIDE;

  // The web contents from which the request originated.
  content::WebContents* web_contents_;

  // This is just a reference, but it's assumed that it won't become invalid
  // while the dialog is showing.
  const extensions::Extension* extension_;

  // The scan results that aren't blacklisted and this extension doesn't
  // already have access to.
  ScanResults scan_results_;

  // The set of scan results which should be removed (blacklisted) - unless
  // the user clicks Cancel.
  MediaGalleryPrefIdSet results_to_remove_;

  // Callback to run when the dialog closes.
  base::Closure on_finish_;

  // The model that tracks galleries and extensions' permissions.
  // This is the authoritative source for gallery information.
  MediaGalleriesPreferences* preferences_;

  // Creates the dialog. Only changed for unit tests.
  CreateDialogCallback create_dialog_callback_;

  // The view that's showing.
  scoped_ptr<MediaGalleriesScanResultDialog> dialog_;

  DISALLOW_COPY_AND_ASSIGN(MediaGalleriesScanResultDialogController);
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_GALLERIES_SCAN_RESULT_DIALOG_CONTROLLER_H_
