#include "webrtc/api/video_codecs/builtin_video_encoder_factory.h"
#include "talk/owt/sdk/base/dualvideoencoder.h"

namespace owt {
namespace base {


DualVideoEncoder::DualVideoEncoder()
    : builtin_encoder_factory_(webrtc::CreateBuiltinVideoEncoderFactory())
{}


std::unique_ptr<webrtc::VideoEncoder> DualVideoEncoder::CreateVideoEncoder(
    const webrtc::SdpVideoFormat& format
) {
    return builtin_encoder_factory_->CreateVideoEncoder(format);
}


std::vector<webrtc::SdpVideoFormat> DualVideoEncoder::GetSupportedFormats() const
{
    return builtin_encoder_factory_->GetSupportedFormats();
}


webrtc::VideoEncoderFactory::CodecInfo DualVideoEncoder::QueryVideoEncoder(const webrtc::SdpVideoFormat& format) const
{
    return builtin_encoder_factory_->QueryVideoEncoder(format);
}


} // namespace base
} // namespace owt
