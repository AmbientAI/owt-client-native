#ifndef OWT_BASE_DUALVIDEOENCODER_H_
#define OWT_BASE_DUALVIDEOENCODER_H_

#include <vector>

#include "webrtc/api/video_codecs/sdp_video_format.h"
#include "webrtc/api/video_codecs/video_encoder.h"
#include "webrtc/api/video_codecs/video_encoder_factory.h"

namespace owt {
namespace base {


class DualVideoEncoder : public webrtc::VideoEncoderFactory {
 public:
  DualVideoEncoder();
  virtual ~DualVideoEncoder(){}
  /* Implement webrtc::VideoEncoderFactory */
  std::unique_ptr<webrtc::VideoEncoder> CreateVideoEncoder(const webrtc::SdpVideoFormat& format) override;
  std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override;
  webrtc::VideoEncoderFactory::CodecInfo QueryVideoEncoder(const webrtc::SdpVideoFormat& format) const override;
 private:
  std::unique_ptr<webrtc::VideoEncoderFactory> builtin_encoder_factory_;
};


}  // namespace base
}  // namespace owt

#endif  // OWT_BASE_DUALVIDEOENCODER_H_
