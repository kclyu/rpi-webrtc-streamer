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

#include "h264bitstream_parser.h"

#define HLOG_FORMAT(format,args...) \
	sprintf(buf, format, args);\
	LOG(INFO) << buf;
#define HLOG(A) \
	LOG(INFO) << A;

namespace webrtc {

//
//
H264StreamParser::H264StreamParser() {
    parser_internal_ = nullptr;
    inited_ = false;
    pic_init_qp_minus26_ = 0;
    slice_qp_delta_ = 0;
}

H264StreamParser::~H264StreamParser() {
    h264_free(parser_internal_);
}

void H264StreamParser::Verbose(bool flag ) {
    verbose_ = flag;
}


bool H264StreamParser::Init( void ) {
    if( inited_ == true ) return true;

    parser_internal_ = h264_new();
    inited_ = true;
    pps_parsed_ = false;
    slice_parsed_ = false;
    verbose_ = false;

    return true;
}

int H264StreamParser::NextNalUnit(const uint8_t *buffer, const uint32_t length,const int offset ) {
    const uint8_t start_code[4] = {0, 0, 0, 1};

    // search NAL start code
    for (uint32_t index=offset; index < length; index++ ) {
        // search the start code(NAL) in buffer
        if( length  >  index + sizeof(start_code) &&
                (memcmp( buffer+index, start_code, sizeof(start_code)) == 0) ) {
            // found the start code
            return index;
        }
    }
    return length;
}

// Search the NAL start code in the given buffer.
// NalUnit offset value ignore NAL unit size
void H264StreamParser::GetNalUnitList(const uint8_t *buffer, const uint32_t length, std::vector<NalUnit>* nal_unit_list) {
    uint32_t offset, first_nal, second_nal;
    // first search whole NAL unit in the buffer

    for(  first_nal = second_nal = offset = 0 ; offset < length; ) {
        first_nal = NextNalUnit( buffer, length, offset);
        second_nal = NextNalUnit( buffer, length, first_nal+ 4 /* start code size*/);

        if( first_nal != second_nal ) {
            nal_unit_list->push_back( NalUnit( first_nal + 4 /*start code*/,
                                               second_nal - ( first_nal + 4) ));
        }

        offset = second_nal;	 // second_nal is next starting position
    }
}


bool H264StreamParser::GetLastSliceQp( int *qp ) {
    RTC_DCHECK(parser_internal_);

    if( !parser_internal_->pps ) return false;
    if( !parser_internal_->sh ) return false;
    if( !pps_parsed_ ) return false;
    if( !slice_parsed_ ) return false;

    *qp = 26 + pic_init_qp_minus26_ + slice_qp_delta_;

    return true;
}

void H264StreamParser::ParseBitstream(uint8_t* bitstream, size_t length, std::vector<NalUnit>* nal_list) {
    char buf[256];
    int	 offset=0;
    if( inited_ == false ) Init();

    RTC_DCHECK(parser_internal_);

    uint32_t nal_index;
    std::vector<NalUnit> nal_unit_list;

    // Getting NAL Unit list from encoded image
    GetNalUnitList(bitstream, length, &nal_unit_list);
    if( nal_list != nullptr ) *nal_list = nal_unit_list;	// copy the nal list

    nal_index = 0;
    for( std::vector<NalUnit>::const_iterator it =  nal_unit_list.begin();
            it != nal_unit_list.end(); it++, nal_index++ ) {

        read_nal_unit(parser_internal_,bitstream + it->offset_, it->length_);
        nal_t* nal = parser_internal_->nal;
        printNalType();

        if( nal->nal_unit_type == NAL_UNIT_TYPE_CODED_SLICE_NON_IDR) {
            RTC_DCHECK(parser_internal_->sh);
            slice_qp_delta_ = parser_internal_->sh->slice_qp_delta;
            slice_parsed_ = true;
        }
        else if( nal->nal_unit_type == NAL_UNIT_TYPE_CODED_SLICE_IDR) {
            RTC_DCHECK(parser_internal_->sh);
            slice_qp_delta_ = parser_internal_->sh->slice_qp_delta;
            slice_parsed_ = true;
        }
        else if( nal->nal_unit_type == NAL_UNIT_TYPE_SPS) {
            RTC_DCHECK(parser_internal_->sps);
        }
        else if( nal->nal_unit_type == NAL_UNIT_TYPE_PPS) {
            RTC_DCHECK(parser_internal_->pps);
            pic_init_qp_minus26_ = parser_internal_->pps->pic_init_qp_minus26 ;
            slice_qp_delta_ =  0;
            pps_parsed_ = true;
            slice_parsed_ = false;
        }
        else if( nal->nal_unit_type == NAL_UNIT_TYPE_AUD) {
            RTC_DCHECK(parser_internal_->aud);
            // do nothing...
        }
        else if( nal->nal_unit_type == NAL_UNIT_TYPE_SEI) {
            // do nothing...
        }
    }
}


/* this function is modified version of h264bitstream debug_nal */
void H264StreamParser::printNalType( void )
{

    if( !verbose_ ) return;

    char buf[256];
    h264_stream_t* h = parser_internal_;
    nal_t* nal = parser_internal_->nal;

    HLOG("==================== NAL ====================");
    HLOG_FORMAT(" forbidden_zero_bit : %d", nal->forbidden_zero_bit );
    HLOG_FORMAT(" nal_ref_idc : %d", nal->nal_ref_idc );

    const char* nal_unit_type_name;
    switch (nal->nal_unit_type)
    {
    case  NAL_UNIT_TYPE_UNSPECIFIED :
        nal_unit_type_name = "Unspecified";
        break;
    case  NAL_UNIT_TYPE_CODED_SLICE_NON_IDR :
        nal_unit_type_name = "Coded slice of a non-IDR picture";
        break;
    case  NAL_UNIT_TYPE_CODED_SLICE_DATA_PARTITION_A :
        nal_unit_type_name = "Coded slice data partition A";
        break;
    case  NAL_UNIT_TYPE_CODED_SLICE_DATA_PARTITION_B :
        nal_unit_type_name = "Coded slice data partition B";
        break;
    case  NAL_UNIT_TYPE_CODED_SLICE_DATA_PARTITION_C :
        nal_unit_type_name = "Coded slice data partition C";
        break;
    case  NAL_UNIT_TYPE_CODED_SLICE_IDR :
        nal_unit_type_name = "Coded slice of an IDR picture";
        break;
    case  NAL_UNIT_TYPE_SEI :
        nal_unit_type_name = "Supplemental enhancement information (SEI)";
        break;
    case  NAL_UNIT_TYPE_SPS :
        nal_unit_type_name = "Sequence parameter set";
        break;
    case  NAL_UNIT_TYPE_PPS :
        nal_unit_type_name = "Picture parameter set";
        break;
    case  NAL_UNIT_TYPE_AUD :
        nal_unit_type_name = "Access unit delimiter";
        break;
    case  NAL_UNIT_TYPE_END_OF_SEQUENCE :
        nal_unit_type_name = "End of sequence";
        break;
    case  NAL_UNIT_TYPE_END_OF_STREAM :
        nal_unit_type_name = "End of stream";
        break;
    case  NAL_UNIT_TYPE_FILLER :
        nal_unit_type_name = "Filler data";
        break;
    case  NAL_UNIT_TYPE_SPS_EXT :
        nal_unit_type_name = "Sequence parameter set extension";
        break;
        // 14..18    // Reserved
    case  NAL_UNIT_TYPE_CODED_SLICE_AUX :
        nal_unit_type_name = "Coded slice of an auxiliary coded picture without partitioning";
        break;
        // 20..23    // Reserved
        // 24..31    // Unspecified
    default :
        nal_unit_type_name = "Unknown";
        break;
    }
    HLOG_FORMAT(" nal_unit_type : %d ( %s )", nal->nal_unit_type, nal_unit_type_name );
}

}	// webrtc namespace

