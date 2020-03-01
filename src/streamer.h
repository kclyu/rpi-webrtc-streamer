/*
 *  Copyright (c) 2016, rpi-webrtc-streamer  Lyu,KeunChang
 *
 * stream.h
 *
 * Modified version of src/webrtc/examples/peer/client/peer_connection.h
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

#include "api/media_stream_interface.h"
#include "api/peer_connection_interface.h"
#include "config_streamer.h"
#include "pc/video_track_source.h"
#include "streamer_observer.h"

class Streamer : public webrtc::PeerConnectionObserver,
                 public webrtc::CreateSessionDescriptionObserver,
                 public StreamerObserver {
   public:
    Streamer(SocketServerObserver* session, StreamerConfig* config);
    void AddObserver(SocketServerObserver* session);
    bool connection_active() const;
    virtual void Close();

   protected:
    ~Streamer();
    bool InitializePeerConnection();
    bool CreatePeerConnection();
    void DeletePeerConnection();
    void AddTracks();

    //
    // PeerConnectionObserver implementation.
    //
    void OnSignalingChange(
        webrtc::PeerConnectionInterface::SignalingState new_state) override;
    void OnDataChannel(
        rtc::scoped_refptr<webrtc::DataChannelInterface> channel) override {}
    void OnRenegotiationNeeded() override {}

    void OnStandardizedIceConnectionChange(
        webrtc::PeerConnectionInterface::IceConnectionState new_state) override;
    void OnConnectionChange(webrtc::PeerConnectionInterface::PeerConnectionState
                                new_state) override;
    void OnIceGatheringChange(
        webrtc::PeerConnectionInterface::IceGatheringState new_state) override;
    void OnIceCandidate(
        const webrtc::IceCandidateInterface* candidate) override;
    void OnIceConnectionReceivingChange(bool receiving) override {}

    void OnIceCandidateError(const std::string& host_candidate,
                             const std::string& url, int error_code,
                             const std::string& error_text) override;

    void OnIceCandidateError(const std::string& address, int port,
                             const std::string& url, int error_code,
                             const std::string& error_text) override;

    void OnAddTrack(
        rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
        const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>&
            streams) override;
    void OnRemoveTrack(
        rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver) override;

    void OnInterestingUsage(int usage_pattern) override;

    //
    // CreateSessionDescriptionObserver implementation.
    //
    virtual void OnSuccess(webrtc::SessionDescriptionInterface* desc) override;
    virtual void OnFailure(webrtc::RTCError error) override;

    //
    // StreamerSessionObserver implementation.
    //
    virtual void OnPeerConnected(int peer_id, const std::string& name) override;
    virtual void OnPeerDisconnected(int peer_id) override;
    virtual void OnMessageFromPeer(int peer_id,
                                   const std::string& message) override;
    virtual void OnMessageSent(int err) override;

   private:
    // Send a message to the remote peer.
    void SendMessage(const std::string& json_object);
    // Send a event message to the remote peer.
    void ReportEvent(bool drop_connection, const std::string message);

    // Change the max_bitrate in RtpSender
    void UpdateMaxBitrate();

    int peer_id_;
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
    rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>
        peer_connection_factory_;
    rtc::scoped_refptr<webrtc::AudioDeviceModule> adm_;

    std::unique_ptr<rtc::Thread> worker_thread_;
    std::unique_ptr<rtc::Thread> network_thread_;
    std::unique_ptr<rtc::Thread> signaling_thread_;

    std::vector<rtc::scoped_refptr<webrtc::VideoTrackSource>>
        video_track_sources_;

    SocketServerObserver* session_;
    StreamerConfig* streamer_config_;
    webrtc::PeerConnectionInterface::IceConnectionState ice_state_;
    webrtc::PeerConnectionInterface::PeerConnectionState peerconnection_state_;
};

#endif  // RPI_STREAMER_H_
