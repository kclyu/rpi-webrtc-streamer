
#ifndef RPI_QUALITY_CONFIG_
#define RPI_QUALITY_CONFIG_

#include <list>
#include <utility>

#include "webrtc/common_types.h"
#include "webrtc/video_encoder.h"
#include "webrtc/base/optional.h"

#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/modules/video_coding/utility/moving_average.h"


enum Adaptation {
    NO_ADAPTATION = 0,
    ADAPTAION_UP,
    ADAPTAION_DOWN,
};

enum AdaptationReason {
    ADAPTAION_NONE = 0,
    ADAPTAION_BITRATE,
    ADAPTAION_QP,
    ADAPTAION_PACKETLOSS,
    ADAPTAION_RTT,
};

// TODO: Need to implement the QP/loss/rtt based Quality Control logic.
// Currently, Only BWE-based QoS functions are implemented.
class QualityConfig {
public:
    QualityConfig();
    ~QualityConfig();

    struct Resolution {
        Resolution() : width_(0),height_(0), 
            adaptation_(NO_ADAPTATION), adaptation_reason_(ADAPTAION_NONE) {};
        Resolution(int width, int height) : width_(width),height_(height), 
            adaptation_(NO_ADAPTATION), adaptation_reason_(ADAPTAION_NONE) {};
        int width_;
        int height_;
        Adaptation adaptation_;
        AdaptationReason adaptation_reason_;
    };

    struct ResolutionConfigEntry {
        ResolutionConfigEntry(int width, int height, int max_fps, int min_fps);
        int width_;
        int height_;
        int max_fps_;
        int min_fps_;
        int average_bandwidth_;
        int max_bandwidth_;
        int min_bandwidth_;
    };

    void Reset();
    void ReportQP(int qp);
    void ReportChannelParameters(uint32_t packet_loss, uint64_t rtt );
    void ReportFrameRate(int framerate);
    void ReportTargetBitrate(int bitrate); // kbps
    void ReportMaxBitrate(int bitrate);    // kbps

    bool IsAdaptationRequired();

    // Getter for Encoder Config
    int  GetFrameRate();
    int  GetBitrate();

    bool GetBestMatch(int target_bitrate, QualityConfig::Resolution& resolution);
    bool GetBestMatch(QualityConfig::Resolution& resolution);

private:
    std::list<ResolutionConfigEntry> resolution_config_;
    int target_framerate_;
    int target_bitrate_;
    int max_bitrate_;
    webrtc::MovingAverage packet_loss_;
    webrtc::MovingAverage rtt_;
    webrtc::MovingAverage average_qp_;
    Resolution current_res_;
    bool use_4_3_resolution_;

    // 
    bool adaptation_down_;
    bool adaptation_up_;
};

#endif  // RPI_QUALITY_CONFIG_
