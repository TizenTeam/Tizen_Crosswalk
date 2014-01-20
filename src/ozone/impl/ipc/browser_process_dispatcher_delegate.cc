// Copyright 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ozone/impl/ipc/browser_process_dispatcher_delegate.h"

#include "base/bind.h"
#include "ozone/wayland/input/kbd_conversion.h"
#include "ozone/wayland/window_change_observer.h"

namespace ozonewayland {

BrowserProcessDispatcherDelegate::BrowserProcessDispatcherDelegate()
    : WaylandDispatcherDelegate(),
      observer_(NULL) {
}

BrowserProcessDispatcherDelegate::~BrowserProcessDispatcherDelegate() {
}

void BrowserProcessDispatcherDelegate::MotionNotify(float x, float y) {
  gfx::Point position = gfx::Point(x, y);
  scoped_ptr<ui::MouseEvent> mouseev(new ui::MouseEvent(ui::ET_MOUSE_MOVED,
                                                        position,
                                                        position,
                                                        0));

  PostTaskOnMainLoop(base::Bind(
      &BrowserProcessDispatcherDelegate::DispatchEventHelper, base::Passed(
          mouseev.PassAs<ui::Event>())));
}

void BrowserProcessDispatcherDelegate::ButtonNotify(unsigned handle,
                                                    int state,
                                                    int flags,
                                                    float x,
                                                    float y) {
  ui::EventType type;
  if (state == 1)
    type = ui::ET_MOUSE_PRESSED;
  else
    type = ui::ET_MOUSE_RELEASED;

  gfx::Point position = gfx::Point(x, y);
  scoped_ptr<ui::MouseEvent> mouseev(new ui::MouseEvent(type,
                                                        position,
                                                        position,
                                                        flags));
  PostTaskOnMainLoop(base::Bind(
      &BrowserProcessDispatcherDelegate::NotifyButtonPress, this, handle));

  PostTaskOnMainLoop(base::Bind(
      &BrowserProcessDispatcherDelegate::DispatchEventHelper, base::Passed(
          mouseev.PassAs<ui::Event>())));
}

void BrowserProcessDispatcherDelegate::AxisNotify(float x,
                                                  float y,
                                                  float xoffset,
                                                  float yoffset) {
  gfx::Point position = gfx::Point(x, y);
  ui::MouseEvent mouseev(ui::ET_MOUSEWHEEL, position, position, 0);

  scoped_ptr<ui::MouseWheelEvent> wheelev(new ui::MouseWheelEvent(mouseev,
                                                                  xoffset,
                                                                  yoffset));

  PostTaskOnMainLoop(base::Bind(
      &BrowserProcessDispatcherDelegate::DispatchEventHelper, base::Passed(
          wheelev.PassAs<ui::Event>())));
}

void BrowserProcessDispatcherDelegate::PointerEnter(unsigned handle,
                                                    float x,
                                                    float y) {
  gfx::Point position = gfx::Point(x, y);
  scoped_ptr<ui::MouseEvent> mouseev(new ui::MouseEvent(ui::ET_MOUSE_ENTERED,
                                                        position,
                                                        position,
                                                        handle));
  PostTaskOnMainLoop(base::Bind(
      &BrowserProcessDispatcherDelegate::NotifyPointerEnter, this, handle));

  PostTaskOnMainLoop(base::Bind(
      &BrowserProcessDispatcherDelegate::DispatchEventHelper, base::Passed(
          mouseev.PassAs<ui::Event>())));
}

void BrowserProcessDispatcherDelegate::PointerLeave(unsigned handle,
                                                    float x,
                                                    float y) {
  gfx::Point position = gfx::Point(x, y);
  scoped_ptr<ui::MouseEvent> mouseev(new ui::MouseEvent(ui::ET_MOUSE_EXITED,
                                                        position,
                                                        position,
                                                        0));

  PostTaskOnMainLoop(base::Bind(
      &BrowserProcessDispatcherDelegate::NotifyPointerLeave, this, handle));

  PostTaskOnMainLoop(base::Bind(
      &BrowserProcessDispatcherDelegate::DispatchEventHelper, base::Passed(
          mouseev.PassAs<ui::Event>())));
}

void BrowserProcessDispatcherDelegate::KeyNotify(unsigned state,
                                                 unsigned code,
                                                 unsigned modifiers) {
  ui::EventType type;
  if (state)
    type = ui::ET_KEY_PRESSED;
  else
    type = ui::ET_KEY_RELEASED;

  scoped_ptr<ui::KeyEvent> keyev(new ui::KeyEvent(type,
                                                  KeyboardCodeFromXKeysym(code),
                                                  modifiers,
                                                  true));

  PostTaskOnMainLoop(base::Bind(
      &BrowserProcessDispatcherDelegate::DispatchEventHelper, base::Passed(
          keyev.PassAs<ui::Event>())));
}

void BrowserProcessDispatcherDelegate::SetWindowChangeObserver(
    WindowChangeObserver* observer) {
  observer_ = observer;
}

void BrowserProcessDispatcherDelegate::NotifyPointerEnter(
    BrowserProcessDispatcherDelegate* data, unsigned handle) {
  if (data->observer_)
    data->observer_->OnWindowEnter(handle);
}

void BrowserProcessDispatcherDelegate::NotifyPointerLeave(
    BrowserProcessDispatcherDelegate* data, unsigned handle) {
  if (data->observer_)
    data->observer_->OnWindowLeave(handle);
}


void BrowserProcessDispatcherDelegate::NotifyButtonPress(
    BrowserProcessDispatcherDelegate* data, unsigned handle) {
  if (data->observer_)
    data->observer_->OnWindowFocused(handle);
}

void BrowserProcessDispatcherDelegate::DispatchEventHelper(
    scoped_ptr<ui::Event> key) {
  base::MessagePumpOzone::Current()->Dispatch(key.get());
}

}  // namespace ozonewayland
