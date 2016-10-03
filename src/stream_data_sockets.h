/*
 *  Copyright (c) 2016, rpi-webrtc-streamer  Lyu,KeunChang
 *
 * Modified version of webrtc/src/webrtc/examples/peer/server/data_socket.h in WebRTC source tree
 * The origianl copyright info below.
 */

/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RPI_STREAM_DATA_SOCKETS_H_
#define RPI_STREAM_DATA_SOCKETS_H_
#pragma once


#include <memory>
#include <string>

#include "webrtc/base/nethelpers.h"
#include "webrtc/base/physicalsocketserver.h"
#include "webrtc/base/signalthread.h"
#include "webrtc/base/sigslot.h"

struct StreamSessionObserver {
	virtual void OnPeerConnected(int id, const std::string& name) = 0;
	virtual void OnPeerDisconnected(int peer_id) = 0;
	virtual void OnMessageFromPeer(int peer_id, const std::string& message) = 0;
	virtual void OnMessageSent(int err) = 0;

	protected:
		virtual ~StreamSessionObserver() {}
};

class PeerId {
	public :
		static int GeneratePeerId(void);
	private:
		static int peer_id_ ;
};


enum RequestMethod {
	METHOD_INVALID = 0,
	METHOD_GET,
	METHOD_POST,
	METHOD_OPTIONS,
};

enum StreamStatus {
	STATUS_INVALID = 0,
	STATUS_ACTIVE,
	STATUS_INACTIVE,
};

struct QueuedResponse {
	std::string status;
	std::string content_type;
	std::string extra_headers;
	std::string data;
};

struct StreamSessionEntry {
	StreamSessionEntry() {};
	~StreamSessionEntry() {};
	std::string GetEntry() const;
	std::string GetPeerIdHeader() const;
	int	peer_id_;
	std::string name_;
	StreamStatus status_;
	// TODO  
	std::string session_auth_str_;
	std::queue<QueuedResponse> queue_;
	uint64_t  timestamp_ms_;
	uint64_t  queue_timestamp_ms_;
};


enum StreamEvent {
	EVENT_SIGNOUT = 1,
	EVENT_MESSAGE_FROM_PEER,
};



class StreamConnection;	// forward
// Singletone StreamSession
class StreamSession {
	private:
		static StreamSession* stream_session_;
		StreamSession() {};
		StreamSession(const StreamSession&) {};
		~StreamSession() {};

		std::vector<struct StreamSessionEntry *> session_holder_;
		sigslot::signal3<int, bool, QueuedResponse &,  
			sigslot::multi_threaded_local> SignalEvent_;

		struct StreamSessionEntry *active_session_entry_;
		int   active_peer_id_;
		StreamSessionObserver *streamer_callback_;

	public:
		static StreamSession* GetInstance();

		bool ReceiveMessageFromPeer(int peer_id, const std::string &message);
		StreamSessionEntry *GetSessionEntry(int peer_id);
		int CreateSessionEntry(void);
		void DestroySessionEntry(const int peer_id);
		size_t GetSessionSize(void);

		void RegisterObserver(StreamSessionObserver* callback);
		bool ActivateStreamerSession(const int peer_id);
		void DeactivateStreamerSession(const int peer_id);
		bool SendMessageToPeer(int peer_id, const std::string &message);

		void ConnectSignalEvent(StreamConnection *sc);
		void SignalEvent(int peer_id, bool target, QueuedResponse &resp );
};

class StreamConnection : public sigslot::has_slots<> {
	public: 
		StreamConnection(rtc::AsyncSocket *socket);
		~StreamConnection();

		bool HandleRequest( void );
		bool HandleReceivedData(char *data, int length);

		bool Match(rtc::AsyncSocket* socket);
		bool Valid(void) const;

		void OnEvent( int peer_id, bool target, QueuedResponse &resp);
		bool Send(const std::string& status, bool connection_close,
				const std::string& content_type,
				const std::string& extra_headers,
				const std::string& data) const;

		void DumpStreamConnection( void );

	private:
		std::string request_arguments(void) const;
		bool headers_received() const { return method_ != METHOD_INVALID; }
		bool data_received() const {
			return method_ != METHOD_POST || data_.length() >= content_length_;
		}
		bool request_received() const {
			return headers_received() && (method_ != METHOD_POST || data_received());
		}
		RequestMethod method() const { return method_; }

		bool Send(const std::string& data) const;
		bool ParseHeaders(void);
		bool ParseMethodAndPath(const char* begin, size_t len);
		bool ParseContentLengthAndType(const char* headers, size_t length);
		bool PathEquals(const char* path) const;
		void Clear(void);

		const std::string& request_path() const { return request_path_; }
		const std::string& data() const { return data_; }
		const std::string& content_type() const { return content_type_; }
		size_t content_length() const { return content_length_; }

		rtc::AsyncSocket *socket_;
		int peer_id_;
		RequestMethod method_;
		uint64_t timestamp_ms_;
		size_t content_length_;
		bool waiting_socket_;
		std::string content_type_;
		std::string request_path_;
		std::string request_headers_;
		std::string data_;
};


class StreamSockets : public sigslot::has_slots<> {
	public:
		StreamSockets();
		~StreamSockets();

		void OnRead(rtc::AsyncSocket* socket);
		void OnBroadCast(QueuedResponse resp);
		// TODO(kclyu) Merge Sockets and Connections
		bool CreateStreamConnection(rtc::AsyncSocket* socket);
		bool DestroyStreamConnection(rtc::AsyncSocket* socket);
		StreamConnection *Lookup(rtc::AsyncSocket *socket);

		void DumpStreamConnections(StreamConnection *sc);
		void DumpStreamConnectionsAll(void);

	protected:
		std::vector<struct StreamConnection *> stream_connections_;
};

class StreamSocketListen : public sigslot::has_slots<> {
	public:
		StreamSocketListen();
		~StreamSocketListen();

		bool Listen(const rtc::SocketAddress& address);
		void StopListening(void);
	private:
		void OnAccept(rtc::AsyncSocket* socket);
		void OnClose(rtc::AsyncSocket* socket, int err);
		std::unique_ptr<rtc::AsyncSocket> listener_;
		StreamSockets stream_sockets_;
};



#endif  // RPI_STREAM_DATA_SOCKETS_H_
