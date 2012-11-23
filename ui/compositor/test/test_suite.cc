// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/test_suite.h"

#include "base/message_loop.h"
#include "ui/base/ui_base_paths.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/gfx_paths.h"
#include "ui/gl/gl_implementation.h"

namespace ui {
namespace test {

CompositorTestSuite::CompositorTestSuite(int argc, char** argv)
    : TestSuite(argc, argv) {}

CompositorTestSuite::~CompositorTestSuite() {}

void CompositorTestSuite::Initialize() {
#if defined(OS_LINUX)
  gfx::InitializeGLBindings(gfx::kGLImplementationOSMesaGL);
#endif
  base::TestSuite::Initialize();

  gfx::RegisterPathProvider();

  message_loop_.reset(new MessageLoop(MessageLoop::TYPE_UI));
  Compositor::Initialize(false);
}

void CompositorTestSuite::Shutdown() {
  Compositor::Terminate();
  message_loop_.reset();

  base::TestSuite::Shutdown();
}

}  // namespace test
}  // namespace ui
