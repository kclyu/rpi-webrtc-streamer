/*
 * Copyright (c) 2017, rpi-webrtc-streamer  Lyu,KeunChang
 *
 * main.cc
 *
 * Modified version of src/webrtc/examples/peer/client/main.cc in WebRTC source tree
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


#include <iostream>

#include "api/scoped_refptr.h"
#include "rtc_base/flags.h"
#include "rtc_base/physical_socket_server.h"
#include "rtc_base/ssl_adapter.h"
#include "system_wrappers/include/field_trial.h"
#include "test/field_trial.h"

#include "websocket_server.h"
#include "app_channel.h"

#include "streamer_observer.h"
#include "direct_socket.h"
#include "config_streamer.h"
#include "config_media.h"
#include "config_motion.h"
#include "streamer.h"
#include "raspi_motion.h"
#include "utils.h"
#include "file_logger.h"
#include "mmal_wrapper.h"
#include "mdns_publish.h"


class StreamingSocketServer : public rtc::PhysicalSocketServer {
public:
    explicit StreamingSocketServer()
        : mdns_publish_enable_(false), websocket_(nullptr) {}
    virtual ~StreamingSocketServer() {
        if( mdns_publish_enable_ == true ) mdns_destroy_clientinfo();
    }

    // TODO: Quit feature implementation using message queue is required.
    void SetMessageQueue(rtc::MessageQueue* queue) override {
        message_queue_ = queue;
    }

    void set_websocket_server(LibWebSocketServer* websocket) { websocket_ = websocket; }

    virtual bool Wait(int cms, bool process_io) {
        if( websocket_ )
            websocket_->RunLoop(0); // Run Websocket loop once per call
        if( mdns_publish_enable_ ) {
            if( mdns_run_loop(0) ) {
                RTC_LOG(LS_ERROR)  << "mDNS: Failed to run mdns publish event loop";
                mdns_publish_enable_ = false;
            }
        }
        return rtc::PhysicalSocketServer::Wait(0/*cms == -1 ? 1 : cms*/,
                process_io);
    }
    void set_mdns_publish_enable(bool enable) { mdns_publish_enable_ = enable;}

protected:
    bool mdns_publish_enable_;
    rtc::MessageQueue* message_queue_;
    std::unique_ptr<rtc::AsyncSocket> listener_;
    LibWebSocketServer *websocket_;
};

//
//  flags definition for streamer
//
WEBRTC_DEFINE_bool(help, false, "Prints this message");
WEBRTC_DEFINE_bool(verbose, false, "Print logging message on stdout");
WEBRTC_DEFINE_string(conf, "etc/webrtc_streamer.conf", \
           "the main configuration file for webrtc-streamer");
WEBRTC_DEFINE_string(severity, "INFO", \
           "logging message severity level(VERBOSE,INFO,WARNING,ERROR)");
WEBRTC_DEFINE_string(log, "log", "directory for logging message");
WEBRTC_DEFINE_bool(licenses, false, "print the LICENSE information");
WEBRTC_DEFINE_bool(version, false, "print the Version information");
WEBRTC_DEFINE_string(fieldtrials,
    "",
    "Field trials control experimental features. This flag specifies the field "
    "trials in effect. E.g. running with "
    "--fieldtrials=WebRTC-FooFeature/Enabled/ "
    "will assign the group Enabled to field trial WebRTC-FooFeature. Multiple "
    "trials are separated by \"/\"");

//
// Main
//
int main(int argc, char** argv) {
    std::string config_filename;
    std::string baselog_dir;
    rtc::FlagList::SetFlagsFromCommandLine(&argc, argv, true);
    webrtc::MMALEncoderWrapper* mmal_encoder;
    std::unique_ptr<utils::FileLogger> file_logger;

    // To initialize the MMAL library at the beginning of the program,
    // first import the MMAL wrapper instance.
    if((mmal_encoder =  webrtc::MMALWrapper::Instance() ) == nullptr ) {
        RTC_LOG(LS_ERROR) << "Failed to get MMALEncoderWrapper";
        return -1;
    }

    if( FLAG_help ) {
        rtc::FlagList::Print(NULL, false);
        return 0;
    }

    if( FLAG_licenses ) {
        utils::PrintLicenseInfo();
        return 0;
    };

    if( FLAG_version ) {
        utils::PrintVersionInfo();
        return 0;
    };

    std::cerr  << "config file :" << FLAG_conf << "\n";
    // Load the streamer configuration from file
    StreamerConfig streamer_config(FLAG_conf);
    if( streamer_config.LoadConfig() == false ) {
        std::cerr  << "Failed to load config options:"
                << streamer_config.GetConfigFilename() << "\n";
        return -1;
    };

    // The command line FieldTrials string has more priority than
    // the FieldTrials specified in the config file.
    // If FieldTrials is not specified on the command line,
    // use it if there is FieldTrials in the config file.
    webrtc::test::ValidateFieldTrialsStringOrDie(FLAG_fieldtrials);
    if( strlen(FLAG_fieldtrials) != 0 ) {   // length 0 is default value
        std::cerr  << "Using FieldTrials : " << FLAG_fieldtrials << "\n";
        webrtc::field_trial::InitFieldTrialsFromString(FLAG_fieldtrials);
    }
    else {
        std::string fieldtrials;
        if( streamer_config.GetFieldTrials(fieldtrials) == true ) {
            std::cerr  << "Using FieldTrials(config) :" << fieldtrials << "\n";
            webrtc::field_trial::InitFieldTrialsFromString(fieldtrials.c_str());
        }
    }

    rtc::LoggingSeverity severity = utils::String2LogSeverity(FLAG_severity);
    if( FLAG_verbose )  {
        // changing severity to INFO level
        severity = utils::String2LogSeverity("INFO");
        rtc::LogMessage::LogToDebug(severity);
    }
    else {
        // Getting the log directory path
        // The priorities are as follows.
        //      1. Use command line flag when path of command line flag exists
        //      2. If not, use the INSTALL_DIR + log path
        baselog_dir = FLAG_log;
        if( streamer_config.GetLogPath(baselog_dir) == false ) {
            std::cerr << "Failed to get log directory : "
                << baselog_dir << "\n";
            return -1;
        };

        file_logger.reset(new utils::FileLogger(baselog_dir, severity,
                streamer_config.GetDisableLogBuffering()));
        // file logging will be enabled only when verbose flag is disabled.
        if( !file_logger->Init() ) {
            std::cerr << "Failed to init file message logger\n" ;
            return -1;
        }
    }

    if( streamer_config.GetMediaConfigFilePath(config_filename) == true ) {
        // media configuration sigleton reference
        ConfigMedia *config_media = ConfigMediaSingleton::Instance();
        RTC_DCHECK( config_media != nullptr );
        // return value does not have meaning
        config_media->Load(config_filename);
        RTC_LOG(INFO) << "Using Media Config file: " << config_filename;
    };

    if( streamer_config.GetMotionConfigFilePath(config_filename) == true ) {
        if( config_motion::config_load(config_filename) == false ) {
            RTC_LOG(LS_WARNING) << "Failed to load motion option config file:"
                << config_filename;
        }
        RTC_LOG(INFO) << "Using Motion Config file: " << config_filename;
    };

    std::unique_ptr<DirectSocketServer> direct_socket_server;
    std::unique_ptr<AppChannel> app_channel;

    StreamingSocketServer socket_server;
    rtc::AutoSocketServerThread thread(&socket_server);

    rtc::InitializeSSL();

    // DirectSocket
    if(streamer_config.GetDirectSocketEnable() == true) {
        int direct_socket_port_num;

        if( !streamer_config.GetDirectSocketPort(direct_socket_port_num) ) {
            RTC_LOG(LS_ERROR) << "Error in getting direct socket port number: "
                << direct_socket_port_num;
            rtc::CleanupSSL();
            return -1;
        }
        RTC_LOG(INFO) << "Direct socket port num: " << direct_socket_port_num;
        rtc::SocketAddress addr( "0.0.0.0", direct_socket_port_num );

        direct_socket_server.reset(new DirectSocketServer());
        if (direct_socket_server->Listen(addr) == false) {
            rtc::CleanupSSL();
            return -1;
        }
    }

    // WebSocket
    if( streamer_config.GetWebSocketEnable() == true ) {
        app_channel.reset(new AppChannel());
        app_channel->AppInitialize(streamer_config);

        socket_server.set_websocket_server(app_channel.get());

        // mDNS publish initialization
        // mDNS only publishes the WebSocket information used by RWS.
        int websocket_port;
        std::string websocket_url_path;
        std::string deviceid;
        streamer_config.GetWebSocketPort(websocket_port);
        streamer_config.GetRwsWsURL(websocket_url_path);
        utils::GetHardwareDeviceId(&deviceid);
        if( mdns_init_clientinfo(websocket_port, websocket_url_path.c_str(),
                    deviceid.c_str()) == MDNS_FAILED) {
            RTC_LOG(LS_ERROR) << "mDNS: Failed to initialize mdns client info";
        }
        else {
            // enabling mDNS publish event loop
            socket_server.set_mdns_publish_enable(true);
        }
    };

    // starting streamer
    rtc::scoped_refptr<Streamer> streamer(
        new rtc::RefCountedObject<Streamer>(StreamerProxy::GetInstance(),
        &streamer_config));

    // Running Loop
    thread.Run();

    rtc::CleanupSSL();
    return 0;
}

