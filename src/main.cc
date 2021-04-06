/*
 * Copyright (c) 2017, rpi-webrtc-streamer  Lyu,KeunChang
 *
 * main.cc
 *
 * Modified version of src/webrtc/examples/peer/client/main.cc in WebRTC source
 * tree The origianl copyright info below.
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

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/flags/usage_config.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "api/scoped_refptr.h"
#include "app_channel.h"
#include "config_media.h"
#include "config_motion.h"
#include "config_streamer.h"
#include "direct_socket.h"
#include "file_log_sink.h"
#include "mdns_publish.h"
#include "mmal_still_capture.h"
#include "mmal_wrapper.h"
#include "raspi_motion.h"
#include "rtc_base/physical_socket_server.h"
#include "rtc_base/ssl_adapter.h"
#include "streamer.h"
#include "streamer_signaling.h"
#include "system_wrappers/include/field_trial.h"
#include "test/field_trial.h"
#include "utils.h"
#include "websocket_server.h"

//
//
class StreamingSocketServer : public rtc::PhysicalSocketServer {
   public:
    explicit StreamingSocketServer()
        : mdns_publish_enable_(false), websocket_(nullptr) {}
    virtual ~StreamingSocketServer() {
        if (mdns_publish_enable_ == true) mdns_destroy_clientinfo();
    }

    // TODO: Quit feature implementation using message queue is required.
    void SetMessageQueue(rtc::Thread* queue) override {
        message_queue_ = queue;
    }

    void set_websocket_server(LibWebSocketServer* websocket) {
        websocket_ = websocket;
    }

    bool Wait(int cms, bool process_io) override {
        if (websocket_)
            websocket_->RunLoop(0);  // Run Websocket loop once per call
        if (mdns_publish_enable_) {
            if (mdns_run_loop(0)) {
                RTC_LOG(LS_ERROR)
                    << "mDNS: Failed to run mdns publish event loop";
                mdns_publish_enable_ = false;
            }
        }
        return rtc::PhysicalSocketServer::Wait(0 /*cms == -1 ? 1 : cms*/,
                                               process_io);
    }
    void set_mdns_publish_enable(bool enable) { mdns_publish_enable_ = enable; }

   protected:
    bool mdns_publish_enable_;
    rtc::Thread* message_queue_;
    std::unique_ptr<rtc::AsyncSocket> listener_;
    LibWebSocketServer* websocket_;
};

//
//  flags definition for streamer
//
ABSL_FLAG(bool, verbose, false, "Print logging message on stdout");
ABSL_FLAG(std::string, conf, "etc/webrtc_streamer.conf",
          "the main configuration file for webrtc-streamer");
ABSL_FLAG(std::string, severity, "INFO",
          "logging message severity level(VERBOSE,INFO,WARNING,ERROR)");
ABSL_FLAG(std::string, log, "log", "directory for logging message");
ABSL_FLAG(bool, licenses, false, "print the LICENSE information");
ABSL_FLAG(
    std::string, fieldtrials, "",
    "Field trials control experimental features. This flag specifies the field "
    "trials in effect. E.g. running with "
    "--fieldtrials=WebRTC-FooFeature/Enabled/ "
    "will assign the group Enabled to field trial WebRTC-FooFeature. Multiple "
    "trials are separated by \"/\"");
ABSL_FLAG(bool, dump_config, false,
          "dump the configration options after loading");

bool RWsContrainHelpFlags(absl::string_view f) {
    // accept help message flags which is defined in main.cc
    return absl::EndsWith(f, "main.cc");
}

//
// Main
//
int main(int argc, char** argv) {
    absl::FlagsUsageConfig flag_config;
    std::string config_filename;
    std::string log_base_dir;
    webrtc::MMALEncoderWrapper* mmal_encoder;
    std::unique_ptr<utils::FileLogSink> file_log_sink;

    //
    flag_config.contains_helpshort_flags = &RWsContrainHelpFlags;
    flag_config.contains_help_flags = &RWsContrainHelpFlags;
    flag_config.version_string = &utils::GetVersionInfo;

    absl::SetProgramUsageMessage(absl::StrCat(utils::GetProgramDescription(),
                                              " \n", argv[0], " <flags>"));
    absl::SetFlagsUsageConfig(flag_config);
    absl::ParseCommandLine(argc, argv);

    if (FLAGS_licenses.IsSpecifiedOnCommandLine()) {
        utils::PrintLicenseInfo();
        return 0;
    }

    // first get the MMAL wrapper instanc to initialize the MMAL library
    // at the beginning of the program,
    if ((mmal_encoder = webrtc::MMALWrapper::Instance()) == nullptr) {
        RTC_LOG(LS_ERROR) << "Failed to get MMALEncoderWrapper";
        return -1;
    }

    std::string conf_filename = absl::GetFlag(FLAGS_conf);
    std::cerr << "Using streamer config file : " << conf_filename << "\n";
    // Load the streamer configuration from file
    ConfigStreamer config_streamer(conf_filename,
                                   absl::GetFlag(FLAGS_dump_config));
    if (config_streamer.Load() == false) {
        std::cerr << "Failed to load config options for streamer: "
                  << conf_filename << "\n";
        return -1;
    };

    // The command line FieldTrials string has more priority than
    // the FieldTrials specified in the config file.
    // If FieldTrials is not specified on the command line,
    // use it if there is FieldTrials in the config file.
    if (FLAGS_fieldtrials.IsSpecifiedOnCommandLine()) {
        std::string fieldtrials = absl::GetFlag(FLAGS_fieldtrials);
        std::cerr << "Using CommandLine FieldTrials :" << fieldtrials << "\n";
        webrtc::field_trial::InitFieldTrialsFromString(fieldtrials.c_str());
    } else {
        std::string fieldtrials = config_streamer.GetFieldTrials();
        if (!fieldtrials.empty()) {
            std::cerr << "Using Config FieldTrials :" << fieldtrials << "\n";
            webrtc::field_trial::InitFieldTrialsFromString(fieldtrials.c_str());
        }
    }

    std::string log_severity_str = absl::GetFlag(FLAGS_severity);
    rtc::LoggingSeverity severity = utils::String2LogSeverity(log_severity_str);
    if (absl::GetFlag(FLAGS_verbose)) {
        // changing severity to INFO level
        severity = utils::String2LogSeverity("INFO");
        rtc::LogMessage::LogToDebug(severity);
    } else {
        // Getting the log directory path
        // The priorities are as follows.
        //      1. Use command line flag when path of command line flag exists
        //      2. If not, use the INSTALL_DIR + log path
        log_base_dir = absl::GetFlag(FLAGS_log);
        if (config_streamer.GetLogPath(log_base_dir) == false) {
            std::cerr << "Failed to get log directory : " << log_base_dir
                      << "\n";
            return -1;
        };

        file_log_sink.reset(new utils::FileLogSink(
            log_base_dir, severity, config_streamer.GetDisableLogBuffering()));
        // file logging will be enabled only when verbose flag is disabled.
        if (!file_log_sink->Init()) {
            std::cerr << "Failed to init file message logger\n";
            return -1;
        }
    }

    // loading media confiratuion options
    // media configuration sigleton reference
    ConfigMedia* config_media = ConfigMediaSingleton::Instance();
    RTC_LOG(INFO) << "Using media config file: "
                  << config_streamer.GetMediaConfigFile();
    config_media->Load(config_streamer.GetMediaConfigFile(),
                       absl::GetFlag(FLAGS_dump_config));

    // loading motion confiratuion options
    // Load the streamer configuration from file
    ConfigMotion config_motion(config_streamer.GetMotionConfigFile(),
                               absl::GetFlag(FLAGS_dump_config));
    if (config_motion.Load() == false) {
        RTC_LOG(LS_WARNING) << "Failed to load motion option config file:"
                            << config_streamer.GetMotionConfigFile();
    } else
        RTC_LOG(INFO) << "Using motion config file: "
                      << config_streamer.GetMotionConfigFile();

    std::unique_ptr<DirectSocketServer> direct_socket_server;
    std::unique_ptr<AppChannel> app_channel;

    StreamingSocketServer socket_server;
    rtc::AutoSocketServerThread thread(&socket_server);

    rtc::InitializeSSL();

    RaspiMotionHolder motion_holder(&config_motion);
    StreamerProxy streamer_proxy(&motion_holder);

    // DirectSocket
    if (config_streamer.GetDirectSocketEnable() == true) {
        int direct_socket_port_num = config_streamer.GetDirectSocketPort();
        RTC_LOG(INFO) << "Direct socket port num: " << direct_socket_port_num;
        rtc::SocketAddress addr("0.0.0.0", direct_socket_port_num);

        direct_socket_server.reset(new DirectSocketServer(&streamer_proxy));
        if (direct_socket_server->Listen(addr) == false) {
            rtc::CleanupSSL();
            return -1;
        }
    }

    // WebSocket
    if (config_streamer.GetWebSocketEnable() == true) {
        app_channel.reset(new AppChannel());
        app_channel->AppInitialize(&streamer_proxy, config_streamer,
                                   config_motion);

        socket_server.set_websocket_server(app_channel.get());

        // mDNS publish initialization
        // mDNS only publishes the WebSocket information used by RWS.
        int websocket_port = config_streamer.GetWebSocketPort();
        std::string websocket_url_path = config_streamer.GetRwsWsUrlPath();
        std::string deviceid;
        if (utils::GetHardwareDeviceId(&deviceid) == true &&
            mdns_init_clientinfo(websocket_port, websocket_url_path.c_str(),
                                 deviceid.c_str()) == MDNS_SUCCESS) {
            // enabling mDNS publish event loop
            socket_server.set_mdns_publish_enable(true);
        } else {
            RTC_LOG(LS_ERROR) << "mDNS: Failed to initialize mdns client info";
        }
    };

    // starting streamer
    rtc::scoped_refptr<Streamer> streamer(
        new rtc::RefCountedObject<Streamer>(&streamer_proxy, &config_streamer));

    // Running Loop
    thread.Run();

    rtc::CleanupSSL();
    return 0;
}
