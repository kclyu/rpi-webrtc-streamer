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

#ifndef __H264STREAM_PARSER_H__
#define __H264STREAM_PARSER_H__

#include <memory>
#include <vector>
#include "h264bitstream/h264_stream.h"

namespace webrtc {

struct NalUnit {
    uint32_t offset_;
    uint32_t length_;

    NalUnit() {};
    NalUnit( int offset, int length ) {
        offset_ = offset;
        length_ = length;
    };
};


class H264StreamParser {
private:
    void printNalType( void );
    void GetNalUnitList(const uint8_t *buffer, const uint32_t length, std::vector<NalUnit>* nal_list);
    int NextNalUnit(const uint8_t *buffer, const uint32_t length,const int offset );

    h264_stream_t* parser_internal_;
    bool verbose_;
    bool inited_;
    uint32_t pic_init_qp_minus26_;
    uint32_t slice_qp_delta_;
    bool pps_parsed_;
    bool slice_parsed_;

public:
    H264StreamParser();
    ~H264StreamParser();
    bool Init( void );

    void ParseBitstream(uint8_t* bitstream, size_t length, std::vector<NalUnit>* nal_list);
    bool GetLastSliceQp( int *qp );
    void Verbose(bool flag);
};



}	// namespace webrtc

#endif // __H264STREAM_PARSER_H__
