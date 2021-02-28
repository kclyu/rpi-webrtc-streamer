/*
Copyright (c) 2017, rpi-webrtc-streamer Lyu,KeunChang

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "raspi_motionfile.h"

#include <dirent.h>

#include <limits>
#include <list>
#include <string>

#include "config_motion.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/thread.h"
#include "utils.h"

namespace {

constexpr char kVideoFileExtension[] = ".h264";
constexpr char kImvFileExtension[] = ".imv";
// minimal wait period

struct FileInfo {
    std::string filename;
    size_t size;
};

static bool FilenameCompare(const FileInfo& a, const FileInfo& b) {
    return a.filename.compare(b.filename) < 0;
}

}  // namespace

bool LimitTotalDirSizeTask::Run() {
    std::list<FileInfo> file_list;
    std::string file_folder;
    size_t total_dir_size = 0;
    size_t file_counter = 0;

    // check video path is folder
    if (!utils::GetFolderWithTailingDelimiter(base_path_, file_folder)) {
        RTC_LOG(LS_ERROR) << "Motion file path is not folder : " << file_folder;
        return true;  // always return true to stop task
    };

    DIR* dirp = ::opendir(file_folder.c_str());
    if (dirp == nullptr) return true;
    for (struct dirent* dirent = ::readdir(dirp); dirent;
         dirent = ::readdir(dirp)) {
        std::string name = dirent->d_name;
        if (name.compare(0, prefix_.size(), prefix_) == 0) {
            FileInfo file_info;
            file_info.filename = name;
            file_info.size = utils::GetFileSize(file_folder + name).value_or(0);
            file_list.emplace_back(file_info);

            file_counter++;
            total_dir_size += file_info.size;
        }
    }
    // not needed it anymore
    ::closedir(dirp);
    file_list.sort(FilenameCompare);

    RTC_LOG(INFO) << "Directory \"" << file_folder << "\" " << file_counter
                  << " files, total size: " << total_dir_size;

    while (file_list.size() > 0 &&
           ((total_dir_size - file_list.front().size) > size_limit_)) {
        total_dir_size -= file_list.front().size;
        std::string file_full_path = file_folder + file_list.front().filename;
        RTC_LOG(INFO) << "Removing Video File :" << file_list.front().filename;
        utils::DeleteFile(file_full_path);
        file_list.pop_front();
    };
    return true;  // always return true to stop task
}

RaspiMotionFile::RaspiMotionFile(ConfigMotion* config_motion,
                                 const std::string base_path,
                                 const std::string prefix, int frame_queue_size,
                                 int motion_queue_size)
    : base_path_(base_path),
      prefix_(prefix),
      writer_active_(false),
      clock_(webrtc::Clock::GetRealTimeClock()),
      config_motion_(config_motion) {
    frame_writer_handle_.reset(
        new webrtc::FileWriterHandle("frame_writer", frame_queue_size));
    imv_writer_handle_.reset(
        new webrtc::FileWriterHandle("imv_writer", motion_queue_size));

    // Since the imv file is relatively small compared to the video file size,
    // the size limit is used only for the video file.
    frame_file_size_limit_ = config_motion_->GetFileSizeLimit() * 1024;
}

RaspiMotionFile::~RaspiMotionFile() {}

///////////////////////////////////////////////////////////////////////////////
//
// Queuing the video and imv frame in buffer queue
//
///////////////////////////////////////////////////////////////////////////////
bool RaspiMotionFile::FrameQueuing(const void* data, size_t bytes,
                                   size_t* bytes_written, bool is_keyframe) {
    // When saving a video file, the keyframe must be saved at the beginning, so
    // when the keyframe arrives, the buffer is cleared. Later, when the video
    // file starts to be saved, what is in the buffer is saved first, and
    // frames are added afterwards.
    if (is_keyframe == true) {
        // reset both of frame_queue and imv_queue
        if (frame_writer_handle_->is_open() == false)
            frame_writer_handle_->clear();
        if (imv_writer_handle_->is_open() == false) imv_writer_handle_->clear();
    }

    size_t written = frame_writer_handle_->WriteBack(data, bytes);
    if (written != bytes) {
        RTC_LOG(LS_ERROR) << "Failed to WriteBack on frame writer buffer";
        return false;
    }
    *bytes_written = written;
    return true;
}

bool RaspiMotionFile::ImvQueuing(const void* data, size_t bytes,
                                 size_t* bytes_written, bool is_keyframe) {
    size_t written = imv_writer_handle_->WriteBack(data, bytes);
    if (written != bytes) {
        RTC_LOG(LS_ERROR) << "Failed to WriteBack on imv writer buffer";
        return false;
    }
    *bytes_written = written;
    return true;
}

bool RaspiMotionFile::StartWriter() {
    if (writer_active_ == false) {
        // start motion file writer thread ;
        if (frame_writer_handle_->Open(base_path_, prefix_, kVideoFileExtension,
                                       frame_file_size_limit_) == false) {
            return false;
        };
        if (config_motion_->GetSaveImvFile()) {
            if (imv_writer_handle_->Open(base_path_, prefix_, kImvFileExtension,
                                         0 /* no limit */) == false) {
                return false;
            };
        }
        writer_active_ = true;
        RTC_LOG(LS_VERBOSE) << "Motion File Writer started.";
        return true;
    }
    RTC_LOG(LS_VERBOSE) << "Motion File Writer thread already started!";
    return false;
}

bool RaspiMotionFile::StopWriter() {
    if (writer_active_ == true) {
        writer_active_ = false;
        RTC_LOG(INFO) << "Motion File Writer stopped.";

        // stop motion file writer thread ;
        frame_writer_handle_->Close();
        if (imv_writer_handle_) imv_writer_handle_->Close();
        return true;
    }
    return false;
}

bool RaspiMotionFile::IsWriterActive() { return writer_active_; }

