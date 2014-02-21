// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for interacting with frames.
// Multiply-included message file, hence no include guard.

#include "content/common/content_export.h"
#include "content/common/frame_param.h"
#include "content/public/common/common_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "url/gurl.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START FrameMsgStart

IPC_STRUCT_BEGIN(FrameHostMsg_DidFailProvisionalLoadWithError_Params)
  // The frame ID for the failure report.
  IPC_STRUCT_MEMBER(int64, frame_id)
  // The WebFrame's uniqueName().
  IPC_STRUCT_MEMBER(base::string16, frame_unique_name)
  // True if this is the top-most frame.
  IPC_STRUCT_MEMBER(bool, is_main_frame)
  // Error code as reported in the DidFailProvisionalLoad callback.
  IPC_STRUCT_MEMBER(int, error_code)
  // An error message generated from the error_code. This can be an empty
  // string if we were unable to find a meaningful description.
  IPC_STRUCT_MEMBER(base::string16, error_description)
  // The URL that the error is reported for.
  IPC_STRUCT_MEMBER(GURL, url)
  // True if the failure is the result of navigating to a POST again
  // and we're going to show the POST interstitial.
  IPC_STRUCT_MEMBER(bool, showing_repost_interstitial)
IPC_STRUCT_END()

// -----------------------------------------------------------------------------
// Messages sent from the browser to the renderer.

// When HW accelerated buffers are swapped in an out-of-process child frame
// renderer, the message is forwarded to the embedding frame to notify it of
// a new texture available for compositing. When the buffer has finished
// presenting, a FrameHostMsg_BuffersSwappedACK should be sent back to
// gpu host that produced this buffer.
//
// This is used in the non-ubercomp HW accelerated compositing path.
IPC_MESSAGE_ROUTED1(FrameMsg_BuffersSwapped,
                    FrameMsg_BuffersSwapped_Params /* params */)

// Notifies the embedding frame that a new CompositorFrame is ready to be
// presented. When the frame finishes presenting, a matching
// FrameHostMsg_CompositorFrameSwappedACK should be sent back to the
// RenderViewHost that was produced the CompositorFrame.
//
// This is used in the ubercomp compositing path.
IPC_MESSAGE_ROUTED1(FrameMsg_CompositorFrameSwapped,
                    FrameMsg_CompositorFrameSwapped_Params /* params */)

// -----------------------------------------------------------------------------
// Messages sent from the renderer to the browser.

// Sent by the renderer when a child frame is created in the renderer. The
// |parent_frame_id| and |frame_id| are NOT routing ids. They are
// renderer-allocated identifiers used for tracking a frame's creation.
//
// Each of these messages will have a corresponding FrameHostMsg_Detach message
// sent when the frame is detached from the DOM.
//
// TOOD(ajwong): replace parent_render_frame_id and frame_id with just the
// routing ids.
IPC_SYNC_MESSAGE_CONTROL4_1(FrameHostMsg_CreateChildFrame,
                            int32 /* parent_render_frame_id */,
                            int64 /* parent_frame_id */,
                            int64 /* frame_id */,
                            std::string /* frame_name */,
                            int /* new_render_frame_id */)

// Sent by the renderer to the parent RenderFrameHost when a child frame is
// detached from the DOM.
IPC_MESSAGE_ROUTED2(FrameHostMsg_Detach,
                    int64 /* parent_frame_id */,
                    int64 /* frame_id */)

// Sent when the renderer starts a provisional load for a frame.
IPC_MESSAGE_ROUTED4(FrameHostMsg_DidStartProvisionalLoadForFrame,
                    int64 /* frame_id */,
                    int64 /* parent_frame_id */,
                    bool /* true if it is the main frame */,
                    GURL /* url */)

// Sent when the renderer fails a provisional load with an error.
IPC_MESSAGE_ROUTED1(FrameHostMsg_DidFailProvisionalLoadWithError,
                    FrameHostMsg_DidFailProvisionalLoadWithError_Params)

// Sent when a provisional load on the main frame redirects.
IPC_MESSAGE_ROUTED3(FrameHostMsg_DidRedirectProvisionalLoad,
                    int /* page_id */,
                    GURL /* source_url*/,
                    GURL /* target_url */)

// Sent to the browser when the renderer detects it is blocked on a pepper
// plugin message for too long. This is also sent when it becomes unhung
// (according to the value of is_hung). The browser can give the user the
// option of killing the plugin.
IPC_MESSAGE_ROUTED3(FrameHostMsg_PepperPluginHung,
                    int /* plugin_child_id */,
                    base::FilePath /* path */,
                    bool /* is_hung */)

// Sent by the renderer process to indicate that a plugin instance has crashed.
// Note: |plugin_pid| should not be trusted. The corresponding process has
// probably died. Moreover, the ID may have been reused by a new process. Any
// usage other than displaying it in a prompt to the user is very likely to be
// wrong.
IPC_MESSAGE_ROUTED2(FrameHostMsg_PluginCrashed,
                    base::FilePath /* plugin_path */,
                    base::ProcessId /* plugin_pid */)

// Return information about a plugin for the given URL and MIME
// type. If there is no matching plugin, |found| is false.
// |actual_mime_type| is the actual mime type supported by the
// found plugin.
IPC_SYNC_MESSAGE_CONTROL4_3(FrameHostMsg_GetPluginInfo,
                            int /* render_frame_id */,
                            GURL /* url */,
                            GURL /* page_url */,
                            std::string /* mime_type */,
                            bool /* found */,
                            content::WebPluginInfo /* plugin info */,
                            std::string /* actual_mime_type */)

// A renderer sends this to the browser process when it wants to
// create a plugin.  The browser will create the plugin process if
// necessary, and will return a handle to the channel on success.
// On error an empty string is returned.
IPC_SYNC_MESSAGE_CONTROL4_2(FrameHostMsg_OpenChannelToPlugin,
                            int /* render_frame_id */,
                            GURL /* url */,
                            GURL /* page_url */,
                            std::string /* mime_type */,
                            IPC::ChannelHandle /* channel_handle */,
                            content::WebPluginInfo /* info */)

// Acknowledge that we presented a HW buffer and provide a sync point
// to specify the location in the command stream when the compositor
// is no longer using it.
//
// See FrameMsg_BuffersSwapped.
IPC_MESSAGE_ROUTED1(FrameHostMsg_BuffersSwappedACK,
                    FrameHostMsg_BuffersSwappedACK_Params /* params */)

// Acknowledge that we presented an ubercomp frame.
//
// See FrameMsg_CompositorFrameSwapped
IPC_MESSAGE_ROUTED1(FrameHostMsg_CompositorFrameSwappedACK,
                    FrameHostMsg_CompositorFrameSwappedACK_Params /* params */)

// Indicates that the current frame has swapped out, after a SwapOut message.
IPC_MESSAGE_ROUTED0(FrameHostMsg_SwapOut_ACK)

IPC_MESSAGE_ROUTED1(FrameHostMsg_ReclaimCompositorResources,
                    FrameHostMsg_ReclaimCompositorResources_Params /* params */)

// Instructs the frame to swap out for a cross-site transition, including
// running the unload event handler. Expects a SwapOut_ACK message when
// finished.
IPC_MESSAGE_ROUTED0(FrameMsg_SwapOut)
