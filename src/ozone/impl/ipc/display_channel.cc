// Copyright 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ozone/impl/ipc/display_channel.h"

#include "content/child/child_process.h"
#include "content/child/child_thread.h"
#include "ozone/impl/ipc/messages.h"
#include "ozone/impl/ozone_display.h"
#include "ozone/ui/events/window_state_change_handler.h"

namespace ozonewayland {
// GpuChannelManager generates unique routeid for every new
// ImageTransportSurface. In Ozone-Wayland, we register a routeid between
// DisplayChannel and ChannelHost. Therefore, we hardcore our own routeid with a
// unique negitive value to avoid any conflicts from the GpuChannelManager ones.
#define WAYLAND_ROUTE_ID -0x1

namespace {

content::ChildThread* GetProcessMainThread() {
  content::ChildProcess* process = content::ChildProcess::current();
  DCHECK(process && process->main_thread());
  return process->main_thread();
}

}

OzoneDisplayChannel::OzoneDisplayChannel() {
}

OzoneDisplayChannel::~OzoneDisplayChannel() {
  content::ChildThread* thread = GetProcessMainThread();
  thread->RemoveRoute(WAYLAND_ROUTE_ID);
}

bool OzoneDisplayChannel::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(OzoneDisplayChannel, message)
  IPC_MESSAGE_HANDLER(WaylandWindow_State, OnWidgetStateChanged)
  IPC_MESSAGE_HANDLER(WaylandWindow_Attributes, OnWidgetAttributesChanged)
  IPC_MESSAGE_HANDLER(WaylandWindow_Title, OnWidgetTitleChanged)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void OzoneDisplayChannel::Register() {
  content::ChildThread* thread = GetProcessMainThread();
  thread->AddRoute(WAYLAND_ROUTE_ID, this);
}

void OzoneDisplayChannel::OnWidgetStateChanged(unsigned handleid,
                                               WidgetState state,
                                               unsigned width,
                                               unsigned height) {
  WindowStateChangeHandler::GetInstance()->SetWidgetState(handleid,
                                                          state,
                                                          width,
                                                          height);
}

void OzoneDisplayChannel::OnWidgetTitleChanged(unsigned widget,
                                               base::string16 title) {
  WindowStateChangeHandler::GetInstance()->SetWidgetTitle(widget, title);
}

void OzoneDisplayChannel::OnWidgetAttributesChanged(unsigned widget,
                                                    unsigned parent,
                                                    unsigned x,
                                                    unsigned y,
                                                    WidgetType type) {
  WindowStateChangeHandler::GetInstance()->SetWidgetAttributes(widget,
                                                               parent,
                                                               x,
                                                               y,
                                                               type);
}

}  // namespace ozonewayland
