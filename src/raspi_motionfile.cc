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
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#include <limits>
#include <string>
#include <list>

#include "common_types.h"

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/thread.h"
#include "rtc_base/pathutils.h"
#include "rtc_base/fileutils.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/platform_thread.h"

#include "config_motion.h"
#include "raspi_motionfile.h"
#include "utils.h"

static const char videoFileExtension[] = ".h264";
static const char imvFileExtension[] = ".imv";
static const size_t DefaultVideoFrameBufferSize=65535*2;

int  RaspiMotionFile::kEventWaitPeriod = 20;    // minimal wait ms period between frame

RaspiMotionFile::RaspiMotionFile(const std::string base_path,const std::string prefix,
            int queue_capacity, int frame_queue_size, int motion_queue_size)
    :   Event(false,false),
    base_path_(base_path), prefix_(prefix) {

    writerThreadStarted_ = false;

    frame_queue_.reset( new rtc::BufferQueue(queue_capacity, frame_queue_size ));
    imv_queue_.reset( new rtc::BufferQueue(queue_capacity, motion_queue_size ));

    // Since the imv file is relatively small compared to the video file size,
    // the size limit is used only for the video file.
    frame_file_size_limit_ = config_motion::motion_file_size_limit * 1024;

    frame_writer_buffer_ = new uint8_t[DefaultVideoFrameBufferSize];
}

RaspiMotionFile::~RaspiMotionFile() {
    delete frame_writer_buffer_;
}

///////////////////////////////////////////////////////////////////////////////
//
// Queuing the video and imv frame in buffer queue
//
///////////////////////////////////////////////////////////////////////////////
bool RaspiMotionFile::FrameQueuing(const void* data, size_t bytes,
        size_t* bytes_written, bool is_keyframe) {
    rtc::CritScope cs(&crit_sect_);
    size_t queue_length = frame_queue_->size();

    if( writerThreadStarted_ == false && queue_length == 0
            && is_keyframe == false) {
        // reject the frame when the first queuing frame is not keyframe
        // if writerThread is not enabled
        return true;
    }

    if( writerThreadStarted_ == true )
        Set(); // Event Set to wake up
    return  frame_queue_->WriteBack(data, bytes, bytes_written);
}

bool RaspiMotionFile::ImvQueuing(const void* data, size_t bytes,
        size_t* bytes_written, bool is_keyframe) {
    rtc::CritScope cs(&crit_sect_);
    size_t queue_length = imv_queue_->size();

    if( writerThreadStarted_ == false && queue_length == 0
            && is_keyframe == false) {
        // reject the frame when the first queuing frame is not keyframe
        // if writerThread is not enabled
        return true;
    }

    return imv_queue_->WriteBack(data, bytes, bytes_written);
}

void RaspiMotionFile::QueueClear(void) {
    rtc::CritScope cs(&crit_sect_);
    // Reset the frame and motion vector queue
    frame_queue_->Clear();
    imv_queue_->Clear();
}

size_t RaspiMotionFile::FrameQueueSize(void) const {
    return frame_queue_->size();
}

size_t RaspiMotionFile::ImvQueueSize(void) const {
    return imv_queue_->size();
}

///////////////////////////////////////////////////////////////////////////////
//
// Motion File Writer Thread
//
///////////////////////////////////////////////////////////////////////////////
bool RaspiMotionFile::WriterThread(void* obj) {
    return static_cast<RaspiMotionFile *> (obj)->WriterProcess();
}

bool RaspiMotionFile::WriterProcess() {
    RTC_DCHECK( writerThreadStarted_ == true )
        << "Unknown Internal Error, Thread activated without flag enabled";

    if( frame_queue_->size() == 0 && imv_queue_->size() == 0 ) {
        Wait(kEventWaitPeriod);    // Waiting for Event or Timeout
    };

    if( frame_queue_->size() ) {
        FrameFileWrite();
    }

    if( imv_queue_->size() ) {
        if( config_motion::motion_save_imv_file  == true ) {
            ImvFileWrite();
        }
        else {
            // clearing the imv queue
            imv_queue_->Clear();
        }
    }

    return true;
}

bool RaspiMotionFile::StartWriterThread() {
    rtc::CritScope cs(&crit_sect_);

    if ( writerThreadStarted_ == false ) {
        // start motion file writer thread ;
        if( OpenWriterFiles()  == false ) {
            return false;
        };

        writerThread_.reset(new rtc::PlatformThread(
                    RaspiMotionFile::WriterThread, this, "MotionWriter"));
        writerThread_->Start();
        writerThread_->SetPriority(rtc::kHighPriority);
        writerThreadStarted_ = true;
        RTC_LOG(LS_VERBOSE) << "Motion File Writer thread initialized.";
        return true;
    }
    RTC_LOG(LS_VERBOSE) << "Motion File Writer thread already initialized!";
    return false;
}

bool RaspiMotionFile::StopWriterThread() {
    rtc::CritScope cs(&crit_sect_);

    if ( writerThreadStarted_ == true) {
        writerThreadStarted_ = false;
        writerThread_->Stop();
        writerThread_.reset();
        RTC_LOG(INFO) << "Motion File Writer thread terminated.";

        // stop motion file writer thread ;
        CloseWriterFiles();
        return true;
    }
    RTC_DCHECK( frame_file_.IsOpen() == true ) << "Unknown Internal Error, frame file not closed";
    RTC_DCHECK( imv_file_.IsOpen() == true ) << "Unknown Internal Error, imv file not closed";
    return false;
}

bool RaspiMotionFile::WriterStatus() {
    return writerThreadStarted_;
}


///////////////////////////////////////////////////////////////////////////////
//
// Motion File Writer Helper Functions
//
///////////////////////////////////////////////////////////////////////////////
const std::string RaspiMotionFile::GetDateTimeString(void) {
    // Get current date/time, format is YYYY-MM-DD.HH:mm:ss
    time_t     now = time(0);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *std::localtime(&now);
    // Visit http://en.cppreference.com/w/cpp/chrono/c/strftime
    // for more information about date/time format
    std::strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);

    return buf;
}

bool RaspiMotionFile::OpenWriterFiles(void) {
    const std::string date_string = GetDateTimeString();
    std::string frame_filename = prefix_ + "_" + date_string + videoFileExtension;

    RTC_LOG(INFO) << "Video File : " <<  base_path_ + "/" + frame_filename;

    // marking h264 video file name and h264 video tempoary file name
    h264_filename_.SetPathname(base_path_,frame_filename);
    frame_filename.append(".saving");
    h264_temp_.SetPathname(base_path_,frame_filename);

    // reset file written size
    total_frame_written_size_ = 0;
    if( (frame_file_ = rtc::File::Create(base_path_ +
                    "/" + frame_filename )).IsOpen() == false  ) {
        RTC_LOG(LS_ERROR) << "Error in opening video file : " <<  frame_filename;
        return false;
    }

    // open imv file when the flag is enabled
    if( config_motion::motion_save_imv_file  == true ) {
        std::string imv_filename = prefix_ + "_" + date_string + imvFileExtension;
        RTC_LOG(INFO) << "IMV File : " <<  base_path_ + "/" + imv_filename;
        if( (imv_file_ = rtc::File::Create( base_path_ + "/" + imv_filename ))
                .IsOpen() == false  ) {
            frame_file_.Close(); // closing already opened video file
            rtc::File::Remove( frame_filename ); // and remove the video file
            RTC_LOG(LS_ERROR) << "Error in opening imv file : " <<  imv_filename;
            return false;
        };
    };

    frame_counter_ = imv_counter_ = 0;
    return true;
}

bool RaspiMotionFile::FrameFileWrite() {
    size_t read_length;
    size_t write_length;
    if( frame_file_.IsOpen() == false ) return false;

    if( frame_queue_->ReadFront(frame_writer_buffer_, DefaultVideoFrameBufferSize,
                &read_length) == true )  {
        if( frame_file_size_limit_ != 0 &&
                total_frame_written_size_ >= frame_file_size_limit_ ) {
            // frame_file_size_limit_ == 0 means size no limit in size
            // reject the additional file
            return true;
        }
        if( (write_length = frame_file_.Write( frame_writer_buffer_, read_length ) )
                < read_length) {
            RTC_LOG(LS_ERROR) << "Error in Write frame buffer : #" <<  frame_counter_;
            return false;
        }
        frame_counter_++;
        total_frame_written_size_ += write_length;

        return true;
    }
    RTC_LOG(LS_ERROR) << "Failed to read Frame from queue";

    // failed to write frame to file
    return false;
}


bool RaspiMotionFile::ImvFileWrite() {
    size_t read_length;
    if( imv_file_.IsOpen() == false ) return false;

    if( imv_queue_->ReadFront(frame_writer_buffer_, DefaultVideoFrameBufferSize,
                &read_length) == true )  {
        if( imv_file_.Write( frame_writer_buffer_, read_length ) < read_length) {
            RTC_LOG(LS_ERROR) << "Error in Write Imv buffer : #" <<  imv_counter_;
            return false;
        }
        imv_counter_++;
        return true;
    }
    RTC_LOG(LS_ERROR) << "Failed to read Imv frame queue";

    // failed to write frame to file
    return false;
}


bool RaspiMotionFile::CloseWriterFiles(void) {
    bool close_status = true;
    if( frame_file_.IsOpen() == false ) {
        RTC_LOG(LS_ERROR) << "Trying to close the video file is not opened";
        close_status = false;
    }
    frame_file_.Close();

    // Rename the video temporary file to video file name
    rtc::Filesystem::MoveFile(h264_temp_, h264_filename_);

    if( config_motion::motion_save_imv_file  == true ) {
        if( imv_file_.IsOpen() == false ) {
            RTC_LOG(LS_ERROR) << "Trying to close the imv file is not opened";
            close_status = false;
        }
        imv_file_.Close();
    };

    return close_status;
}

struct VideoFilePath{
    std::string video_file_;
    size_t size_;
};

static bool VideoFileCompare(const VideoFilePath& a,const VideoFilePath& b) {
    return a.video_file_.compare(b.video_file_) < 0;
}

bool RaspiMotionFile::ManagingVideoFolder(void) {
    std::list<struct VideoFilePath>  video_file_list;
    VideoFilePath video_file_info;
    rtc::DirectoryIterator it;
    rtc::Pathname video_files_folder;
    rtc::Pathname video_file_path;
    size_t total_directory_size = 0;
    size_t file_size = 0;
    size_t file_counter = 0;

    video_files_folder.SetPathname(config_motion::motion_directory + "/");

    // check video path is folder
    if( !rtc::Filesystem::IsFolder(video_files_folder)) {
        RTC_LOG(LS_ERROR) << "Motion file path is not folder : "
            << video_files_folder.pathname();
        return false;
    };

    // iterate video file directory
    if (!it.Iterate(video_files_folder)) {
        RTC_LOG(LS_ERROR) << "Could't make DirectoryIterator"
            << video_files_folder.pathname();
        return false;
    }
    do {
        std::string filename = it.Name();
        video_file_path.SetPathname(utils::GetFolder(video_files_folder), filename);
        if( !(filename.compare(".") == 0 || filename.compare("..") == 0) ) {
            if( rtc::Filesystem::GetFileSize(video_file_path,&file_size)) {
                video_file_info.video_file_ = filename;
                video_file_info.size_ = file_size;
                video_file_list.push_back(video_file_info);
                total_directory_size += file_size;
                file_counter++;
            };
        }
    } while ( it.Next() );

    // sorting the video file list
    video_file_list.sort(VideoFileCompare);

    RTC_LOG(INFO) << "Directory \"" << video_files_folder.pathname()
        << "\" " <<  file_counter << " files, total size: "
        << total_directory_size;

    while ( video_file_list.size() > 0 &&
            ((total_directory_size - video_file_list.front().size_)
            > (size_t)config_motion::motion_file_total_size_limit * 1000000) ) {
        total_directory_size -= video_file_list.front().size_;
        video_file_path.SetPathname( video_files_folder.pathname(),
                video_file_list.front().video_file_);
        RTC_LOG(INFO) << "Removing Video File :" << video_file_list.front().video_file_;
        rtc::Filesystem::DeleteFile( video_file_path );
        video_file_list.pop_front();
    };

    return true;
}


std::string RaspiMotionFile::VideoPathname(void) const {
    return h264_filename_.pathname();
}

std::string RaspiMotionFile::VideoFilename(void) const {
    return h264_filename_.filename();
}



