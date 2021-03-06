// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/platform_file.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "chrome/browser/media/webrtc_log_uploader.h"
#include "testing/gtest/include/gtest/gtest.h"

const char kTestTime[] = "time";
const char kTestReportId[] = "report-id";
const char kTestLocalId[] = "local-id";

class WebRtcLogUploaderTest : public testing::Test {
 public:
  WebRtcLogUploaderTest() {}

  bool VerifyNumberOfLines(int expected_lines) {
    std::vector<std::string> lines = GetLinesFromListFile();
    EXPECT_EQ(expected_lines, static_cast<int>(lines.size()));
    return expected_lines == static_cast<int>(lines.size());
  }

  bool VerifyLastLineHasAllInfo() {
    std::string last_line = GetLastLineFromListFile();
    if (last_line.empty())
      return false;
    std::vector<std::string> line_parts;
    base::SplitString(last_line, ',', &line_parts);
    EXPECT_EQ(3u, line_parts.size());
    if (3u != line_parts.size())
      return false;
    // The time (line_parts[0]) is the time when the info was written to the
    // file which we don't know, so just verify that it's not empty.
    EXPECT_FALSE(line_parts[0].empty());
    EXPECT_STREQ(kTestReportId, line_parts[1].c_str());
    EXPECT_STREQ(kTestLocalId, line_parts[2].c_str());
    return true;
  }

  bool VerifyLastLineHasLocalIdOnly() {
    std::string last_line = GetLastLineFromListFile();
    if (last_line.empty())
      return false;
    std::vector<std::string> line_parts;
    base::SplitString(last_line, ',', &line_parts);
    EXPECT_EQ(3u, line_parts.size());
    if (3u != line_parts.size())
      return false;
    EXPECT_TRUE(line_parts[0].empty());
    EXPECT_TRUE(line_parts[1].empty());
    EXPECT_STREQ(kTestLocalId, line_parts[2].c_str());
    return true;
  }

  bool VerifyLastLineHasUploadTimeAndIdOnly() {
    std::string last_line = GetLastLineFromListFile();
    if (last_line.empty())
      return false;
    std::vector<std::string> line_parts;
    base::SplitString(last_line, ',', &line_parts);
    EXPECT_EQ(3u, line_parts.size());
    if (3u != line_parts.size())
      return false;
    EXPECT_FALSE(line_parts[0].empty());
    EXPECT_STREQ(kTestReportId, line_parts[1].c_str());
    EXPECT_TRUE(line_parts[2].empty());
    return true;
  }

  bool AddLinesToTestFile(int number_of_lines) {
    int flags = base::PLATFORM_FILE_OPEN |
                base::PLATFORM_FILE_APPEND;
    base::PlatformFileError error = base::PLATFORM_FILE_OK;
    base::PlatformFile test_list_file =
        base::CreatePlatformFile(test_list_path_, flags, NULL, &error);
    EXPECT_EQ(base::PLATFORM_FILE_OK, error);
    EXPECT_NE(base::kInvalidPlatformFileValue, test_list_file);
    if (base::PLATFORM_FILE_OK != error ||
        base::kInvalidPlatformFileValue == test_list_file) {
      return false;
    }

    for (int i = 0; i < number_of_lines; ++i) {
      EXPECT_EQ(static_cast<int>(sizeof(kTestTime)) - 1,
                base::WritePlatformFileAtCurrentPos(test_list_file,
                                                    kTestTime,
                                                    sizeof(kTestTime) - 1));
      EXPECT_EQ(1, base::WritePlatformFileAtCurrentPos(test_list_file, ",", 1));
      EXPECT_EQ(static_cast<int>(sizeof(kTestReportId)) - 1,
                base::WritePlatformFileAtCurrentPos(test_list_file,
                                                    kTestReportId,
                                                    sizeof(kTestReportId) - 1));
      EXPECT_EQ(1, base::WritePlatformFileAtCurrentPos(test_list_file, ",", 1));
      EXPECT_EQ(static_cast<int>(sizeof(kTestLocalId)) - 1,
                base::WritePlatformFileAtCurrentPos(
                    test_list_file, kTestLocalId, sizeof(kTestLocalId) - 1));
      EXPECT_EQ(1, base::WritePlatformFileAtCurrentPos(test_list_file,
                                                       "\n", 1));
    }
    EXPECT_TRUE(base::ClosePlatformFile(test_list_file));

    return true;
  }

  std::vector<std::string> GetLinesFromListFile() {
    std::string contents;
    int read = base::ReadFileToString(test_list_path_, &contents);
    EXPECT_GT(read, 0);
    if (read == 0)
      return std::vector<std::string>();
    // Since every line should end with '\n', the last line should be empty. So
    // we expect at least two lines including the final empty. Remove the empty
    // line before returning.
    std::vector<std::string> lines;
    base::SplitString(contents, '\n', &lines);
    EXPECT_GT(lines.size(), 1u);
    if (lines.size() < 2)
      return std::vector<std::string>();
    EXPECT_TRUE(lines[lines.size() - 1].empty());
    if (!lines[lines.size() - 1].empty())
      return std::vector<std::string>();
    lines.pop_back();
    return lines;
  }

  std::string GetLastLineFromListFile() {
    std::vector<std::string> lines = GetLinesFromListFile();
    EXPECT_GT(lines.size(), 0u);
    if (lines.empty())
      return std::string();
    return lines[lines.size() - 1];
  }

  base::FilePath test_list_path_;
};

TEST_F(WebRtcLogUploaderTest, AddLocallyStoredLogInfoToUploadListFile) {
  // Get a temporary filename. We don't want the file to exist to begin with
  // since that's the normal use case, hence the delete.
  ASSERT_TRUE(base::CreateTemporaryFile(&test_list_path_));
  EXPECT_TRUE(base::DeleteFile(test_list_path_, false));
  scoped_ptr<WebRtcLogUploader> webrtc_log_uploader_(
      new WebRtcLogUploader());

  webrtc_log_uploader_->AddLocallyStoredLogInfoToUploadListFile(test_list_path_,
                                                                kTestLocalId);
  webrtc_log_uploader_->AddLocallyStoredLogInfoToUploadListFile(test_list_path_,
                                                                kTestLocalId);
  ASSERT_TRUE(VerifyNumberOfLines(2));
  ASSERT_TRUE(VerifyLastLineHasLocalIdOnly());

  const int expected_line_limit = 50;
  ASSERT_TRUE(AddLinesToTestFile(expected_line_limit - 2));
  ASSERT_TRUE(VerifyNumberOfLines(expected_line_limit));
  ASSERT_TRUE(VerifyLastLineHasAllInfo());

  webrtc_log_uploader_->AddLocallyStoredLogInfoToUploadListFile(test_list_path_,
                                                                kTestLocalId);
  ASSERT_TRUE(VerifyNumberOfLines(expected_line_limit));
  ASSERT_TRUE(VerifyLastLineHasLocalIdOnly());

  ASSERT_TRUE(AddLinesToTestFile(10));
  ASSERT_TRUE(VerifyNumberOfLines(60));
  ASSERT_TRUE(VerifyLastLineHasAllInfo());

  webrtc_log_uploader_->AddLocallyStoredLogInfoToUploadListFile(test_list_path_,
                                                                kTestLocalId);
  ASSERT_TRUE(VerifyNumberOfLines(expected_line_limit));
  ASSERT_TRUE(VerifyLastLineHasLocalIdOnly());

  webrtc_log_uploader_->StartShutdown();
}

TEST_F(WebRtcLogUploaderTest, AddUploadedLogInfoToUploadListFile) {
  // Get a temporary filename. We don't want the file to exist to begin with
  // since that's the normal use case, hence the delete.
  ASSERT_TRUE(base::CreateTemporaryFile(&test_list_path_));
  EXPECT_TRUE(base::DeleteFile(test_list_path_, false));
  scoped_ptr<WebRtcLogUploader> webrtc_log_uploader_(new WebRtcLogUploader());

  webrtc_log_uploader_->AddLocallyStoredLogInfoToUploadListFile(test_list_path_,
                                                                kTestLocalId);
  ASSERT_TRUE(VerifyNumberOfLines(1));
  ASSERT_TRUE(VerifyLastLineHasLocalIdOnly());

  webrtc_log_uploader_->AddUploadedLogInfoToUploadListFile(
      test_list_path_, kTestLocalId, kTestReportId);
  ASSERT_TRUE(VerifyNumberOfLines(1));
  ASSERT_TRUE(VerifyLastLineHasAllInfo());

  // Use a local ID that should not be found in the list.
  webrtc_log_uploader_->AddUploadedLogInfoToUploadListFile(
      test_list_path_, "dummy id", kTestReportId);
  ASSERT_TRUE(VerifyNumberOfLines(2));
  ASSERT_TRUE(VerifyLastLineHasUploadTimeAndIdOnly());

  webrtc_log_uploader_->StartShutdown();
}
