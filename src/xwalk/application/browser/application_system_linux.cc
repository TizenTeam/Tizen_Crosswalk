// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xwalk/application/browser/application_system_linux.h"

#include "dbus/bus.h"
#include "xwalk/application/browser/application_service_provider_linux.h"
#include "xwalk/dbus/dbus_manager.h"
#include "xwalk/runtime/browser/xwalk_runner.h"

namespace xwalk {
namespace application {

ApplicationSystemLinux::ApplicationSystemLinux(RuntimeContext* runtime_context)
    : ApplicationSystem(runtime_context) {
  if (XWalkRunner::GetInstance()->is_running_as_service()) {
    service_provider_.reset(
        new ApplicationServiceProviderLinux(application_service(),
                                            application_storage(),
                                            dbus_manager().session_bus()));
  }
}

ApplicationSystemLinux::~ApplicationSystemLinux() {}

DBusManager& ApplicationSystemLinux::dbus_manager() {
  if (!dbus_manager_)
    dbus_manager_.reset(new DBusManager());
  return *dbus_manager_.get();
}

}  // namespace application
}  // namespace xwalk
