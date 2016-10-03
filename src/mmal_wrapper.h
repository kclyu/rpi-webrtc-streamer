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

#ifndef __MMAL_WRAPPER_H__
#define __MMAL_WRAPPER_H__

#include "webrtc/system_wrappers/include/critical_section_wrapper.h"
#include "mmal_encoder.h"

namespace webrtc {

#define FRAME_BUFFER_SIZE	65536
#define FRAME_QUEUE_LENGTH 10


class FrameQueue {
	private:
		CriticalSectionWrapper* crit_sect_;

		int num_, size_;

		MMAL_QUEUE_T *encoded_frame_queue_;
		MMAL_POOL_T *pool_internal_;
		uint8_t		*keyframe_buf_;
		uint8_t		*frame_buf_;
		int			frame_buf_pos_;
		int			frame_segment_cnt_;
		bool		inited_;

		uint32_t	frame_count_;
		uint32_t	frame_drop_;
		uint32_t	frame_queue_drop_;

	public: 
		FrameQueue();
		 ~FrameQueue();
		bool Init( void );
		void Reset( void );
		bool Isinitialized( void );

		// Queueuing and Assembling frame
		void ProcessBuffer( MMAL_BUFFER_HEADER_T *buffer );
		int Length( void );

		// Buffer header handling
		MMAL_BUFFER_HEADER_T * GetBufferFromPool( void );
		void ReturnToPool( MMAL_BUFFER_HEADER_T *buffer );
		void QueueingFrame( MMAL_BUFFER_HEADER_T *buffer );
		MMAL_BUFFER_HEADER_T *DequeueFrame( void );
		void dumpFrameQueueInfo( void );
};


class MMALEncoderWrapper : public FrameQueue {

public:

	MMALEncoderWrapper();
	~MMALEncoderWrapper();

	int getWidth(void);
	int getHeight(void);
	bool SetFrame(int width, int height);
	bool SetRate(int framerate, int bitrate);
	bool InitEncoder(int width, int height, int framerate, int bitrate);
	bool ReinitEncoder(int width, int height, int framerate, int bitrate);
	bool UninitEncoder(void);
	bool ForceKeyFrame(void);
	bool IsKeyFrame(void);
	bool StartCapture(void);
	bool StopCapture(void);

	// Callback Functions
	void OnBufferCallback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
	static void BufferCallback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);


private:
	MMAL_PORT_T *camera_preview_port_;
	MMAL_PORT_T *camera_video_port_;
	MMAL_PORT_T *camera_still_port_;
	MMAL_PORT_T *preview_input_port_;
	MMAL_PORT_T *encoder_input_port_;
	MMAL_PORT_T *encoder_output_port_;
	RASPIVID_STATE state_;

	CriticalSectionWrapper* crit_sect_;
};

// singleton wrappper 
MMALEncoderWrapper* getMMALEncoderWrapper(void);

}	// namespace webrtc

#endif // __MMAL_WRAPPER_H__
