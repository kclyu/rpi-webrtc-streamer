/*
Copyright (c) 2016, rpi-webrtc-streamer Lyu,KeunChang

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

#include <stdio.h>
#include <string.h>
#include "webrtc/base/checks.h"
#include "webrtc/base/common.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/stringutils.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"

#include "mmal_wrapper.h"


namespace webrtc {

// MMAL Encoder singleton reference
MMALEncoderWrapper* getMMALEncoderWrapper() {
    static MMALEncoderWrapper encode_wrapper_;
    return &encode_wrapper_;
}

//
//
FrameQueue::FrameQueue() {
    num_ = FRAME_QUEUE_LENGTH;
    size_ = FRAME_BUFFER_SIZE;

    encoded_frame_queue_ 	= nullptr;
    pool_internal_ 			= nullptr;
    frame_buf_ 				= nullptr;
    frame_buf_pos_ 			= 0;
    frame_segment_cnt_ 		= 0;
    inited_ 				= false;
    frame_count_ 			= 0;
    frame_drop_ 			= 0;
    frame_queue_drop_ 		= 0;
}

FrameQueue::~FrameQueue() {
    mmal_queue_destroy(encoded_frame_queue_);
    mmal_pool_destroy(pool_internal_);
    delete frame_buf_;
}

bool FrameQueue::Isinitialized( void ) {
    return inited_;
}

bool FrameQueue::Init( void ) {
    if( inited_ == true ) return true;
    if((pool_internal_ = mmal_pool_create( num_, size_ )) == NULL ) {
        LOG(LS_ERROR) << "FrameQueue internal pool creation failed";
        return false;
    };

    if((encoded_frame_queue_ = mmal_queue_create()) == NULL ) {
        LOG(LS_ERROR) << "FrameQueue queue creation failed";
        return false;
    };

    // temporary buffer to assemble frame from encoder
    frame_buf_ = new uint8_t [size_];
    inited_ = true;

    return true;
}

void FrameQueue::Reset( void ) {
    MMAL_BUFFER_HEADER_T *buf;
    int len = mmal_queue_length( encoded_frame_queue_);

    for( int i = 0 ; i < len ; i++ ) {
        buf = mmal_queue_get(encoded_frame_queue_);
        mmal_buffer_header_release(buf);
    };
    frame_buf_pos_ = 0;
    frame_segment_cnt_ = 0;
    inited_ = true;

    RTC_DCHECK_EQ(mmal_queue_length(encoded_frame_queue_), 0);
    RTC_DCHECK_EQ(mmal_queue_length(pool_internal_->queue), FRAME_QUEUE_LENGTH);
}

int FrameQueue::Length( void ) {
    return mmal_queue_length( encoded_frame_queue_);
}

MMAL_BUFFER_HEADER_T * FrameQueue::GetBufferFromPool( void ) {
    RTC_DCHECK(inited_);

    return mmal_queue_get( pool_internal_->queue);
}

void FrameQueue::ReturnToPool( MMAL_BUFFER_HEADER_T *buffer ) {
    RTC_DCHECK(inited_);
    mmal_buffer_header_release(buffer);
}

void FrameQueue::QueueingFrame( MMAL_BUFFER_HEADER_T *buffer ) {
    RTC_DCHECK(inited_);
    mmal_queue_put( encoded_frame_queue_, buffer);
}

MMAL_BUFFER_HEADER_T *FrameQueue::DequeueFrame( void ) {
    RTC_DCHECK(inited_);
    return mmal_queue_get(encoded_frame_queue_);
}

void FrameQueue::ProcessBuffer( MMAL_BUFFER_HEADER_T *buffer ) {
    RTC_DCHECK(inited_);
    RTC_DCHECK(buffer != NULL);
    RTC_DCHECK_GE((mmal_queue_length(encoded_frame_queue_)+
                   mmal_queue_length(pool_internal_->queue)), FRAME_QUEUE_LENGTH - 1);

    // it should be same as FRAME_QUEUE_LENGTH except one queue in the encoder
    // there is something in the buffer
    if( buffer->length < FRAME_BUFFER_SIZE && buffer->length > 0 ) {

        // there is no end of frame mark in this buffer, so keep it in the internal buffer
        if( !(buffer->flags & MMAL_BUFFER_HEADER_FLAG_FRAME_END) ||
                (buffer->flags & MMAL_BUFFER_HEADER_FLAG_CONFIG ) ) {
            RTC_DCHECK(frame_buf_pos_ < size_);
            mmal_buffer_header_mem_lock(buffer);
            // save partial frame data to assemble frame at next time
            memcpy( frame_buf_ + frame_buf_pos_,  buffer->data, buffer->length);
            frame_buf_pos_ += buffer->length;
            frame_segment_cnt_ ++;
            mmal_buffer_header_mem_unlock(buffer);      
            // RTC_DCHECK(frame_segment_cnt_ < 4);	// temp 2 --> 4
        }
        // end of frame marked
        else if( buffer->flags & MMAL_BUFFER_HEADER_FLAG_FRAME_END ) {
            if( (int)(frame_buf_pos_ + buffer->length) >= size_) {
                LOG(INFO) << "frame_buf_pos : " << frame_buf_pos_
                          << ", buffer length: " << buffer->length;
            };
            RTC_DCHECK( (int)(frame_buf_pos_ + buffer->length) < size_);

            // getting frame from pool
            MMAL_BUFFER_HEADER_T *frame = mmal_queue_get(pool_internal_->queue);
            if( frame ) { 	// frame buffer is available
                mmal_buffer_header_mem_lock(buffer);
                // copy the previously saved frame at first
                if( frame_buf_pos_  ) {
                    memcpy( frame->data, frame_buf_, frame_buf_pos_);
                }
                memcpy( frame->data+frame_buf_pos_, buffer->data, buffer->length);

                // copying meta data to frame
                mmal_buffer_header_copy_header( frame, buffer);
                if( frame_buf_pos_  ) {
                    frame->length += frame_buf_pos_;
                }

                mmal_buffer_header_mem_unlock(buffer);

                // reset internal buffer pointer
                frame_segment_cnt_  = frame_buf_pos_ = 0;
                // one frame done
                mmal_queue_put( encoded_frame_queue_, frame);
            }
            else {
                frame_drop_ ++;
                LOG(INFO) << "MMAL Frame Dropped during ProcessBuffer : " << frame_drop_;
            }
        }

        // frame count statistics
        frame_count_++;
    }
}

void FrameQueue::dumpFrameQueueInfo( void ) {
    RTC_DCHECK(inited_);
    LOG(INFO) << "Queue length: " << mmal_queue_length(encoded_frame_queue_);
    LOG(INFO) << "Pool length: " << mmal_queue_length(pool_internal_->queue);
}

////////////////////////////////////////////////////////////////////////////////////////
//
//
//
////////////////////////////////////////////////////////////////////////////////////////
MMALEncoderWrapper::MMALEncoderWrapper()
    : crit_sect_(CriticalSectionWrapper::CreateCriticalSection())
{

    camera_preview_port_ 	= nullptr;
    camera_video_port_ 		= nullptr;
    camera_still_port_		= nullptr;
    preview_input_port_		= nullptr;
    encoder_input_port_		= nullptr;
    encoder_output_port_	= nullptr;

    bcm_host_init();

    // Register our application with the logging system
    vcos_log_register("mmal_wrappper", VCOS_LOG_CATEGORY);

    // reset encoder setting to default state
    default_status(&state_);

    FrameQueue();
}

MMALEncoderWrapper::~MMALEncoderWrapper()
{
    if (crit_sect_) {
        delete crit_sect_;
    }
}

int MMALEncoderWrapper::getWidth( void )
{
    return state_.width;
}

int MMALEncoderWrapper::getHeight( void )
{
    return state_.height;
}


bool MMALEncoderWrapper::InitEncoder(int width, int height, int framerate, int bitrate)
{
    MMAL_STATUS_T status = MMAL_SUCCESS;

    LOG(INFO) << "Start initialize the MMAL encode wrapper."
              << width << "x" << height << "@" << framerate << ", " << bitrate << "kbps";
    CriticalSectionScoped cs(crit_sect_);

    state_.width =  width;
    state_.height =  height;
    state_.framerate =  framerate;
    state_.bitrate =  bitrate * 1000;

    if ((status = create_camera_component(&state_)) != MMAL_SUCCESS) {
        LOG(LS_ERROR) << "Failed to create camera component";
        return false;
    }
    else if ((status = raspipreview_create(&state_.preview_parameters)) != MMAL_SUCCESS) {
        LOG(LS_ERROR) << "Failed to create preview component";
        destroy_camera_component(&state_);
        return false;
    }
    else if ((status = create_encoder_component(&state_)) != MMAL_SUCCESS) {
        LOG(LS_ERROR) << "Failed to create encode component";
        raspipreview_destroy(&state_.preview_parameters);
        destroy_camera_component(&state_);
        return false;
    }
    else {
        if (state_.verbose)
            LOG(INFO) << "Starting component connection stage";

        camera_preview_port_ = state_.camera_component->output[MMAL_CAMERA_PREVIEW_PORT];
        camera_video_port_   = state_.camera_component->output[MMAL_CAMERA_VIDEO_PORT];
        camera_still_port_   = state_.camera_component->output[MMAL_CAMERA_CAPTURE_PORT];
        preview_input_port_  = state_.preview_parameters.preview_component->input[0];
        encoder_input_port_  = state_.encoder_component->input[0];
        encoder_output_port_ = state_.encoder_component->output[0];

        if (state_.preview_parameters.wantPreview ) {
            if (state_.verbose) {
                LOG(INFO) << "Connecting camera preview port to preview input port";
                LOG(INFO) << "Starting video preview";
            }

            // Connect camera to preview
            status = connect_ports(camera_preview_port_, preview_input_port_, &state_.preview_connection);

            if (status != MMAL_SUCCESS) {
                state_.preview_connection = nullptr;
                return false;
            }
        }

        if (state_.verbose)
            LOG(INFO) << "Connecting camera stills port to encoder input port";

        // Now connect the camera to the encoder
        status = connect_ports(camera_video_port_, encoder_input_port_, &state_.encoder_connection);
        if (status != MMAL_SUCCESS) {
            state_.encoder_connection = nullptr;
            LOG(LS_ERROR) << "Failed to connect camera video port to encoder input";
            return false;
        }

        // Set up our userdata - this is passed though to the callback where we need the information.
        state_.callback_data.pstate = &state_;
        state_.callback_data.abort = 0;

        encoder_output_port_->userdata = (struct MMAL_PORT_USERDATA_T *)this;
        if (state_.verbose)
            LOG(INFO) << "Enabling encoder output port";

        // FrameQueue Init
        Init();

        // Enable the encoder output port and tell it its callback function
        status = mmal_port_enable(encoder_output_port_, BufferCallback);
        if (status != MMAL_SUCCESS) {
            LOG(LS_ERROR) << "Failed to setup encoder output";
            return false;
        }

        dump_all_mmal_component(&state_);

        return true;
    }

    LOG(LS_ERROR) << "Not Reached";
    return false;
}

bool MMALEncoderWrapper::ReinitEncoder(int width, int height, int framerate, int bitrate) {
    MMAL_STATUS_T status = MMAL_SUCCESS;

    LOG(INFO) << "Start reinitialize the MMAL encode wrapper."
              << width << "x" << height << "@" << framerate << ", " << bitrate << "kbps";
    CriticalSectionScoped cs(crit_sect_);

    state_.width =  width;
    state_.height =  height;
    state_.framerate =  framerate;
    state_.bitrate =  bitrate * 1000;

    //
    // disable all component
    //
    if ( StopCapture() == false ) {
        LOG(LS_ERROR) << "Unable to unset capture start";
        return false;
    }

    //
    dump_component((char *)"camera: ", state_.camera_component );
    dump_component((char *)"encoder: ", state_.encoder_component );

    // Disable all our ports that are not handled by connections
    check_disable_port(camera_still_port_);
    check_disable_port(encoder_output_port_);
    check_disable_port(state_.camera_component->control);

    if (state_.preview_parameters.wantPreview && state_.preview_connection)
        mmal_connection_destroy(state_.preview_connection);

    if (state_.encoder_connection)
        mmal_connection_destroy(state_.encoder_connection);

    /* Disable components */
    if (state_.encoder_component)
        mmal_component_disable(state_.encoder_component);

    if (state_.preview_parameters.preview_component)
        mmal_component_disable(state_.preview_parameters.preview_component);

    if (state_.camera_component)
        mmal_component_disable(state_.camera_component);

    //
    // Reseting all component
    //
    if ((status = reset_camera_component(&state_)) != MMAL_SUCCESS) {
        LOG(LS_ERROR) << "Failed to create camera component";
        return false;
    }
    else if ((status = raspipreview_create(&state_.preview_parameters)) != MMAL_SUCCESS) {
        LOG(LS_ERROR) << "Failed to create preview component";
        destroy_camera_component(&state_);
        return false;
    }
    else if ((status = reset_encoder_component(&state_)) != MMAL_SUCCESS) {
        LOG(LS_ERROR) << "Failed to create encode component";
        raspipreview_destroy(&state_.preview_parameters);
        destroy_camera_component(&state_);
        return false;
    }
    else {
        if (state_.verbose)
            LOG(INFO) << "Starting component connection stage";

        camera_preview_port_ = state_.camera_component->output[MMAL_CAMERA_PREVIEW_PORT];
        camera_video_port_   = state_.camera_component->output[MMAL_CAMERA_VIDEO_PORT];
        camera_still_port_   = state_.camera_component->output[MMAL_CAMERA_CAPTURE_PORT];
        preview_input_port_  = state_.preview_parameters.preview_component->input[0];
        encoder_input_port_  = state_.encoder_component->input[0];
        encoder_output_port_ = state_.encoder_component->output[0];

        if (state_.preview_parameters.wantPreview ) {
            if (state_.verbose) {
                LOG(INFO) << "Connecting camera preview port to preview input port";
                LOG(INFO) << "Starting video preview";
            }

            // Connect camera to preview
            status = connect_ports(camera_preview_port_, preview_input_port_, &state_.preview_connection);

            if (status != MMAL_SUCCESS) {
                state_.preview_connection = nullptr;
                return false;
            }
        }

        if (state_.verbose)
            LOG(INFO) << "Connecting camera stills port to encoder input port";

        // Now connect the camera to the encoder
        status = connect_ports(camera_video_port_, encoder_input_port_, &state_.encoder_connection);
        if (status != MMAL_SUCCESS) {
            state_.encoder_connection = nullptr;
            LOG(LS_ERROR) << "Failed to connect camera video port to encoder input";
            return false;
        }

        // Set up our userdata - this is passed though to the callback where we need the information.
        state_.callback_data.pstate = &state_;
        state_.callback_data.abort = 0;

        encoder_output_port_->userdata = (struct MMAL_PORT_USERDATA_T *)this;
        if (state_.verbose)
            LOG(INFO) << "Enabling encoder output port";

        // FrameQueue Init
        Init();

        // Enable the encoder output port and tell it its callback function
        status = mmal_port_enable(encoder_output_port_, BufferCallback);
        if (status != MMAL_SUCCESS) {
            LOG(LS_ERROR) << "Failed to setup encoder output";
            return false;
        }

        dump_all_mmal_component(&state_);

        return true;
    }

    LOG(LS_ERROR) << "Not Reached";
    return false;
}


void MMALEncoderWrapper::BufferCallback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) {
    // static_cast<MMALEncoderWrapper *> (port->userdata)-> OnBufferCallback(port,buffer);
    //((MMALEncoderWrapper*)port->userdata)->OnBufferCallback(port,buffer);
    reinterpret_cast<MMALEncoderWrapper *> (port->userdata)-> OnBufferCallback(port,buffer);
}


bool MMALEncoderWrapper::UninitEncoder(void) {
    CriticalSectionScoped cs(crit_sect_);

    LOG(INFO) << "unitialize the MMAL encode wrapper.";

    // Disable all our ports that are not handled by connections
    check_disable_port(camera_still_port_);
    check_disable_port(encoder_output_port_);

    if (state_.preview_parameters.wantPreview && state_.preview_connection)
        mmal_connection_destroy(state_.preview_connection);

    if (state_.encoder_connection)
        mmal_connection_destroy(state_.encoder_connection);

    /* Disable components */
    if (state_.encoder_component)
        mmal_component_disable(state_.encoder_component);

    if (state_.preview_parameters.preview_component)
        mmal_component_disable(state_.preview_parameters.preview_component);

    if (state_.camera_component)
        mmal_component_disable(state_.camera_component);

    destroy_encoder_component(&state_);
    raspipreview_destroy(&state_.preview_parameters);
    destroy_camera_component(&state_);

    return true;
}

void MMALEncoderWrapper::OnBufferCallback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) {
    MMAL_BUFFER_HEADER_T *new_buffer = nullptr;
    static int64_t base_time =  -1;
    static int64_t last_second = -1;
    int64_t current_time = vcos_getmicrosecs64()/1000;

    // All our segment times based on the receipt of the first encoder callback
    if (base_time == -1)
        base_time = vcos_getmicrosecs64()/1000;

    // We pass our file handle and other stuff in via the userdata field.
    PORT_USERDATA *pData = (PORT_USERDATA *)port->userdata;

    if (pData) {
        ProcessBuffer(buffer);
    }
    else {
        vcos_log_error("Received a encoder buffer callback with no state");
    }

    // release buffer back to the pool
    mmal_buffer_header_release(buffer);

    // and send one back to the port (if still open)
    if (port->is_enabled) {
        MMAL_STATUS_T status;

        new_buffer = mmal_queue_get(state_.encoder_pool->queue);

        if (new_buffer)
            status = mmal_port_send_buffer(port, new_buffer);

        if (!new_buffer || status != MMAL_SUCCESS)
            LOG(LS_ERROR) << "Unable to return a buffer to the encoder port";
    }

    // See if the second count has changed and we need to update any annotation
    if (current_time/1000 != last_second) {
        update_annotation_data(&state_);
        last_second = current_time/1000;
    }
}


bool MMALEncoderWrapper::StartCapture( void ) {
    // Send all the buffers to the encoder output port
    int num = mmal_queue_length(state_.encoder_pool->queue);

    for (int q=0; q<num; q++) {
        MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(state_.encoder_pool->queue);

        if (!buffer)
            LOG(LS_ERROR) << "Unable to get a required buffer " << q << " from pool queue";

        if (mmal_port_send_buffer(encoder_output_port_, buffer)!= MMAL_SUCCESS)
            LOG(LS_ERROR) << "Unable to send a buffer to encoder output port (" << q << ").";
    }

    if (mmal_port_parameter_set_boolean(camera_video_port_, MMAL_PARAMETER_CAPTURE, MMAL_TRUE)
            != MMAL_SUCCESS) {
        LOG(LS_ERROR) << "Unable to set capture start";
        return false;
    }
    LOG(INFO) << "capture started.";

    return true;
}


bool MMALEncoderWrapper::StopCapture( void ) {
    if (mmal_port_parameter_set_boolean(camera_video_port_, MMAL_PARAMETER_CAPTURE, MMAL_FALSE)
            != MMAL_SUCCESS) {
        LOG(LS_ERROR) << "Unable to unset capture start";
        return false;
    }
    LOG(INFO) << "capture stopped.";
    return true;
}


//
bool MMALEncoderWrapper::SetFrame(int width, int height ) {
    if( state_.width != width || state_.height != height ) {
        CriticalSectionScoped cs(crit_sect_);

        LOG(INFO) << "MMAL frame encoding parameters changed:  "
                  << width << "x" << height << "@" << state_.framerate << ", " << state_.bitrate << "kbps";

        state_.width = width;
        state_.height = height;
    }
    return true;
}


//
bool MMALEncoderWrapper::SetRate(int framerate, int bitrate) {
    if( state_.framerate != framerate || state_.bitrate != bitrate*1000 ) {
        CriticalSectionScoped cs(crit_sect_);
        MMAL_STATUS_T status;

        // LOG(INFO) << "MMAL frame encoding rate changed : "
        // 	<< state_.width << "x" << state_.height << "@" << framerate << ", " << bitrate << "kbps";
        if( state_.bitrate != bitrate * 1000 ) {
            MMAL_PARAMETER_UINT32_T param =
            {{ MMAL_PARAMETER_VIDEO_BIT_RATE, sizeof(param)}, (uint32_t)bitrate*1000};
            // {{ MMAL_PARAMETER_VIDEO_ENCODE_PEAK_RATE, sizeof(param)}, (uint32_t)bitrate*1000};
            status = mmal_port_parameter_set(encoder_output_port_, &param.hdr);
            if (status != MMAL_SUCCESS) {
                LOG(LS_ERROR) << "Unable to set bitrate";
            }
        }

        if( state_.framerate != framerate ) {   // frame rate
            LOG(INFO) << "MMAL frame encoding framerate changed : "  << framerate << " fps";
            MMAL_PARAMETER_FRAME_RATE_T param;
            param.hdr.id = MMAL_PARAMETER_FRAME_RATE;
            param.hdr.size = sizeof(param);
            param.frame_rate.num	= framerate;
            param.frame_rate.den	= 1;

            // status = mmal_port_parameter_set(encoder_output_port_, &param.hdr);
            status = mmal_port_parameter_set(camera_video_port_, &param.hdr);
            if (status != MMAL_SUCCESS) {
                LOG(LS_ERROR) << "Unable to set framerate";
            }

        }
        state_.bitrate = bitrate*1000;
        state_.framerate = framerate;

    }
    return true;
}


//
bool MMALEncoderWrapper::ForceKeyFrame(void) {
    CriticalSectionScoped cs(crit_sect_);
    LOG(INFO) << "MMAL force key frame encoding";

    if (mmal_port_parameter_set_boolean(encoder_output_port_,
                MMAL_PARAMETER_VIDEO_REQUEST_I_FRAME, MMAL_TRUE) != MMAL_SUCCESS) {
        LOG(LS_ERROR) << "failed to request KeyFrame";
        return false;
    }
    return true;
}

}	// webrtc namespace

