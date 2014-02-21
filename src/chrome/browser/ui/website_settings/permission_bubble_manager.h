// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBSITE_SETTINGS_PERMISSION_BUBBLE_MANAGER_H_
#define CHROME_BROWSER_UI_WEBSITE_SETTINGS_PERMISSION_BUBBLE_MANAGER_H_

#include <vector>

#include "chrome/browser/ui/website_settings/permission_bubble_view.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class PermissionBubbleDelegate;

// Provides access to permissions bubbles. Allows clients to add a delegate
// callback interface to the existing permission bubble configuration.
// Depending on the situation and policy, that may add new UI to an existing
// permission bubble, create and show a new permission bubble, or provide no
// visible UI action at all. (In that case, the delegate will be immediately
// informed that the permission request failed.)
//
// A PermissionBubbleManager is associated with a particular WebContents.
// Delegates attached to a particular WebContents' PBM must outlive it.
//
// The PermissionBubbleManager should be addressed on the UI thread.
class PermissionBubbleManager
    : public content::WebContentsObserver,
      public content::WebContentsUserData<PermissionBubbleManager>,
      public PermissionBubbleView::Delegate {
 public:
  // Return the flag-driven enabled state of permissions bubbles.
  static bool Enabled();

  virtual ~PermissionBubbleManager();

  // Add a new consumer delegate to the permission bubble. Ownership of the
  // delegate remains with the caller. The caller must arrange for the delegate
  // to outlive the PermissionBubbleManager.
  virtual void AddPermissionBubbleDelegate(PermissionBubbleDelegate* delegate);

  // Remove a consumer delegate from the permission bubble.
  virtual void RemovePermissionBubbleDelegate(
      PermissionBubbleDelegate* delegate);

  // Set the active view for the permission bubble. If this is NULL, it
  // means the permission bubble is no longer showing.
  virtual void SetView(PermissionBubbleView* view);

 private:
  friend class PermissionBubbleManagerTest;
  friend class content::WebContentsUserData<PermissionBubbleManager>;

  explicit PermissionBubbleManager(content::WebContents* web_contents);

  // contents::WebContentsObserver:
  virtual void WebContentsDestroyed(
      content::WebContents* web_contents) OVERRIDE;

  // PermissionBubbleView::Delegate:
  virtual void ToggleAccept(int delegate_index, bool new_value) OVERRIDE;
  virtual void SetCustomizationMode() OVERRIDE;
  virtual void Accept() OVERRIDE;
  virtual void Deny() OVERRIDE;
  virtual void Closing() OVERRIDE;

  // Finalize the pending permissions request.
  void FinalizeBubble();

  // Whether or not we are showing the bubble in this tab.
  bool bubble_showing_;

  // Set to the UI surface to be used to display the permissions requests.
  PermissionBubbleView* view_;

  std::vector<PermissionBubbleDelegate*> delegates_;
  std::vector<bool> accept_state_;
  bool customization_mode_;
};

#endif  // CHROME_BROWSER_UI_WEBSITE_SETTINGS_PERMISSION_BUBBLE_MANAGER_H_
