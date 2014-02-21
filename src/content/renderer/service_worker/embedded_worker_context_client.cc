// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/embedded_worker_context_client.h"

#include "base/lazy_instance.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/pickle.h"
#include "base/threading/thread_local.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/service_worker/embedded_worker_dispatcher.h"
#include "content/renderer/service_worker/service_worker_script_context.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "webkit/child/worker_task_runner.h"

using webkit_glue::WorkerTaskRunner;

namespace content {

namespace {

// For now client must be a per-thread instance.
// TODO(kinuko): This needs to be refactored when we start using thread pool
// or having multiple clients per one thread.
base::LazyInstance<base::ThreadLocalPointer<EmbeddedWorkerContextClient> >::
    Leaky g_worker_client_tls = LAZY_INSTANCE_INITIALIZER;

void CallWorkerContextDestroyedOnMainThread(int embedded_worker_id) {
  if (!RenderThreadImpl::current() ||
      !RenderThreadImpl::current()->embedded_worker_dispatcher())
    return;
  RenderThreadImpl::current()->embedded_worker_dispatcher()->
      WorkerContextDestroyed(embedded_worker_id);
}

}  // namespace

EmbeddedWorkerContextClient*
EmbeddedWorkerContextClient::ThreadSpecificInstance() {
  return g_worker_client_tls.Pointer()->Get();
}

EmbeddedWorkerContextClient::EmbeddedWorkerContextClient(
    int embedded_worker_id,
    int64 service_worker_version_id,
    const GURL& script_url)
    : embedded_worker_id_(embedded_worker_id),
      service_worker_version_id_(service_worker_version_id),
      script_url_(script_url),
      sender_(ChildThread::current()->thread_safe_sender()),
      main_thread_proxy_(base::MessageLoopProxy::current()) {
  g_worker_client_tls.Pointer()->Set(this);
}

EmbeddedWorkerContextClient::~EmbeddedWorkerContextClient() {
  DCHECK(g_worker_client_tls.Pointer()->Get() != NULL);
  g_worker_client_tls.Pointer()->Set(NULL);
}

bool EmbeddedWorkerContextClient::OnMessageReceived(
    const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(EmbeddedWorkerContextClient, msg)
    IPC_MESSAGE_HANDLER(EmbeddedWorkerContextMsg_SendMessageToWorker,
                        OnSendMessageToWorker)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void EmbeddedWorkerContextClient::SendMessageToBrowser(
    const IPC::Message& message) {
  sender_->Send(new EmbeddedWorkerHostMsg_SendMessageToBrowser(
      embedded_worker_id_, message));
}

void EmbeddedWorkerContextClient::workerContextFailedToStart() {
  DCHECK(main_thread_proxy_->RunsTasksOnCurrentThread());
  DCHECK(!script_context_);

  RenderThreadImpl::current()->embedded_worker_dispatcher()->
      WorkerContextDestroyed(embedded_worker_id_);
}

void EmbeddedWorkerContextClient::workerContextStarted(
    blink::WebServiceWorkerContextProxy* proxy) {
  DCHECK_NE(0, WorkerTaskRunner::Instance()->CurrentWorkerId());
  DCHECK(g_worker_client_tls.Pointer()->Get() == NULL);
  DCHECK(!script_context_);
  g_worker_client_tls.Pointer()->Set(this);
  script_context_.reset(new ServiceWorkerScriptContext(this, proxy));

  sender_->Send(new EmbeddedWorkerHostMsg_WorkerStarted(
      WorkerTaskRunner::Instance()->CurrentWorkerId(),
      embedded_worker_id_));
}

void EmbeddedWorkerContextClient::workerContextDestroyed() {
  // At this point OnWorkerRunLoopStopped is already called, so
  // CurrentWorkerId() returns 0 (while we're still on the worker thread).
  script_context_.reset();
  main_thread_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&CallWorkerContextDestroyedOnMainThread,
                 embedded_worker_id_));
}

void EmbeddedWorkerContextClient::OnSendMessageToWorker(
    int thread_id,
    int embedded_worker_id,
    const IPC::Message& message) {
  if (!script_context_)
    return;
  DCHECK_EQ(embedded_worker_id_, embedded_worker_id);
  script_context_->OnMessageReceived(message);
}

}  // namespace content
