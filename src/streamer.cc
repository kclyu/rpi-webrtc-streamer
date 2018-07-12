/*
 *  Copyright (c) 2016, rpi-webrtc-streamer  Lyu,KeunChang
 *
 * streamer.cc
 *
 * Modified version of src/webrtc/examples/peer/client/conductor.cc in WebRTC source tree
 * The origianl copyright info below.
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

#include <memory>
#include <utility>
#include <vector>

#include "rtc_base/json.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "api/mediaconstraintsinterface.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "media/engine/webrtcvideodecoderfactory.h"
#include "media/engine/webrtcvideoencoderfactory.h"
#include "media/engine/webrtcvideocapturerfactory.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/video_capture/video_capture_factory.h"
#include "modules/audio_processing/include/audio_processing.h"

#include "api/audio/audio_mixer.h"
#include "api/rtpsenderinterface.h"
#include "api/test/fakeconstraints.h"
#include "media/base/fakevideocapturer.h"

#include "streamer.h"
#include "streamer_observer.h"
#include "config_streamer.h"
#include "config_media.h"

#include "raspi_encoder.h"
#include "raspi_encoder_impl.h"
#include "raspi_decoder.h"
#include "raspi_decoder_dummy.h"


using webrtc::PeerConnectionInterface;


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

// Max Bitrate
static const int kDefaultMaxBitrate = 3500000;

class DummySetSessionDescriptionObserver
    : public webrtc::SetSessionDescriptionObserver {
public:
    static DummySetSessionDescriptionObserver* Create() {
        return
            new rtc::RefCountedObject<DummySetSessionDescriptionObserver>();
    }
    virtual void OnSuccess() { RTC_LOG(INFO) << __FUNCTION__; }
    virtual void OnFailure(const std::string& error) {
        RTC_LOG(INFO) << __FUNCTION__ << " " << error;
    }

protected:
    DummySetSessionDescriptionObserver() {}
    ~DummySetSessionDescriptionObserver() {}
};

Streamer::Streamer(SocketServerObserver *session, StreamerConfig *config)
    : peer_id_(-1) {
    RTC_DCHECK(session != nullptr);
    session_ = session;
    session->RegisterObserver(this);
    streamer_config_ = config;
}

Streamer::~Streamer() {
    RTC_DCHECK(!peer_connection_);
}

bool Streamer::connection_active() const {
    return peer_connection_ != nullptr;
}

void Streamer::Close() {
    // Reset session active session
    DeletePeerConnection();
}

bool Streamer::InitializePeerConnection() {
    RTC_DCHECK(!peer_connection_factory_);
    RTC_DCHECK(!peer_connection_);

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

    rtc::scoped_refptr<webrtc::AudioMixer> audio_mixer = nullptr;
    rtc::scoped_refptr<webrtc::AudioProcessing> audio_processor = nullptr;

    auto audio_encoder_factory = webrtc::CreateBuiltinAudioEncoderFactory();
    auto audio_decoder_factory = webrtc::CreateBuiltinAudioDecoderFactory();

    std::unique_ptr<webrtc::VideoDecoderFactory> video_decoder_factory =
        std::unique_ptr<webrtc::VideoDecoderFactory>
        (webrtc::RaspiVideoDecoderFactory::CreateVideoDecoderFactory());
    std::unique_ptr<webrtc::VideoEncoderFactory> video_encoder_factory =
        std::unique_ptr<webrtc::VideoEncoderFactory>
        (webrtc::RaspiVideoEncoderFactory::CreateVideoEncoderFactory());

    peer_connection_factory_  = webrtc::CreatePeerConnectionFactory(
            network_thread_.get(), worker_thread_.get(),
            signaling_thread_.get(),
            nullptr /* adm */,
            audio_encoder_factory, audio_decoder_factory,
            std::move(video_encoder_factory), std::move(video_decoder_factory),
            audio_mixer, audio_processor );
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
    AddStreams();

    return peer_connection_.get() != nullptr;
}

void Streamer::UpdateMaxBitrate() {
    RTC_DCHECK(peer_connection_);
    auto senders = peer_connection_->GetSenders();
    for (const auto& sender : senders) {
        if( sender->media_type() == cricket::MediaType::MEDIA_TYPE_VIDEO &&
            sender->id().compare(kVideoLabel) == 0) {
            webrtc::RtpParameters parameters =  sender->GetParameters();
            for (webrtc::RtpEncodingParameters& encoding : parameters.encodings) {
                 if (encoding.max_bitrate_bps) {
                     RTC_LOG(INFO) << "Previous Max Bitrate Bps setting already exists: "
                         << *encoding.max_bitrate_bps;
                     RTC_LOG(INFO) << "Do not modifying Max Bitrate Bps";
                    return;
                 }
                 else {
                     encoding.max_bitrate_bps
                         = rtc::Optional<int>(
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
    // config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
    // config.enable_dtls_srtp = dtls;
    webrtc::PeerConnectionInterface::IceServer stun_server;
    webrtc::PeerConnectionInterface::IceServer turn_server;

    if(streamer_config_->GetStunServer(stun_server) == true ) {
        config.servers.push_back(stun_server);
    }

    if(streamer_config_->GetTurnServer(turn_server) == true ) {
        config.servers.push_back(turn_server);
    }

    peer_connection_ = peer_connection_factory_->CreatePeerConnection(
            config, nullptr, nullptr, this);

    return peer_connection_.get() != nullptr;
}

void Streamer::DeletePeerConnection() {
    // Reset active stream session
    active_streams_.clear();
    peer_connection_ = nullptr;
    peer_connection_factory_ = nullptr;
    peer_id_ = -1;
}

//
// PeerConnectionObserver implementation.
//

// Called when a remote stream is added
void Streamer::OnAddStream(
        rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) {
    RTC_LOG(INFO) << __FUNCTION__ << "Remote Stream added!" << stream->id();
}

void Streamer::OnRemoveStream(
        rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) {
    RTC_LOG(INFO) << __FUNCTION__ << "Remote Stream removed!" << stream->id();
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

//
// StreamerObserver implementation.
//
void Streamer::OnPeerConnected(int peer_id, const std::string& name) {
    RTC_DCHECK(peer_id_ == -1);
    RTC_DCHECK(peer_id != -1);
    RTC_LOG(INFO) << __FUNCTION__ << "Peer " << peer_id  << ", " << name.c_str()
              << " connected, trying to initialize streamer instance";

    if (peer_connection_.get()) {
        RTC_LOG(LS_ERROR) << "We only support connecting to one peer at a time";
        return;
    }

    // Trying to receive audio, but not video for RWS.
    // RWS does not supports video receving
    webrtc::PeerConnectionInterface::RTCOfferAnswerOptions offer_options;
    offer_options.offer_to_receive_video = 0;   // do not receive video
    offer_options.offer_to_receive_audio = 1;   // receive audio

    if (InitializePeerConnection()) {
        peer_id_ = peer_id;
        peer_connection_->CreateOffer(this, offer_options);
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
            RTC_LOG(LS_ERROR) << "Failed to initialize our PeerConnection instance";
            return;
        }
    } else if (peer_id != peer_id_) {
        RTC_DCHECK(peer_id_ != -1);
        RTC_LOG(WARNING) << "Received a message from unknown peer while already in a "
                     "conversation with a different peer.";
        return;
    }

    Json::Reader reader;
    Json::Value jmessage;
    if (!reader.parse(message, jmessage)) {
        RTC_LOG(WARNING) << "Received unknown message. " << message;
        return;
    }
    std::string sdp;
    std::string json_object;

    rtc::GetStringFromJsonObject(jmessage, kSessionDescriptionSdpName, &sdp);
    if (!sdp.empty()) {
        std::string sdp;
        if (!rtc::GetStringFromJsonObject(jmessage, kSessionDescriptionSdpName,
                                          &sdp)) {
            RTC_LOG(WARNING) << "Can't parse received session description message.";
            RTC_LOG(WARNING) << ":" << message << ":";
            return;
        }
        std::string type;
        rtc::GetStringFromJsonObject(jmessage, kSessionDescriptionTypeName, &type);

        webrtc::SdpParseError error;
        webrtc::SessionDescriptionInterface* session_description(
            webrtc::CreateSessionDescription(type, sdp, &error));
        if (!session_description) {
            RTC_LOG(WARNING) << "Can't parse received session description message. "
                         << "SdpParseError was: " << error.description;
            return;
        }
        RTC_LOG(INFO) << " Received session description : \"" << message << "\"";
        peer_connection_->SetRemoteDescription(
            DummySetSessionDescriptionObserver::Create(), session_description);

        // Trying to receive audio, but not video for RWS.
        // RWS does not supports video receving
        webrtc::PeerConnectionInterface::RTCOfferAnswerOptions offer_options;
        offer_options.offer_to_receive_video = 0;
        offer_options.offer_to_receive_audio = 1;

        // TODO(kclyu) invalid sdp negotiation makes session_description
        // null, need to figure it out how to fix it.
        if (session_description->type() ==
                webrtc::SessionDescriptionInterface::kOffer) {
            peer_connection_->CreateAnswer(this, offer_options);
        }

        // Update the max bitrate on RTPSender if there is no SDP negotiation
        // of max bitrate between client.
        UpdateMaxBitrate();
        return;
    } else {
        std::string sdp_mid;
        int sdp_mlineindex = 0;
        std::string sdp;

        if (!rtc::GetStringFromJsonObject(jmessage, kCandidateSdpMidName,
                                          &sdp_mid) ||
                !rtc::GetIntFromJsonObject(jmessage, kCandidateSdpMlineIndexName,
                                           &sdp_mlineindex) ||
                !rtc::GetStringFromJsonObject(jmessage, kCandidateSdpName, &sdp)) {
            RTC_LOG(WARNING) << "Can't parse received message.";
            return;
        }

        webrtc::SdpParseError error;
        std::unique_ptr<webrtc::IceCandidateInterface> candidate(
            webrtc::CreateIceCandidate(sdp_mid, sdp_mlineindex, sdp, &error));
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
}

void Streamer::OnMessageSent(int err) {
    RTC_LOG(INFO) << __FUNCTION__ << "Message Sent result : " << err;
}

std::unique_ptr<cricket::VideoCapturer> Streamer::OpenVideoCaptureDevice() {

    std::vector<std::string> device_names;
    {
        std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> info(
            webrtc::VideoCaptureFactory::CreateDeviceInfo());
        if (!info) {
            return nullptr;
        }
        int num_devices = info->NumberOfDevices();
        for (int i = 0; i < num_devices; ++i) {
            const uint32_t kSize = 256;
            char name[kSize] = {0};
            char id[kSize] = {0};
            if (info->GetDeviceName(i, name, kSize, id, kSize) != -1) {
                device_names.push_back(name);
            }
        }
    }

    cricket::WebRtcVideoDeviceCapturerFactory factory;
    std::unique_ptr<cricket::VideoCapturer> capturer;
    for (const auto& name : device_names) {
        capturer = factory.Create(cricket::Device(name, 0));
        if (capturer) {
            break;
        }
    }
    return capturer;
}

void Streamer::AddStreams() {
    if (active_streams_.find(kStreamId) != active_streams_.end())
        return;  // Already added.

    // media configuration sigleton reference
    ConfigMedia *config_media = ConfigMediaSingleton::Instance();

    cricket::AudioOptions options;
    if( config_media->GetAudioProcessing()  == true ) {
        if( config_media->GetAudioEchoCancel() == true )
            options.echo_cancellation = rtc::Optional<bool>(true);
        if( config_media->GetAudioEchoCancel()  == true )
            options.auto_gain_control = rtc::Optional<bool>(true);
        if( config_media->GetAudioHighPassFilter() == true )
            options.highpass_filter = rtc::Optional<bool>(true);
        if( config_media->GetAudioNoiseSuppression() == true )
            options.noise_suppression = rtc::Optional<bool>(true);
    }
    else {
        options.echo_cancellation = rtc::Optional<bool>(false);
        options.auto_gain_control = rtc::Optional<bool>(false);
        options.highpass_filter = rtc::Optional<bool>(false);
        options.noise_suppression = rtc::Optional<bool>(false);
    }

    // audio_level_control is removed
    // if( config_media::audio_level_control == true )
    //         options.level_control = rtc::Optional<bool>(true);

    RTC_LOG(INFO) << "Audio options: " << options.ToString();

    rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track(
        peer_connection_factory_->CreateAudioTrack(
            kAudioLabel, peer_connection_factory_->CreateAudioSource(options)));

    rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track(
        peer_connection_factory_->CreateVideoTrack(
            kVideoLabel,
            peer_connection_factory_->CreateVideoSource(new cricket::FakeVideoCapturer(false),
                    nullptr)));

    rtc::scoped_refptr<webrtc::MediaStreamInterface> stream =
        peer_connection_factory_->CreateLocalMediaStream(kStreamId);

    stream->AddTrack(audio_track);
    stream->AddTrack(video_track);
    if (!peer_connection_->AddStream(stream)) {
        RTC_LOG(LS_ERROR) << "Adding stream to PeerConnection failed";
    }

    typedef std::pair<std::string,
            rtc::scoped_refptr<webrtc::MediaStreamInterface> >
            MediaStreamPair;
    active_streams_.insert(MediaStreamPair(stream->id(), stream));
}


void Streamer::OnSuccess(webrtc::SessionDescriptionInterface* desc) {
    peer_connection_->SetLocalDescription(
        DummySetSessionDescriptionObserver::Create(), desc);

    std::string sdp;
    desc->ToString(&sdp);

    Json::StyledWriter writer;
    Json::Value jmessage;
    jmessage[kSessionDescriptionTypeName] = desc->type();
    jmessage[kSessionDescriptionSdpName] = sdp;
    SendMessage(writer.write(jmessage));
}

void Streamer::OnFailure(const std::string& error) {
    RTC_LOG(LERROR) << error;
}

void Streamer::SendMessage(const std::string& json_object) {
    RTC_LOG(INFO) << "Sending Message to Peer: " << json_object.c_str();
    session_->SendMessageToPeer( peer_id_, json_object );
}


