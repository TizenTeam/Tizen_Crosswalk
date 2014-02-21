// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "chrome/browser/extensions/api/image_writer_private/destroy_partitions_operation.h"
#include "chrome/browser/extensions/api/image_writer_private/error_messages.h"

namespace extensions {
namespace image_writer {

// Number of bytes for the maximum partition table size.  By wiping this many
// bytes we can essentially guarantee the header and associated information will
// be wiped. See http://crbug.com/328246 for more information.
const int kPartitionTableSize = 1 * 1024;

DestroyPartitionsOperation::DestroyPartitionsOperation(
    base::WeakPtr<OperationManager> manager,
    const ExtensionId& extension_id,
    const std::string& storage_unit_id)
    : Operation(manager, extension_id, storage_unit_id) {
  verify_write_ = false;
}

DestroyPartitionsOperation::~DestroyPartitionsOperation() {}

void DestroyPartitionsOperation::Start() {
  if (!temp_dir_.CreateUniqueTempDir()) {
    Error(error::kTempDirError);
    return;
  }

  if (!base::CreateTemporaryFileInDir(temp_dir_.path(), &image_path_)) {
    Error(error::kTempFileError);
    return;
  }

  scoped_ptr<char[]> buffer(new char[kPartitionTableSize]);
  memset(buffer.get(), 0, kPartitionTableSize);

  if (file_util::WriteFile(image_path_, buffer.get(), kPartitionTableSize) !=
      kPartitionTableSize) {
    Error(error::kTempFileError);
    return;
  }

  WriteStart();
}

}  // namespace image_writer
}  // namespace extensions
