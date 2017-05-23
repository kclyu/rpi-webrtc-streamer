/*
 *  Copyright (c) 2016, rpi-webrtc-streamer  Lyu,KeunChang
 *
 * stream.h
 *
 * Modified version of webrtc/src/webrtc/examples/peer/client/peer_connection.h 
 * in WebRTC source tree
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

#ifndef RPI_STREAMER_H_
#define RPI_STREAMER_H_
#pragma once

#include <deque>
#include <map>
#include <set>
#include <string>

#include "webrtc/api/mediastreaminterface.h"
#include "webrtc/api/mediaconstraintsinterface.h"
#include "webrtc/api/peerconnectioninterface.h"
#include "streamer_observer.h"
#include "streamer_config.h"


namespace webrtc {
class VideoCaptureModule;
}  // namespace webrtc


class Streamer
    : public webrtc::PeerConnectionObserver, 
      public webrtc::CreateSessionDescriptionObserver,
      public StreamerObserver
{
public:
    Streamer(SocketServerObserver *session, StreamerConfig *config );
    void AddObserver(SocketServerObserver *session);
    bool connection_active() const;
    virtual void Close();

protected:
    ~Streamer();
    bool InitializePeerConnection();
    bool CreatePeerConnection();
    void DeletePeerConnection();
    void AddStreams();
    std::unique_ptr<cricket::VideoCapturer> OpenVideoCaptureDevice();

    //
    // PeerConnectionObserver implementation.
    //
    void OnSignalingChange(
        webrtc::PeerConnectionInterface::SignalingState new_state) override {};
    void OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override;
    void OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override;
    void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> channel) override {}
    void OnRenegotiationNeeded() override {}
    void OnIceConnectionChange(
        webrtc::PeerConnectionInterface::IceConnectionState new_state) override {};
    void OnIceGatheringChange(
        webrtc::PeerConnectionInterface::IceGatheringState new_state) override {};
    void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override;
    void OnIceConnectionReceivingChange(bool receiving) override {}

    //
    // CreateSessionDescriptionObserver implementation.
    //
    virtual void OnSuccess(webrtc::SessionDescriptionInterface* desc) override;
    virtual void OnFailure(const std::string& error) override;

    //
    // StreamerSessionObserver implementation.
    //
    virtual void OnPeerConnected(int peer_id, const std::string& name) override;
    virtual void OnPeerDisconnected(int peer_id) override;
    virtual void OnMessageFromPeer(int peer_id, const std::string& message) override;
    virtual void OnMessageSent(int err) override;

private:
    // Send a message to the remote peer.
    void SendMessage(const std::string& json_object);

    // Change the max_bitrate in RtpSender
    void UpdateMaxBitrate();

    int peer_id_;
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
    rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>
        peer_connection_factory_;
    std::map<std::string, rtc::scoped_refptr<webrtc::MediaStreamInterface>>
        active_streams_;

    std::unique_ptr<rtc::Thread> worker_thread_;
    std::unique_ptr<rtc::Thread> network_thread_;
    std::unique_ptr<rtc::Thread> signaling_thread_;

    bool dtls_enable_;
    SocketServerObserver* session_;
    StreamerConfig *streamer_config_;
};


#endif  // RPI_STREAMER_H_
