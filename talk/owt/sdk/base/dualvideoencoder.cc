#include <utility>

#include "webrtc/api/video_codecs/builtin_video_encoder_factory.h"
#include "talk/owt/sdk/base/customizedencoderbufferhandle.h"
#include "talk/owt/sdk/base/customizedvideoencoderproxy.h"
#include "talk/owt/sdk/base/dualvideoencoder.h"
#include "talk/owt/sdk/base/encodedvideoencoderfactory.h"
#include "webrtc/modules/video_coding/include/video_error_codes.h"
#include "webrtc/rtc_base/logging.h"

namespace owt {
namespace base {

namespace {

class HybridVideoEncoder : public webrtc::VideoEncoder {
 public:
  HybridVideoEncoder(std::unique_ptr<webrtc::VideoEncoder> raw_encoder,
                     std::unique_ptr<webrtc::VideoEncoder> encoded_encoder)
      : raw_encoder_(std::move(raw_encoder)),
        encoded_encoder_(std::move(encoded_encoder)),
        callback_(nullptr),
        encoded_encoder_initialized_(false),
        raw_encoder_initialized_(false),
        has_codec_settings_(false),
        number_of_cores_(0),
        max_payload_size_(0) {}

  int InitEncode(const webrtc::VideoCodec* codec_settings,
                 int number_of_cores,
                 size_t max_payload_size) override {
    if (codec_settings == nullptr) {
      return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
    }
    codec_settings_ = *codec_settings;
    has_codec_settings_ = true;
    number_of_cores_ = number_of_cores;
    max_payload_size_ = max_payload_size;
    encoded_encoder_initialized_ = false;
    raw_encoder_initialized_ = false;
    return WEBRTC_VIDEO_CODEC_OK;
  }

  int32_t Encode(
      const webrtc::VideoFrame& input_image,
      const std::vector<webrtc::VideoFrameType>* frame_types) override {
    if (input_image.video_frame_buffer()->type() ==
        webrtc::VideoFrameBuffer::Type::kNative) {
      if (!EnsureEncoderInitialized(encoded_encoder_.get(),
                                    encoded_encoder_initialized_)) {
        RTC_LOG(LS_ERROR) << "Failed to initialize passthrough video encoder.";
        return WEBRTC_VIDEO_CODEC_ERROR;
      }
      return encoded_encoder_->Encode(input_image, frame_types);
    }

    if (raw_encoder_ == nullptr) {
      RTC_LOG(LS_ERROR) << "Raw video frame received without a raw encoder.";
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
    if (!EnsureEncoderInitialized(raw_encoder_.get(), raw_encoder_initialized_)) {
      RTC_LOG(LS_ERROR) << "Failed to initialize raw video encoder.";
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
    return raw_encoder_->Encode(input_image, frame_types);
  }

  int RegisterEncodeCompleteCallback(
      webrtc::EncodedImageCallback* callback) override {
    callback_ = callback;
    int status = WEBRTC_VIDEO_CODEC_OK;
    if (raw_encoder_ != nullptr) {
      status = raw_encoder_->RegisterEncodeCompleteCallback(callback);
      if (status != WEBRTC_VIDEO_CODEC_OK) {
        return status;
      }
    }
    if (encoded_encoder_ != nullptr) {
      status = encoded_encoder_->RegisterEncodeCompleteCallback(callback);
    }
    return status;
  }

  void SetRates(const RateControlParameters& parameters) override {
    latest_rate_parameters_.reset(new RateControlParameters(parameters));
    if (raw_encoder_initialized_) {
      raw_encoder_->SetRates(parameters);
    }
    if (encoded_encoder_initialized_) {
      encoded_encoder_->SetRates(parameters);
    }
  }

  void OnPacketLossRateUpdate(float packet_loss_rate) override {
    if (raw_encoder_initialized_) {
      raw_encoder_->OnPacketLossRateUpdate(packet_loss_rate);
    }
    if (encoded_encoder_initialized_) {
      encoded_encoder_->OnPacketLossRateUpdate(packet_loss_rate);
    }
  }

  void OnRttUpdate(int64_t rtt_ms) override {
    if (raw_encoder_initialized_) {
      raw_encoder_->OnRttUpdate(rtt_ms);
    }
    if (encoded_encoder_initialized_) {
      encoded_encoder_->OnRttUpdate(rtt_ms);
    }
  }

  void OnLossNotification(
      const LossNotification& loss_notification) override {
    if (raw_encoder_initialized_) {
      raw_encoder_->OnLossNotification(loss_notification);
    }
    if (encoded_encoder_initialized_) {
      encoded_encoder_->OnLossNotification(loss_notification);
    }
  }

  EncoderInfo GetEncoderInfo() const override {
    EncoderInfo info;
    if (raw_encoder_ != nullptr) {
      info = raw_encoder_->GetEncoderInfo();
    } else if (encoded_encoder_ != nullptr) {
      info = encoded_encoder_->GetEncoderInfo();
    }
    info.supports_native_handle = true;
    info.implementation_name = "OWTDualVideoEncoder";
    return info;
  }

  int Release() override {
    int status = WEBRTC_VIDEO_CODEC_OK;
    if (raw_encoder_ != nullptr) {
      status = raw_encoder_->Release();
      raw_encoder_initialized_ = false;
    }
    if (encoded_encoder_ != nullptr) {
      const int encoded_status = encoded_encoder_->Release();
      encoded_encoder_initialized_ = false;
      if (status == WEBRTC_VIDEO_CODEC_OK) {
        status = encoded_status;
      }
    }
    latest_rate_parameters_.reset();
    return status;
  }

 private:
  bool EnsureEncoderInitialized(webrtc::VideoEncoder* encoder,
                                bool& initialized) {
    if (initialized) {
      return true;
    }
    if (encoder == nullptr || !has_codec_settings_) {
      return false;
    }
    if (callback_ != nullptr) {
      const int callback_status =
          encoder->RegisterEncodeCompleteCallback(callback_);
      if (callback_status != WEBRTC_VIDEO_CODEC_OK) {
        RTC_LOG(LS_ERROR) << "Failed to register encoder callback.";
        return false;
      }
    }
    const int init_status =
        encoder->InitEncode(&codec_settings_, number_of_cores_,
                            max_payload_size_);
    if (init_status != WEBRTC_VIDEO_CODEC_OK) {
      RTC_LOG(LS_ERROR) << "InitEncode failed for hybrid sub-encoder.";
      return false;
    }
    initialized = true;
    if (latest_rate_parameters_) {
      encoder->SetRates(*latest_rate_parameters_);
    }
    return true;
  }

  std::unique_ptr<webrtc::VideoEncoder> raw_encoder_;
  std::unique_ptr<webrtc::VideoEncoder> encoded_encoder_;
  webrtc::EncodedImageCallback* callback_;
  bool encoded_encoder_initialized_;
  bool raw_encoder_initialized_;
  webrtc::VideoCodec codec_settings_;
  bool has_codec_settings_;
  int number_of_cores_;
  size_t max_payload_size_;
  std::unique_ptr<RateControlParameters> latest_rate_parameters_;
};

}  // namespace


DualVideoEncoder::DualVideoEncoder()
    : builtin_encoder_factory_(webrtc::CreateBuiltinVideoEncoderFactory()),
      encoded_encoder_factory_(std::make_unique<EncodedVideoEncoderFactory>())
{}


std::unique_ptr<webrtc::VideoEncoder> DualVideoEncoder::CreateVideoEncoder(
    const webrtc::SdpVideoFormat& format
) {
    return std::make_unique<HybridVideoEncoder>(
        builtin_encoder_factory_->CreateVideoEncoder(format),
        encoded_encoder_factory_->CreateVideoEncoder(format));
}


std::vector<webrtc::SdpVideoFormat> DualVideoEncoder::GetSupportedFormats() const
{
    std::vector<webrtc::SdpVideoFormat> supported_formats =
        builtin_encoder_factory_->GetSupportedFormats();
    for (const auto& format : encoded_encoder_factory_->GetSupportedFormats()) {
        bool already_present = false;
        for (const auto& existing_format : supported_formats) {
            if (existing_format.name == format.name &&
                existing_format.parameters == format.parameters) {
                already_present = true;
                break;
            }
        }
        if (!already_present) {
            supported_formats.push_back(format);
        }
    }
    return supported_formats;
}


webrtc::VideoEncoderFactory::CodecInfo DualVideoEncoder::QueryVideoEncoder(const webrtc::SdpVideoFormat& format) const
{
    auto info = builtin_encoder_factory_->QueryVideoEncoder(format);
    info.has_internal_source = false;
    return info;
}


} // namespace base
} // namespace owt
