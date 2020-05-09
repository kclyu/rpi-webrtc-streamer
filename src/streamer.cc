/*
 *  Copyright (c) 2016, rpi-webrtc-streamer  Lyu,KeunChang
 *
 * streamer.cc
 *
 * Modified version of src/webrtc/examples/peer/client/conductor.cc in WebRTC
 * source tree The origianl copyright info below.
 */

/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "streamer.h"

#include <memory>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/types/optional.h"
#include "api/audio/audio_mixer.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/audio_options.h"
#include "api/create_peerconnection_factory.h"
#include "api/rtp_sender_interface.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "config_media.h"
#include "config_streamer.h"
#include "modules/audio_device/dummy/audio_device_dummy.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/video_capture/video_capture.h"
#include "modules/video_capture/video_capture_factory.h"
#include "p2p/base/port_allocator.h"
#include "pc/test/fake_video_track_source.h"
#include "pc/video_track_source.h"
#include "raspi_decoder.h"
#include "raspi_decoder_dummy.h"
#include "raspi_encoder.h"
#include "raspi_encoder_impl.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/strings/json.h"
#include "streamer_observer.h"
#include "test/vcm_capturer.h"
#include "utils_pc_strings.h"

// Names used for SDP label
static const char kAudioLabel[] = "audio_label";
static const char kVideoLabel[] = "video_label";
static const char kStreamId[] = "stream_id";

// Names used for a Android Direct IceCandidate JSON object.
static const char kCandidateSdpMidName[] = "id";
static const char kCandidateSdpMlineIndexName[] = "label";
static const char kCandidateSdpName[] = "candidate";
static const char kCandidateType[] = "type";

// Names used for a SessionDescription JSON object.
static const char kSessionDescriptionTypeName[] = "type";
static const char kSessionDescriptionSdpName[] = "sdp";

class DummySetSessionDescriptionObserver
    : public webrtc::SetSessionDescriptionObserver {
   public:
    static DummySetSessionDescriptionObserver* Create() {
        return new rtc::RefCountedObject<DummySetSessionDescriptionObserver>();
    }
    virtual void OnSuccess() { RTC_LOG(INFO) << __FUNCTION__; }
    virtual void OnFailure(webrtc::RTCError error) {
        RTC_LOG(INFO) << __FUNCTION__ << " " << ToString(error.type()) << ": "
                      << error.message();
    }

   protected:
    DummySetSessionDescriptionObserver() {}
    ~DummySetSessionDescriptionObserver() {}
};

Streamer::Streamer(SocketServerObserver* session, StreamerConfig* config)
    : peer_id_(-1) {
    RTC_DCHECK(session != nullptr);
    session_ = session;
    session->RegisterObserver(this);
    streamer_config_ = config;
}

Streamer::~Streamer() { RTC_DCHECK(!peer_connection_); }

bool Streamer::connection_active() const { return peer_connection_ != nullptr; }

void Streamer::Close() {
    // Reset session active session
    DeletePeerConnection();
}

bool Streamer::InitializePeerConnection() {
    RTC_DCHECK(!peer_connection_factory_);
    RTC_DCHECK(!peer_connection_);

    // The first time ice_state/peerconnection_state is initialized with k*New.
    ice_state_ =
        webrtc::PeerConnectionInterface::IceConnectionState::kIceConnectionNew;
    peerconnection_state_ =
        webrtc::PeerConnectionInterface::PeerConnectionState::kNew;

    // Create threads and necessary objects to create the PeerConnectionFactory
    rtc::ThreadManager::Instance()->WrapCurrentThread();

    network_thread_ = rtc::Thread::CreateWithSocketServer();
    network_thread_->SetName("network_thread", nullptr);
    RTC_CHECK(network_thread_->Start()) << "Failed to start network thread";

    worker_thread_ = rtc::Thread::Create();
    worker_thread_->SetName("worker_thread", nullptr);
    RTC_CHECK(worker_thread_->Start()) << "Failed to start worker thread";

    signaling_thread_ = rtc::Thread::Create();
    signaling_thread_->SetName("signaling_thread", nullptr);
    RTC_CHECK(signaling_thread_->Start()) << "Failed to start signaling thread";

    //  Audio Device Module
    adm_ = nullptr;
    // TODO: Tempoary Disable audio_enable config parameter.
    // Current WebRTC native stack have problem with setting callback of
    // dummy audio
    /****
    if(streamer_config_->GetAudioEnable() == false ) {
    //  The default value of AudioEnable is false. To use audio,
    //  you must set audio_enable to true in webrtc_streamer.conf.
    adm_ = webrtc::AudioDeviceModule::Create(
        webrtc::AudioDeviceModule::AudioLayer::kDummyAudio);
    };
    *****/
    peer_connection_factory_ = webrtc::CreatePeerConnectionFactory(
        network_thread_.get(), worker_thread_.get(), signaling_thread_.get(),
        adm_.get() /* adm */, webrtc::CreateBuiltinAudioEncoderFactory(),
        webrtc::CreateBuiltinAudioDecoderFactory(),
        webrtc::CreateRaspiVideoEncoderFactory(),
        webrtc::CreateRaspiVideoDecoderFactory(), nullptr /* audio_mixer */,
        nullptr /* audio_processor */);
    if (!peer_connection_factory_.get()) {
        RTC_LOG(LS_ERROR) << __FUNCTION__
                          << "Failed to initialize PeerConnectionFactory";
        DeletePeerConnection();
        return false;
    }

    if (!CreatePeerConnection()) {
        RTC_LOG(LS_ERROR) << __FUNCTION__ << "CreatePeerConnection failed";
        DeletePeerConnection();
    }

    AddTracks();

    return peer_connection_.get() != nullptr;
}

void Streamer::UpdateMaxBitrate() {
    RTC_DCHECK(peer_connection_);

    auto senders = peer_connection_->GetSenders();
    for (const auto& sender : senders) {
        if (sender->media_type() == cricket::MediaType::MEDIA_TYPE_VIDEO &&
            sender->id().compare(kVideoLabel) == 0) {
            webrtc::RtpParameters parameters = sender->GetParameters();
            for (webrtc::RtpEncodingParameters& encoding :
                 parameters.encodings) {
                if (encoding.max_bitrate_bps) {
                    RTC_LOG(INFO)
                        << "Previous Max Bitrate Bps setting already exists: "
                        << *encoding.max_bitrate_bps;
                    RTC_LOG(INFO) << "Do not modifying Max Bitrate Bps";
                    return;
                } else {
                    encoding.max_bitrate_bps = absl::optional<int>(
                        ConfigMediaSingleton::Instance()->GetMaxBitrate());
                    RTC_LOG(INFO) << "Changing Max Bitrate Bps: "
                                  << *encoding.max_bitrate_bps;
                    sender->SetParameters(parameters);
                    return;
                }
            };
        };
    };
}

bool Streamer::CreatePeerConnection() {
    RTC_DCHECK(peer_connection_factory_);
    RTC_DCHECK(!peer_connection_);

    webrtc::PeerConnectionInterface::RTCConfiguration config;
    config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
    config.enable_dtls_srtp =
        streamer_config_->GetSrtpEnable();  // get Dtls config

    streamer_config_->GetIceTransportsType(config);
    streamer_config_->GetIceBundlePolicy(config);
    streamer_config_->GetIceRtcpMuxPolicy(config);

    streamer_config_->GetIceServers(config, true /* internal_config */);
    utils::PrintIceServers(config);

    peer_connection_ = peer_connection_factory_->CreatePeerConnection(
        config, nullptr, nullptr, this);

    return peer_connection_.get() != nullptr;
}

void Streamer::DeletePeerConnection() {
    adm_ = nullptr;
    peer_connection_ = nullptr;
    peer_connection_factory_ = nullptr;
    peer_id_ = -1;
}

//
// PeerConnectionObserver implementation.
//
void Streamer::OnAddTrack(
    rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
    const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>&
        streams) {
    RTC_LOG(INFO) << __FUNCTION__ << " " << receiver->id() << ", "
                  << receiver->media_type();
    receiver->track().release();
}

void Streamer::OnRemoveTrack(
    rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver) {
    RTC_LOG(INFO) << __FUNCTION__ << " " << receiver->id() << ", "
                  << receiver->media_type();
    receiver->track().release();
}

void Streamer::OnIceCandidate(const webrtc::IceCandidateInterface* candidate) {
    RTC_LOG(INFO) << __FUNCTION__ << " " << candidate->sdp_mline_index();

    Json::StyledWriter writer;
    Json::Value jmessage;

    // Names used for a Android Direct IceCandidate JSON object.
    jmessage[kCandidateType] = kCandidateSdpName;
    jmessage[kCandidateSdpMidName] = candidate->sdp_mid();
    jmessage[kCandidateSdpMlineIndexName] = candidate->sdp_mline_index();
    std::string sdp;
    if (!candidate->ToString(&sdp)) {
        RTC_LOG(LS_ERROR) << "Failed to serialize candidate";
        return;
    }
    jmessage[kCandidateSdpName] = sdp;
    SendMessage(writer.write(jmessage));
}

// PeerConnectionObserver event logging
void Streamer::OnSignalingChange(
    webrtc::PeerConnectionInterface::SignalingState new_state) {
    RTC_LOG(INFO) << "PeerConnectionObserver " << __FUNCTION__ << " "
                  << utils::SignalingStateToString(new_state);
}

void Streamer::OnStandardizedIceConnectionChange(
    webrtc::PeerConnectionInterface::IceConnectionState new_state) {
    RTC_LOG(INFO) << "PeerConnectionObserver " << __FUNCTION__ << " changed to "
                  << utils::PeerIceConnectionStateToString(new_state);
    ice_state_ = new_state;
    if (new_state == webrtc::PeerConnectionInterface::IceConnectionState::
                         kIceConnectionFailed &&
        peerconnection_state_ ==
            webrtc::PeerConnectionInterface::PeerConnectionState::kConnected) {
        RTC_LOG(LS_ERROR) << __FUNCTION__ << "Need to delete PeerConnection";
        ReportEvent(true /* drop_connection */, "IceConnection Failed");
        return;
    };
}

void Streamer::OnIceGatheringChange(
    webrtc::PeerConnectionInterface::IceGatheringState new_state) {
    RTC_LOG(INFO) << "PeerConnectionObserver " << __FUNCTION__ << " "
                  << utils::IceGatheringStateToString(new_state);
}

void Streamer::OnConnectionChange(
    webrtc::PeerConnectionInterface::PeerConnectionState new_state) {
    RTC_LOG(INFO) << "PeerConnectionObserver " << __FUNCTION__ << " "
                  << utils::PeerConnectionStateToString(new_state);
    peerconnection_state_ = new_state;
}

void Streamer::OnIceCandidateError(const std::string& host_candidate,
                                   const std::string& url, int error_code,
                                   const std::string& error_text) {
    RTC_LOG(INFO) << "PeerConnectionObserver " << __FUNCTION__
                  << " host candidate: " << host_candidate << ", url : " << url
                  << ", errorcode : " << error_code
                  << ", error_text : " << error_text;
}

void Streamer::OnIceCandidateError(const std::string& address, int port,
                                   const std::string& url, int error_code,
                                   const std::string& error_text) {
    RTC_LOG(INFO) << "PeerConnectionObserver " << __FUNCTION__
                  << " address: " << address << ", port : " << port
                  << ", url : " << url << ", errorcode : " << error_code
                  << ", error_text : " << error_text;
}

void Streamer::OnInterestingUsage(int usage_pattern) {
    RTC_LOG(INFO) << "PeerConnectionObserver " << __FUNCTION__
                  << " usage pattern: " << usage_pattern;
}

//
// StreamerObserver implementation.
//
void Streamer::OnPeerConnected(int peer_id, const std::string& name) {
    RTC_DCHECK(peer_id_ == -1);
    RTC_DCHECK(peer_id != -1);
    RTC_LOG(INFO) << __FUNCTION__ << "Peer " << peer_id << ", " << name.c_str()
                  << " connected, trying to initialize streamer instance";

    if (peer_connection_.get()) {
        RTC_LOG(LS_ERROR) << "We only support connecting to one peer at a time";
        return;
    }

    if (InitializePeerConnection()) {
        peer_id_ = peer_id;
        peer_connection_->CreateOffer(
            this, webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
    } else {
        RTC_LOG(LS_ERROR) << "Failed to initialize PeerConnection";
    }
}

void Streamer::OnPeerDisconnected(int peer_id) {
    RTC_LOG(INFO) << __FUNCTION__;
    if (peer_id == peer_id_ && peer_connection_.get()) {
        RTC_LOG(INFO) << "Peer disconnected";
        DeletePeerConnection();
    }
}

void Streamer::OnMessageFromPeer(int peer_id, const std::string& message) {
    RTC_CHECK(peer_id_ == peer_id || peer_id_ == -1);
    RTC_CHECK(!message.empty());

    if (!peer_connection_.get()) {
        RTC_CHECK(peer_id_ == -1);
        peer_id_ = peer_id;

        if (!InitializePeerConnection()) {
            RTC_LOG(LS_ERROR)
                << "Failed to initialize our PeerConnection instance";
            return;
        }
    } else if (peer_id != peer_id_) {
        RTC_DCHECK(peer_id_ != -1);
        RTC_LOG(WARNING)
            << "Received a message from unknown peer while already in a "
               "conversation with a different peer.";
        return;
    }

    Json::Reader reader;
    Json::Value jmessage;
    if (!reader.parse(message, jmessage)) {
        RTC_LOG(WARNING) << "Received unknown message. " << message;
        return;
    }

    std::string type_str;
    std::string json_object;
    rtc::GetStringFromJsonObject(jmessage, kSessionDescriptionTypeName,
                                 &type_str);
    if (!type_str.empty()) {
        absl::optional<webrtc::SdpType> type_maybe =
            webrtc::SdpTypeFromString(type_str);
        //  SdpTypeFromString will only care about kOffer/PrAnswer/kAnswer
        if (!type_maybe) {
            // candidate message from peer
            std::string candidate_mid;
            int candidate_mlineindex = 0;
            std::string candidate_message;

            if (!rtc::GetStringFromJsonObject(jmessage, kCandidateSdpMidName,
                                              &candidate_mid) ||
                !rtc::GetIntFromJsonObject(jmessage,
                                           kCandidateSdpMlineIndexName,
                                           &candidate_mlineindex) ||
                !rtc::GetStringFromJsonObject(jmessage, kCandidateSdpName,
                                              &candidate_message)) {
                RTC_LOG(LS_ERROR)
                    << "Can't parse received message." << jmessage;
                return;
            }

            webrtc::SdpParseError error;
            std::unique_ptr<webrtc::IceCandidateInterface> candidate(
                webrtc::CreateIceCandidate(candidate_mid, candidate_mlineindex,
                                           candidate_message, &error));
            if (!candidate.get()) {
                RTC_LOG(WARNING) << "Can't parse received candidate message. "
                                 << "SdpParseError was: " << error.description;
                return;
            }
            if (!peer_connection_->AddIceCandidate(candidate.get())) {
                RTC_LOG(WARNING) << "Failed to apply the received candidate";
                return;
            }
            RTC_LOG(INFO) << " Received candidate :" << message;
            return;
        }
        webrtc::SdpType type = *type_maybe;
        std::string sdp;
        if (!rtc::GetStringFromJsonObject(jmessage, kSessionDescriptionSdpName,
                                          &sdp)) {
            RTC_LOG(WARNING)
                << "Can't parse received session description message.";
            RTC_LOG(WARNING) << ":" << message << ":";
            return;
        }

        webrtc::SdpParseError error;
        std::unique_ptr<webrtc::SessionDescriptionInterface>
            session_description =
                webrtc::CreateSessionDescription(type, sdp, &error);
        if (!session_description) {
            RTC_LOG(WARNING)
                << "Can't parse received session description message. "
                << "SdpParseError was: " << error.description;
            return;
        }

        RTC_LOG(INFO) << " Received session description : \"" << message
                      << "\"";
        peer_connection_->SetRemoteDescription(
            DummySetSessionDescriptionObserver::Create(),
            session_description.release());

        if (type == webrtc::SdpType::kOffer) {
            peer_connection_->CreateAnswer(
                this, webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
        }

        // Update the max bitrate on RTPSender if there is no SDP negotiation
        // of max bitrate between client.
        UpdateMaxBitrate();
        return;
    }
    RTC_LOG(LS_ERROR) << "Message type is unknown : " << jmessage;
}

void Streamer::OnMessageSent(int err) {
    RTC_LOG(INFO) << __FUNCTION__ << "Message Sent result : " << err;
}

void Streamer::AddTracks() {
    if (!peer_connection_->GetSenders().empty()) {
        return;  // Already added.
    }

    // media configuration sigleton reference
    ConfigMedia* config_media = ConfigMediaSingleton::Instance();

    if (streamer_config_->GetAudioEnable() == true) {
        cricket::AudioOptions options;
        if (config_media->GetAudioProcessing() == true) {
            if (config_media->GetAudioEchoCancel() == true)
                options.echo_cancellation = absl::optional<bool>(true);
            if (config_media->GetAudioEchoCancel() == true)
                options.auto_gain_control = absl::optional<bool>(true);
            if (config_media->GetAudioHighPassFilter() == true)
                options.highpass_filter = absl::optional<bool>(true);
            if (config_media->GetAudioNoiseSuppression() == true)
                options.noise_suppression = absl::optional<bool>(true);
        } else {
            options.echo_cancellation = absl::optional<bool>(false);
            options.auto_gain_control = absl::optional<bool>(false);
            options.highpass_filter = absl::optional<bool>(false);
            options.noise_suppression = absl::optional<bool>(false);
        }

        RTC_LOG(INFO) << "Media Config Audio options: " << options.ToString();

        rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track(
            peer_connection_factory_->CreateAudioTrack(
                kAudioLabel,
                peer_connection_factory_->CreateAudioSource(options)));
        auto result_or_error =
            peer_connection_->AddTrack(audio_track, {kStreamId});
        if (!result_or_error.ok()) {
            RTC_LOG(LS_ERROR) << "Failed to add audio track to PeerConnection: "
                              << result_or_error.error().message();
        }
    }

    if (streamer_config_->GetVideoEnable() == true) {
        video_track_sources_.clear();
        video_track_sources_.emplace_back(
            webrtc::FakeVideoTrackSource::Create(false));
        rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track_(
            peer_connection_factory_->CreateVideoTrack(
                kVideoLabel, video_track_sources_.back()));

        auto result_or_error =
            peer_connection_->AddTrack(video_track_, {kStreamId});
        if (!result_or_error.ok()) {
            RTC_LOG(LS_ERROR) << "Failed to add video track to PeerConnection: "
                              << result_or_error.error().message();
        }
    }
}

void Streamer::OnSuccess(webrtc::SessionDescriptionInterface* desc) {
    peer_connection_->SetLocalDescription(
        DummySetSessionDescriptionObserver::Create(), desc);

    std::string sdp;
    desc->ToString(&sdp);

    Json::StyledWriter writer;
    Json::Value jmessage;
    jmessage[kSessionDescriptionTypeName] =
        webrtc::SdpTypeToString(desc->GetType());
    jmessage[kSessionDescriptionSdpName] = sdp;
    SendMessage(writer.write(jmessage));
}

void Streamer::OnFailure(webrtc::RTCError error) {
    RTC_LOG(LERROR) << ToString(error.type()) << ": " << error.message();
}

void Streamer::SendMessage(const std::string& json_object) {
    RTC_LOG(INFO) << "Sending Message to Peer: " << json_object.c_str();
    session_->SendMessageToPeer(peer_id_, json_object);
}

void Streamer::ReportEvent(bool drop_connection, const std::string message) {
    RTC_LOG(INFO) << "Sending Event Message to Peer: " << message;
    session_->ReportEvent(peer_id_, drop_connection, message);
}
