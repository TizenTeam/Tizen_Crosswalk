// Copyright 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ozone/wayland/dispatcher.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <wayland-client.h>

#include "base/bind.h"
#include "ozone/wayland/dispatcher_delegate.h"
#include "ozone/wayland/display.h"

namespace ozonewayland {
WaylandDispatcher* WaylandDispatcher::instance_ = NULL;
const int MAX_EVENTS = 16;

// os-compatibility
extern "C" {
int osEpollCreateCloExec(void);

static int setCloExecOrClose(int fd) {
  long flags;

  if (fd == -1)
    return -1;

  flags = fcntl(fd, F_GETFD);
  if (flags == -1)
    goto err;

  if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1)
    goto err;

  return fd;

  err:
    close(fd);
    return -1;
}

int osEpollCreateCloExec(void) {
  int fd;

#ifdef EPOLL_CLOEXEC
  fd = epoll_create1(EPOLL_CLOEXEC);
  if (fd >= 0)
    return fd;
  if (errno != EINVAL)
    return -1;
#endif

  fd = epoll_create(1);
  return setCloExecOrClose(fd);
}
}  // os-compatibility

void WaylandDispatcher::MotionNotify(float x, float y) {
  DCHECK(delegate_);
  delegate_->MotionNotify(x, y);
}

void WaylandDispatcher::ButtonNotify(unsigned handle,
                                     int state,
                                     int flags,
                                     float x,
                                     float y) {
  DCHECK(delegate_);
  delegate_->ButtonNotify(handle, state, flags, x, y);
}

void WaylandDispatcher::AxisNotify(float x,
                                   float y,
                                   float xoffset,
                                   float yoffset) {
  DCHECK(delegate_);
  delegate_->AxisNotify(x, y, xoffset, yoffset);
}

void WaylandDispatcher::PointerEnter(unsigned handle, float x, float y) {
  DCHECK(delegate_);
  delegate_->PointerEnter(handle, x, y);
}

void WaylandDispatcher::PointerLeave(unsigned handle, float x, float y) {
  DCHECK(delegate_);
  delegate_->PointerLeave(handle, x, y);
}

void WaylandDispatcher::KeyNotify(unsigned state,
                                  unsigned code,
                                  unsigned modifiers) {
  DCHECK(delegate_);
  delegate_->KeyNotify(state, code, modifiers);
}

void WaylandDispatcher::OutputSizeChanged(unsigned width, unsigned height) {
  DCHECK(delegate_);
  delegate_->OutputSizeChanged(width, height);
}

void WaylandDispatcher::PostTask(Task type) {
  if (!IsRunning() || !active_)
    return;

  DCHECK(delegate_);

  switch (type) {
    case(Flush):
      message_loop_proxy()->PostTask(
          FROM_HERE, base::Bind(&WaylandDispatcher::HandleFlush));
      break;
    case(Poll):
      DCHECK(epoll_fd_);
      message_loop_proxy()->PostTask(FROM_HERE, base::Bind(
          &WaylandDispatcher::DisplayRun, this));
  default:
    break;
  }
}

void WaylandDispatcher::SetWindowChangeObserver(
    WindowChangeObserver* observer) {
  DCHECK(delegate_);
  delegate_->SetWindowChangeObserver(observer);
}

void WaylandDispatcher::SetDelegate(WaylandDispatcherDelegate* delegate) {
  delegate_ = delegate;
  SetActive(true);
}

void WaylandDispatcher::SetActive(bool active) {
  DCHECK(delegate_);
  active_ = active;
  delegate_->SetActive(active);
}

WaylandDispatcher::WaylandDispatcher(int fd)
    : Thread("WaylandDispatcher"),
      active_(false),
      epoll_fd_(0),
      display_fd_(fd),
      delegate_(NULL) {
  instance_ = this;
  if (display_fd_) {
    epoll_fd_ = osEpollCreateCloExec();
    struct epoll_event ep;
    ep.events = EPOLLIN;
    ep.data.ptr = 0;
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, display_fd_, &ep);
  }

  Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  StartWithOptions(options);
  SetPriority(base::kThreadPriority_Background);
}

WaylandDispatcher::~WaylandDispatcher() {
  active_ = false;
  if (delegate_)
    delete delegate_;

  Stop();

  if (epoll_fd_) {
    close(epoll_fd_);
    epoll_fd_ = 0;
  }

  instance_ = NULL;
}

void WaylandDispatcher::HandleFlush() {
  wl_display* waylandDisp = WaylandDisplay::GetInstance()->display();

  while (wl_display_prepare_read(waylandDisp) != 0)
    wl_display_dispatch_pending(waylandDisp);

  wl_display_flush(waylandDisp);
  wl_display_read_events(waylandDisp);
  wl_display_dispatch_pending(waylandDisp);
}

void  WaylandDispatcher::DisplayRun(WaylandDispatcher* data) {
  struct epoll_event ep[MAX_EVENTS];
  int i, ret, count = 0;
  uint32_t event = 0;
  bool epoll_err = false;

  // Adopted from:
  // http://cgit.freedesktop.org/wayland/weston/tree/clients/window.c#n5531.
  while (1) {
    wl_display* waylandDisp = WaylandDisplay::GetInstance()->display();
    wl_display_dispatch_pending(waylandDisp);
    ret = wl_display_flush(waylandDisp);
    if (ret < 0 && errno == EAGAIN) {
      ep[0].events = EPOLLIN | EPOLLERR | EPOLLHUP;
      epoll_ctl(data->epoll_fd_, EPOLL_CTL_MOD, data->display_fd_, &ep[0]);
    } else if (ret < 0) {
      epoll_err = true;
      break;
    }

    // StopProcessingEvents has been called or we have been asked to stop
    // polling. Break from the loop.
    if (!data->active_)
      break;

    count = epoll_wait(data->epoll_fd_, ep, MAX_EVENTS, -1);
    // Break if epoll wait returned value less than 0 and we aren't interrupted
    // by a signal.
    if (count < 0 && errno != EINTR) {
      LOG(ERROR) << "epoll_wait returned an error." << errno;
      epoll_err = true;
      break;
    }

    for (i = 0; i < count; i++) {
      event = ep[i].events;
      // We can have cases where EPOLLIN and EPOLLHUP are both set for
      // example. Don't break if both flags are set.
      if ((event & EPOLLERR || event & EPOLLHUP) &&
             !(event & EPOLLIN)) {
        epoll_err = true;
        break;
      }

      if (event & EPOLLIN) {
        ret = wl_display_dispatch(waylandDisp);
        if (ret == -1) {
          LOG(ERROR) << "wl_display_dispatch failed with an error." << errno;
          epoll_err = true;
          break;
        }
      }
    }

    if (epoll_err)
      break;
  }
}

}  // namespace ozonewayland
