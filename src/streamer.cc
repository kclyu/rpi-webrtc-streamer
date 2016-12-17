/*
 *  Copyright (c) 2016, rpi-webrtc-streamer  Lyu,KeunChang
 *
 * streamer.cc
 *
 * Modified version of webrtc/src/webrtc/examples/peer/client/peer_connection.cc in WebRTC source tree
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

#include "webrtc/base/common.h"
#include "webrtc/base/json.h"
#include "webrtc/base/logging.h"
#include "webrtc/api/mediaconstraintsinterface.h"
#include "webrtc/media/engine/webrtcvideodecoderfactory.h"
#include "webrtc/media/engine/webrtcvideoencoderfactory.h"
#include "webrtc/media/engine/webrtcvideocapturerfactory.h"
#include "webrtc/modules/video_capture/video_capture_factory.h"
#include "webrtc/media/base/fakevideocapturer.h"

#include "webrtc/system_wrappers/include/trace.h"

#include "clientconstraints.h"

#include "dummyvideocapturer.h"
#include "raspi_encoder.h"
#include "streamer.h"
#include "streamer_observer.h"
#include "stream_data_sockets.h"
#include "streamer_defaults.h"


using webrtc::PeerConnectionInterface;

// Names used for a Android Direct IceCandidate JSON object.
const char kCandidateSdpMidName[] = "id";
const char kCandidateSdpMlineIndexName[] = "label";
const char kCandidateSdpName[] = "candidate";
const char kCandidateType[] = "type";

// Names used for a SessionDescription JSON object.
const char kSessionDescriptionTypeName[] = "type";
const char kSessionDescriptionSdpName[] = "sdp";

// DTLS enable flags
#define DTLS_ON  true
#define DTLS_OFF false

const bool dtls_enable = DTLS_ON;


class DummySetSessionDescriptionObserver : public webrtc::SetSessionDescriptionObserver {
public:
    static DummySetSessionDescriptionObserver* Create() {
        return
            new rtc::RefCountedObject<DummySetSessionDescriptionObserver>();
    }
    virtual void OnSuccess() {
        LOG(INFO) << __FUNCTION__;
    }
    virtual void OnFailure(const std::string& error) {
        LOG(INFO) << __FUNCTION__ << " " << error;
    }

protected:
    DummySetSessionDescriptionObserver() {}
    ~DummySetSessionDescriptionObserver() {}
};

Streamer::Streamer(SocketServerObserver *session)
    : peer_id_(-1) {
    ASSERT(session != nullptr);
    session_ = session;
    dtls_enable_ = GetDTLSEnableBool();
    session->RegisterObserver(this);
}

Streamer::~Streamer() {
    ASSERT(peer_connection_.get() == NULL);
}

bool Streamer::connection_active() const {
    return peer_connection_.get() != NULL;
}

void Streamer::Close() {
    // Reset session active session
    DeletePeerConnection();
}

bool Streamer::InitializePeerConnection() {
    ASSERT(peer_connection_factory_.get() == NULL);
    ASSERT(peer_connection_.get() == NULL);

    rtc::ThreadManager::Instance()->WrapCurrentThread();
    webrtc::Trace::CreateTrace();
    rtc::Thread* worker_thread = new rtc::Thread();
    worker_thread->SetName("worker_thread", NULL);
    rtc::Thread* signaling_thread = new rtc::Thread();
    signaling_thread->SetName("signaling_thread", NULL);
    RTC_CHECK(worker_thread->Start() && signaling_thread->Start())
            << "Failed to start threads";

    cricket::WebRtcVideoEncoderFactory* encoder_factory = nullptr;
    encoder_factory = new webrtc::MMALVideoEncoderFactory;
    peer_connection_factory_  = webrtc::CreatePeerConnectionFactory(
                                    worker_thread, signaling_thread, NULL, 
                                    encoder_factory, NULL );

    if (!peer_connection_factory_.get()) {
        LOG(LS_ERROR) << __FUNCTION__ << "Failed to initialize PeerConnectionFactory";
        DeletePeerConnection();
        return false;
    }

    if (!CreatePeerConnection()) {
        LOG(LS_ERROR) << __FUNCTION__ << "CreatePeerConnection failed";
        DeletePeerConnection();
    }
    AddStreams();
    return peer_connection_.get() != NULL;
}

bool Streamer::CreatePeerConnection() {
    ASSERT(peer_connection_factory_.get() != NULL);
    ASSERT(peer_connection_.get() == NULL);

    PeerConnectionInterface::RTCConfiguration rtcConfig;
    PeerConnectionInterface::IceServer server;

    // STUN server
    server.uri = GetPeerConnectionString();
    rtcConfig.servers.push_back(server);

    // RTCConfiguration
    rtcConfig.type =  PeerConnectionInterface::IceTransportsType::kAll;
    rtcConfig.bundle_policy =  PeerConnectionInterface::kBundlePolicyBalanced;
    rtcConfig.rtcp_mux_policy = PeerConnectionInterface::kRtcpMuxPolicyNegotiate;
    rtcConfig.tcp_candidate_policy = PeerConnectionInterface::kTcpCandidatePolicyDisabled;
    rtcConfig.continual_gathering_policy = PeerConnectionInterface::GATHER_ONCE;
    rtcConfig.audio_jitter_buffer_max_packets = 50;
    rtcConfig.audio_jitter_buffer_fast_accelerate = false;
    rtcConfig.ice_connection_receiving_timeout = -1;
    rtcConfig.ice_backup_candidate_pair_ping_interval = -1;

    webrtc::ClientConstraints pcConstraints;
    if (dtls_enable) {
        pcConstraints.AddOptional(webrtc::MediaConstraintsInterface::kEnableDtlsSrtp,
                                  "true");
    } else {
        pcConstraints.AddOptional(webrtc::MediaConstraintsInterface::kEnableDtlsSrtp,
                                  "false");
    }

    peer_connection_ = peer_connection_factory_->CreatePeerConnection(
                           rtcConfig, &pcConstraints, NULL, NULL, this);
    return peer_connection_.get() != NULL;
}

void Streamer::DeletePeerConnection() {
    // Reset active stream session
    peer_connection_ = NULL;
    active_streams_.clear();
    peer_connection_factory_ = NULL;
    peer_id_ = -1;
}

//
// PeerConnectionObserver implementation.
//

// Called when a remote stream is added
void Streamer::OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) {
    LOG(INFO) << __FUNCTION__ << " " << stream->label();
    LOG(LS_ERROR) << __FUNCTION__ << "Remote Stream added!" << stream->label();
}

void Streamer::OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) {
    LOG(INFO) << __FUNCTION__ << " " << stream->label();
    LOG(LS_ERROR) << __FUNCTION__ << "Remote Stream removed!" << stream->label();
}

void Streamer::OnIceCandidate(const webrtc::IceCandidateInterface* candidate) {
    LOG(INFO) << __FUNCTION__ << " " << candidate->sdp_mline_index();

    Json::StyledWriter writer;
    Json::Value jmessage;


    // Names used for a Android Direct IceCandidate JSON object.
    jmessage[kCandidateType] = kCandidateSdpName;
    jmessage[kCandidateSdpMidName] = candidate->sdp_mid();
    jmessage[kCandidateSdpMlineIndexName] = candidate->sdp_mline_index();
    std::string sdp;
    if (!candidate->ToString(&sdp)) {
        LOG(LS_ERROR) << "Failed to serialize candidate";
        return;
    }
    jmessage[kCandidateSdpName] = sdp;
    SendMessage(writer.write(jmessage));
}

//
// StreamerObserver implementation.
//
void Streamer::OnPeerConnected(int peer_id, const std::string& name) {
    RTC_CHECK(peer_id_ == -1);
    RTC_CHECK(peer_id != -1);
    LOG(INFO) << __FUNCTION__ << "Peer " << peer_id  << ", " << name.c_str()
              << " connected, trying to initialize streamer instance";

    if (peer_connection_.get()) {
        LOG(LS_ERROR) << "We only support connecting to one peer at a time";
        return;
    }

    if (InitializePeerConnection()) {
        peer_id_ = peer_id;
        peer_connection_->CreateOffer(this, NULL);
    } else {
        LOG(LS_ERROR) << "Failed to initialize PeerConnection";
    }
}

void Streamer::OnPeerDisconnected(int peer_id) {
    LOG(INFO) << __FUNCTION__;
    if (peer_id == peer_id_ && peer_connection_.get()) {
        LOG(INFO) << "Peer disconnected";
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
            LOG(LS_ERROR) << "Failed to initialize our PeerConnection instance";
            // Reset the active stream session
            // TODO
            // client_->SignOut();
            return;
        }
    } else if (peer_id != peer_id_) {
        ASSERT(peer_id_ != -1);
        LOG(WARNING) << "Received a message from unknown peer while already in a "
                     "conversation with a different peer.";
        return;
    }

    Json::Reader reader;
    Json::Value jmessage;
    if (!reader.parse(message, jmessage)) {
        LOG(WARNING) << "Received unknown message. " << message;
        return;
    }
    std::string type;
    std::string json_object;

    rtc::GetStringFromJsonObject(jmessage, kSessionDescriptionTypeName, &type);
    if (!type.empty()) {
        std::string sdp;
        if (!rtc::GetStringFromJsonObject(jmessage, kSessionDescriptionSdpName,
                                          &sdp)) {
            LOG(WARNING) << "Can't parse received session description message.";
            return;
        }
        webrtc::SdpParseError error;
        webrtc::SessionDescriptionInterface* session_description(
            webrtc::CreateSessionDescription(type, sdp, &error));
        if (!session_description) {
            LOG(WARNING) << "Can't parse received session description message. "
                         << "SdpParseError was: " << error.description;
            return;
        }
        LOG(INFO) << " Received session description : \"" << message << "\"";
        peer_connection_->SetRemoteDescription(
            DummySetSessionDescriptionObserver::Create(), session_description);

        webrtc::ClientConstraints sdpConstraints;
        sdpConstraints.SetMandatoryReceiveAudio(true);
        sdpConstraints.SetMandatoryReceiveVideo(false);
        if (session_description->type() ==
                webrtc::SessionDescriptionInterface::kOffer) {
            peer_connection_->CreateAnswer(this, &sdpConstraints);
        }
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
            LOG(WARNING) << "Can't parse received message.";
            return;
        }

        webrtc::SdpParseError error;
        std::unique_ptr<webrtc::IceCandidateInterface> candidate(
            webrtc::CreateIceCandidate(sdp_mid, sdp_mlineindex, sdp, &error));
        if (!candidate.get()) {
            LOG(WARNING) << "Can't parse received candidate message. "
                         << "SdpParseError was: " << error.description;
            return;
        }
        if (!peer_connection_->AddIceCandidate(candidate.get())) {
            LOG(WARNING) << "Failed to apply the received candidate";
            return;
        }
        LOG(INFO) << " Received candidate :" << message;
        return;
    }
}

void Streamer::OnMessageSent(int err) {
    LOG(INFO) << __FUNCTION__ << "Message Sent result : " << err;
}

cricket::VideoCapturer* Streamer::OpenVideoCaptureDevice() {
    webrtc::Trace::CreateTrace();

#ifdef __0

    FakeWebRtcDeviceInfo fake_device_info;
    const std::string device_name("raspi_cam");
    const std::string device_id("0");
    fake_device_info.AddDevice( device_name, device_id);

    {
        int sizes = 8 ;
        unsigned int size[][2] = { { 352, 288 }, { 640, 480 },
            { 704, 576 }, { 800, 600 }, { 960, 720 },
            { 1280, 720 }, { 1024, 768 }, { 1920, 1080 }
        };

        for ( int i = 0; i < sizes; i++ ) {
            webrtc::VideoCaptureCapability cap;

            cap.width = size[i][0];
            cap.height = size[i][1];
            cap.rawType = webrtc::RawVideoType::kVideoI420;
            cap.expectedCaptureDelay = 120;
            cap.maxFPS = 30;
            fake_device_info.AddCapability( device_id, cap);
        }
    }
#endif

    std::vector<std::string> device_names;
    {
        std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> info(
            webrtc::VideoCaptureFactory::CreateDeviceInfo(0));
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
    cricket::VideoCapturer* capturer = nullptr;
    for (const auto& name : device_names) {
        capturer = factory.Create(cricket::Device(name, 0));
        if (capturer) {
            break;
        }
    }
    return capturer;
}

void Streamer::AddStreams() {
    if (active_streams_.find(kStreamLabel) != active_streams_.end())
        return;  // Already added.

    webrtc::ClientConstraints audioConstraints;
    audioConstraints.SetMandatoryEchoCancellation(false);
    audioConstraints.SetMandatoryAudoGainControl(false);
    audioConstraints.SetMandatoryNoiseSuppression(false);
    audioConstraints.SetMandatoryHighpassFilter(false);

    rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track(
        peer_connection_factory_->CreateAudioTrack(
            kAudioLabel, peer_connection_factory_->CreateAudioSource(&audioConstraints)));

    webrtc::ClientConstraints videoConstraints;
    //videoConstraints.SetMandatoryMaxWidth(1280);
    //videoConstraints.SetMandatoryMaxHeight(1024);
    videoConstraints.SetMandatoryMaxWidth(640);
    videoConstraints.SetMandatoryMaxHeight(480);
    videoConstraints.SetMandatoryMinWidth(320);
    videoConstraints.SetMandatoryMinHeight(240);
    videoConstraints.SetMandatoryMaxFrameRate(30);
    videoConstraints.SetMandatoryMinFrameRate(15);

#ifdef _0
    rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track(
        peer_connection_factory_->CreateVideoTrack(
            kVideoLabel,
            peer_connection_factory_->CreateVideoSource(OpenVideoCaptureDevice(),
                    &videoConstraints)));
#endif

    rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track(
        peer_connection_factory_->CreateVideoTrack(
            kVideoLabel,
            peer_connection_factory_->CreateVideoSource(new cricket::DummyVideoCapturer(false),
                    &videoConstraints)));

    rtc::scoped_refptr<webrtc::MediaStreamInterface> stream =
        peer_connection_factory_->CreateLocalMediaStream(kStreamLabel);

    stream->AddTrack(audio_track);
    stream->AddTrack(video_track);
    if (!peer_connection_->AddStream(stream)) {
        LOG(LS_ERROR) << "Adding stream to PeerConnection failed";
    }
    typedef std::pair<std::string,
            rtc::scoped_refptr<webrtc::MediaStreamInterface> >
            MediaStreamPair;
    active_streams_.insert(MediaStreamPair(stream->label(), stream));
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
    LOG(LERROR) << error;
}

void Streamer::SendMessage(const std::string& json_object) {
    LOG(INFO) << "Sending Message to Peer: " << json_object.c_str();
    session_->SendMessageToPeer( peer_id_, json_object );
    //
}


