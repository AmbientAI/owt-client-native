#include "webrtc/api/video_codecs/builtin_video_encoder_factory.h"
#include "webrtc/modules/video_coding/include/video_codec_interface.h"
#include "webrtc/modules/video_coding/include/video_error_codes.h"
#include "webrtc/rtc_base/logging.h"
#include "talk/owt/sdk/base/dualvideoencoder.h"
#include "talk/owt/sdk/base/customizedencoderbufferhandle.h"
#include "media/base/media_constants.h"
#include "talk/owt/sdk/base/codecutils.h"

namespace owt {
namespace base {


// Dummy Frame
static const uint8_t orange_frame_values[] = {
    0, 0, 0, 1, 103, 66, 192, 20, 220, 20, 31, 161, 0, 0, 3, 0, 1, 0, 0, 3, 0, 60, 143, 20, 43, 128,
    0, 0, 0, 1, 104, 206, 60, 128, 0, 0, 1, 6, 5, 255, 255, 101, 220, 69, 233, 189, 230, 217, 72,
    183, 150, 44, 216, 32, 217, 35, 238, 239, 120, 50, 54, 52, 32, 45, 32, 99, 111, 114, 101, 32, 49,
    54, 52, 32, 114, 51, 48, 57, 53, 32, 98, 97, 101, 101, 52, 48, 48, 32, 45, 32, 72, 46, 50, 54,
    52, 47, 77, 80, 69, 71, 45, 52, 32, 65, 86, 67, 32, 99, 111, 100, 101, 99, 32, 45, 32, 67, 111,
    112, 121, 108, 101, 102, 116, 32, 50, 48, 48, 51, 45, 50, 48, 50, 50, 32, 45, 32, 104, 116, 116,
    112, 58, 47, 47, 119, 119, 119, 46, 118, 105, 100, 101, 111, 108, 97, 110, 46, 111, 114, 103, 47,
    120, 50, 54, 52, 46, 104, 116, 109, 108, 32, 45, 32, 111, 112, 116, 105, 111, 110, 115, 58, 32,
    99, 97, 98, 97, 99, 61, 48, 32, 114, 101, 102, 61, 49, 32, 100, 101, 98, 108, 111, 99, 107, 61,
    48, 58, 48, 58, 48, 32, 97, 110, 97, 108, 121, 115, 101, 61, 48, 58, 48, 32, 109, 101, 61, 100,
    105, 97, 32, 115, 117, 98, 109, 101, 61, 48, 32, 112, 115, 121, 61, 49, 32, 112, 115, 121, 95, 114,
    100, 61, 49, 46, 48, 48, 58, 48, 46, 48, 48, 32, 109, 105, 120, 101, 100, 95, 114, 101, 102, 61,
    48, 32, 109, 101, 95, 114, 97, 110, 103, 101, 61, 49, 54, 32, 99, 104, 114, 111, 109, 97, 95, 109,
    101, 61, 49, 32, 116, 114, 101, 108, 108, 105, 115, 61, 48, 32, 56, 120, 56, 100, 99, 116, 61, 48,
    32, 99, 113, 109, 61, 48, 32, 100, 101, 97, 100, 122, 111, 110, 101, 61, 50, 49, 44, 49, 49, 32,
    102, 97, 115, 116, 95, 112, 115, 107, 105, 112, 61, 49, 32, 99, 104, 114, 111, 109, 97, 95, 113,
    112, 95, 111, 102, 102, 115, 101, 116, 61, 48, 32, 116, 104, 114, 101, 97, 100, 115, 61, 51, 32,
    108, 111, 111, 107, 97, 104, 101, 97, 100, 95, 116, 104, 114, 101, 97, 100, 115, 61, 51, 32, 115,
    108, 105, 99, 101, 100, 95, 116, 104, 114, 101, 97, 100, 115, 61, 49, 32, 115, 108, 105, 99, 101,
    115, 61, 51, 32, 110, 114, 61, 48, 32, 100, 101, 99, 105, 109, 97, 116, 101, 61, 49, 32, 105, 110,
    116, 101, 114, 108, 97, 99, 101, 100, 61, 48, 32, 98, 108, 117, 114, 97, 121, 95, 99, 111, 109, 112,
    97, 116, 61, 48, 32, 99, 111, 110, 115, 116, 114, 97, 105, 110, 101, 100, 95, 105, 110, 116, 114, 97,
    61, 48, 32, 98, 102, 114, 97, 109, 101, 115, 61, 48, 32, 119, 101, 105, 103, 104, 116, 112, 61, 48,
    32, 107, 101, 121, 105, 110, 116, 61, 49, 32, 107, 101, 121, 105, 110, 116, 95, 109, 105, 110, 61, 49,
    32, 115, 99, 101, 110, 101, 99, 117, 116, 61, 48, 32, 105, 110, 116, 114, 97, 95, 114, 101, 102, 114,
    101, 115, 104, 61, 48, 32, 114, 99, 61, 97, 98, 114, 32, 109, 98, 116, 114, 101, 101, 61, 48, 32, 98,
    105, 116, 114, 97, 116, 101, 61, 53, 48, 48, 32, 114, 97, 116, 101, 116, 111, 108, 61, 49, 46, 48,
    32, 113, 99, 111, 109, 112, 61, 48, 46, 54, 48, 32, 113, 112, 109, 105, 110, 61, 48, 32, 113, 112,
    109, 97, 120, 61, 54, 57, 32, 113, 112, 115, 116, 101, 112, 61, 52, 32, 105, 112, 95, 114, 97, 116,
    105, 111, 61, 49, 46, 52, 48, 32, 97, 113, 61, 48, 0, 128, 0, 0, 1, 101, 136, 132, 15, 161, 24, 160,
    0, 34, 17, 28, 0, 4, 142, 99, 128, 0, 155, 20, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156,
    156, 156, 156, 156, 156, 156, 156, 156, 157, 117, 215, 93, 117, 215, 93, 117, 215, 93, 117, 215, 93,
    120
};
static const uint8_t blue_frame_values[] = {
  0,0,0,1,103,66,192,20,220,20,31,161,0,0,3,0,1,0,0,3,0,60,143,20,43,128,0,0,0,1,104,206,60,128,0,0,1,6,5,
  255,255,101,220,69,233,189,230,217,72,183,150,44,216,32,217,35,238,239,120,50,54,52,32,45,32,99,111,114,
  101,32,49,54,52,32,114,51,48,57,53,32,98,97,101,101,52,48,48,32,45,32,72,46,50,54,52,47,77,80,69,71,45,52,
  32,65,86,67,32,99,111,100,101,99,32,45,32,67,111,112,121,108,101,102,116,32,50,48,48,51,45,50,48,50,50,32,
  45,32,104,116,116,112,58,47,47,119,119,119,46,118,105,100,101,111,108,97,110,46,111,114,103,47,120,50,54,
  52,46,104,116,109,108,32,45,32,111,112,116,105,111,110,115,58,32,99,97,98,97,99,61,48,32,114,101,102,61,
  49,32,100,101,98,108,111,99,107,61,48,58,48,58,48,32,97,110,97,108,121,115,101,61,48,58,48,32,109,101,61,
  100,105,97,32,115,117,98,109,101,61,48,32,112,115,121,61,49,32,112,115,121,95,114,100,61,49,46,48,48,58,
  48,46,48,48,32,109,105,120,101,100,95,114,101,102,61,48,32,109,101,95,114,97,110,103,101,61,49,54,32,99,
  104,114,111,109,97,95,109,101,61,49,32,116,114,101,108,108,105,115,61,48,32,56,120,56,100,99,116,61,48,32,
  99,113,109,61,48,32,100,101,97,100,122,111,110,101,61,50,49,44,49,49,32,102,97,115,116,95,112,115,107,105,
  112,61,49,32,99,104,114,111,109,97,95,113,112,95,111,102,102,115,101,116,61,48,32,116,104,114,101,97,100,
  115,61,51,32,108,111,111,107,97,104,101,97,100,95,116,104,114,101,97,100,115,61,51,32,115,108,105,99,101,
  100,95,116,104,114,101,97,100,115,61,49,32,115,108,105,99,101,115,61,51,32,110,114,61,48,32,100,101,99,
  105,109,97,116,101,61,49,32,105,110,116,101,114,108,97,99,101,100,61,48,32,98,108,117,114,97,121,95,99,
  111,109,112,97,116,61,48,32,99,111,110,115,116,114,97,105,110,101,100,95,105,110,116,114,97,61,48,32,98,
  102,114,97,109,101,115,61,48,32,119,101,105,103,104,116,112,61,48,32,107,101,121,105,110,116,61,49,32,107,
  101,121,105,110,116,95,109,105,110,61,49,32,115,99,101,110,101,99,117,116,61,48,32,105,110,116,114,97,95,
  114,101,102,114,101,115,104,61,48,32,114,99,61,97,98,114,32,109,98,116,114,101,101,61,48,32,98,105,116,
  114,97,116,101,61,53,48,48,32,114,97,116,101,116,111,108,61,49,46,48,32,113,99,111,109,112,61,48,46,54,48,
  32,113,112,109,105,110,61,48,32,113,112,109,97,120,61,54,57,32,113,112,115,116,101,112,61,52,32,105,112,
  95,114,97,116,105,111,61,49,46,52,48,32,97,113,61,48,0,128,0,0,1,101,136,132,15,161,24,160,0,41,107,28,0,
  4,190,35,128,0,130,236,156,156,156,156,156,156,156,156,156,156,156,156,156,156,156,156,156,156,157,117,
  215,93,117,215,93,117,215,93,117,215,93,117,215,93,117,215,93,117,215,93,117,215,93,117,215,93,117,215,
  93,117,215,93,117,215,93,117,215,93,117,215,93,117,215,93,117,215,93,117,215,93,117,215,93,117,215,93,117,
  215,94,0,0,1,101,3,40,136,64,250,17,138,0,2,150,177,192,0,75,226,56,0,8,46,201,201,201,201,201,201,201,
  201,201,201,201,201,201,201,201,201,201,201,201,215,93,117,215,93,117,215,93,117,215,93,117,215,93,117,
  215,93,117,215,93,117,215,93,117,215,93,117,215,93,117,215,93,117,215,93,117,215,93,117,215,93,117,215,93,
  117,215,93,117,215,93,117,215,93,117,215,93,117,215,93,117,224,0,0,1,101,1,146,34,16,62,132,98,128,0,165,
  172,112,0,18,248,142,0,2,11,178,114,114,114,114,114,114,114,114,114,114,114,114,114,114,114,114,114,114,
  117,215,93,117,215,93,117,215,93,117,215,93,117,215,93,117,215,93,117,215,93,117,215,93,117,215,93,117,
  215,93,117,215,93,117,215,93,117,215,93,117,215,93,117,215,93,117,215,93,117,215,93,117,215,93,117,215,
  93,117,215,93,120
};


// H.264 start code length.
#define H264_SC_LENGTH 4

// Maximum allowed NALUs in one output frame.
#define MAX_NALUS_PERFRAME 32


static int32_t next_nalu_position(
  uint8_t* buffer,
  size_t buffer_size,
  size_t* sc_length
)
{
  if (buffer_size < H264_SC_LENGTH) {
    return -1;
  }
  uint8_t* head = buffer;
  // Set end buffer pointer to 4 bytes before actual buffer end so we can
  // access head[1], head[2] and head[3] in a loop without buffer overrun.
  uint8_t* end = buffer + buffer_size - H264_SC_LENGTH;
  while (head < end) {
    if (head[0]) {
      head++;
      continue;
    }
    if (head[1]) {  // got 00xx
      head += 2;
      continue;
    }
    if (head[2]) {  // got 0000xx
      if (head[2] == 0x01) {
        *sc_length = 3;
        return (int32_t)(head - buffer);
      }
      head += 3;
      continue;
    }
    if (head[3] != 0x01) {  // got 000000xx
      head++;               // xx != 1, continue searching.
      continue;
    }
    *sc_length = 4;
    return (int32_t)(head - buffer);
  }
  return -1;
}


class PrivateVideoEncoder : public webrtc::VideoEncoder {
 public:
  PrivateVideoEncoder(std::unique_ptr<webrtc::VideoEncoder> default_encoder);
  virtual ~PrivateVideoEncoder();
  int InitEncode(
    const webrtc::VideoCodec* codec_settings,
    int number_of_cores,
    size_t max_payload_size
  ) override;
  int RegisterEncodeCompleteCallback(webrtc::EncodedImageCallback* callback) override;
  void SetRates(const RateControlParameters& parameters) override;
  int Release() override;
  int32_t Encode(
    const webrtc::VideoFrame& input_image,
    const std::vector<webrtc::VideoFrameType>* frame_types
  ) override;
  webrtc::VideoEncoder::EncoderInfo GetEncoderInfo() const override;
 private:
  std::unique_ptr<webrtc::VideoEncoder> default_encoder_;
  webrtc::EncodedImageCallback* callback_ = nullptr;
};


PrivateVideoEncoder::PrivateVideoEncoder(std::unique_ptr<webrtc::VideoEncoder> default_encoder)
  : default_encoder_(std::move(default_encoder))
{
  RTC_LOG(LS_WARNING) << "[DTAG] " << "PrivateVideoEncoder::PrivateVideoEncoder()";
}


PrivateVideoEncoder::~PrivateVideoEncoder()
{
  RTC_LOG(LS_WARNING) << "[DTAG] " << "PrivateVideoEncoder::~PrivateVideoEncoder()";
}


int PrivateVideoEncoder::InitEncode(
  const webrtc::VideoCodec* codec_settings,
  int number_of_cores,
  size_t max_payload_size
)
{
  RTC_LOG(LS_WARNING) << "[DTAG] " << "PrivateVideoEncoder::InitEncode()";
  return default_encoder_->InitEncode(codec_settings, number_of_cores, max_payload_size);
}


int PrivateVideoEncoder::RegisterEncodeCompleteCallback(webrtc::EncodedImageCallback* callback)
{
  RTC_LOG(LS_WARNING) << "[DTAG] " << "PrivateVideoEncoder::RegisterEncodeCompleteCallback()";
  callback_ = callback;
  return default_encoder_->RegisterEncodeCompleteCallback(callback);
}


void PrivateVideoEncoder::SetRates(const RateControlParameters& parameters)
{
  RTC_LOG(LS_WARNING) << "[DTAG] " << "PrivateVideoEncoder::SetRates(): no-op";
  default_encoder_->SetRates(parameters);
}


int PrivateVideoEncoder::Release()
{
  RTC_LOG(LS_WARNING) << "[DTAG] " << "PrivateVideoEncoder::Release()";
  return default_encoder_->Release();
}


int PrivateVideoEncoder::Encode(
  const webrtc::VideoFrame& input_image,
  const std::vector<webrtc::VideoFrameType>* frame_types
)
{
  RTC_LOG(LS_WARNING) << "[DTAG] " << "PrivateVideoEncoder::Encode()";
  if (frame_types->size() > 0 and frame_types->at(0) == webrtc::VideoFrameType::kVideoFrameKey) {
    RTC_LOG(LS_WARNING) << "[DTAG] " << "PrivateVideoEncoder::Encode() FTV " << frame_types->size() << " frame types";
  }
  if (callback_ == nullptr) {
    RTC_LOG(LS_ERROR) << "Callback not set";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  // CustomizedEncoderBufferHandle* encoder_buffer_handle =
  //     reinterpret_cast<CustomizedEncoderBufferHandle*>(
  //         static_cast<owt::base::EncodedFrameBuffer*>(
  //             input_image.video_frame_buffer().get())
  //             ->native_handle());
  // assert(encoder_buffer_handle != nullptr);
  // VideoEncoderInterface* encoder = encoder_buffer_handle->encoder;
  // assert(encoder != nullptr);
  // RTC_LOG(LS_WARNING) << "[DTAG] " << "PrivateVideoEncoder::Encode() got encoder "
  //   << static_cast<void*>(encoder);

  // std::vector<uint8_t> buffer;
  // bool success = encoder->EncodeOneFrame(buffer, true);
  // RTC_LOG(LS_WARNING) << "[DTAG] " << "PrivateVideoEncoder::Encode() encoder->EncodeOneFrame()=" << success;

  // if (!success) {
  //   return WEBRTC_VIDEO_CODEC_ERROR;
  // }

  // bool is_keyframe = buffer.back();
  // buffer.pop_back();
  // uint8_t* data_ptr = buffer.data();
  // size_t data_size = buffer.size();

  // webrtc::EncodedImage encoded_frame(buffer.data(), buffer.size(), buffer.size());
  // encoded_frame._encodedWidth = input_image.width();
  // encoded_frame._encodedHeight = input_image.height();
  // encoded_frame._completeFrame = true;
  // encoded_frame._frameType = is_keyframe ? webrtc::VideoFrameType::kVideoFrameKey : webrtc::VideoFrameType::kVideoFrameDelta;
  // encoded_frame.capture_time_ms_ = input_image.render_time_ms();
  // encoded_frame.SetTimestamp(input_image.timestamp());


  auto frame_buffer = input_image.video_frame_buffer().get();
  if (frame_buffer == nullptr) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  if (frame_buffer->type() != webrtc::VideoFrameBuffer::Type::kNative) {
    // Raw frames use the default encoder
    // TODO(dtag): Don't route like this frame-by-frame. Should be determined once.
    RTC_LOG(LS_WARNING) << "[DTAG] " << "PrivateVideoEncoder::Encode() raw frame";
    return default_encoder_->Encode(input_image, frame_types);
  }
  auto encoder_buffer_handle = reinterpret_cast<EncodedVideoFrameBufferNativeHandle*>(
     static_cast<owt::base::EncodedVideoFrameBuffer*>(frame_buffer)->native_handle());
  if (encoder_buffer_handle == nullptr) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  // struct EncodedVideoFrameBufferNativeHandle{
  //   bool is_keyframe = false;
  //   size_t width = 0;
  //   size_t height = 0;
  //   std::vector<uint8_t> data;
  // };
  // EncodedVideoFrameBufferNativeHandle* encoder_buffer_handle =
  //     reinterpret_cast<EncodedVideoFrameBufferNativeHandle*>(
  //         static_cast<owt::base::EncodedVideoFrameBuffer*>(
  //             input_image.video_frame_buffer().get())
  //             ->native_handle());
  //assert(encoder_buffer_handle != nullptr);

  bool is_keyframe = encoder_buffer_handle->is_keyframe;
  uint8_t* data_ptr = encoder_buffer_handle->data.data();
  size_t data_size = encoder_buffer_handle->data.size();

  webrtc::EncodedImage encoded_frame(data_ptr, data_size, data_size);
  encoded_frame._encodedWidth = encoder_buffer_handle->width;
  encoded_frame._encodedHeight = encoder_buffer_handle->height;
  encoded_frame._completeFrame = true;
  encoded_frame._frameType = is_keyframe ? webrtc::VideoFrameType::kVideoFrameKey : webrtc::VideoFrameType::kVideoFrameDelta;
  encoded_frame.capture_time_ms_ = input_image.render_time_ms();
  encoded_frame.SetTimestamp(input_image.timestamp());


  // // Construct the encoded frame
  // uint8_t* data_ptr = nullptr;
  // size_t data_size = 0;
  // if (rand() % 2 == 0) {
  //   data_ptr = const_cast<uint8_t*>(&orange_frame_values[0]);
  //   data_size = sizeof(orange_frame_values);
  //   (void)blue_frame_values;
  // } else {
  //   data_ptr = const_cast<uint8_t*>(&blue_frame_values[0]);
  //   data_size = sizeof(blue_frame_values);
  //   (void)orange_frame_values;
  // }
  // webrtc::EncodedImage encoded_frame(data_ptr, data_size, data_size);
  // encoded_frame._encodedWidth = 320; //input_image.width();
  // encoded_frame._encodedHeight = 240; //input_image.height();
  // encoded_frame._completeFrame = true;
  // encoded_frame._frameType = webrtc::VideoFrameType::kVideoFrameKey;
  // encoded_frame.capture_time_ms_ = input_image.render_time_ms();
  // encoded_frame.SetTimestamp(input_image.timestamp());

  // Construct the codec specific info
  webrtc::CodecSpecificInfo info;
  memset(&info, 0, sizeof(info));
  info.codecType = webrtc::kVideoCodecH264;
  info.codecSpecific.H264.temporal_idx = 0;
  info.codecSpecific.H264.idr_frame = false;
  info.codecSpecific.H264.base_layer_sync = false;

  // Construct the fragmentation header
  webrtc::RTPFragmentationHeader header;
  memset(&header, 0, sizeof(header));
  {
    int32_t scPositions[MAX_NALUS_PERFRAME + 1] = {};
    size_t scLengths[MAX_NALUS_PERFRAME + 1] = {};
    int32_t scPositionsLength = 0;
    int32_t scPosition = 0;
    while (scPositionsLength < MAX_NALUS_PERFRAME) {
      size_t scLength = 0;
      int32_t naluPosition = next_nalu_position(
          data_ptr + scPosition, data_size - scPosition, &scLength);
      if (naluPosition < 0) {
        break;
      }
      scPosition += naluPosition;
      scPositions[scPositionsLength++] = scPosition;
      scLengths[scPositionsLength - 1] = static_cast<int32_t>(scLength);
      scPosition += static_cast<int32_t>(scLength);
    }
    if (scPositionsLength == 0) {
      RTC_LOG(LS_ERROR) << "Start code is not found for H264/H265 codec!";
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
    scPositions[scPositionsLength] = data_size;
    header.VerifyAndAllocateFragmentationHeader(scPositionsLength);
    for (int i = 0; i < scPositionsLength; i++) {
      header.fragmentationOffset[i] = scPositions[i] + scLengths[i];
      header.fragmentationLength[i] = scPositions[i + 1] - header.fragmentationOffset[i];
    }
  }

  // Supply the encoded frame to the callback
  const auto result = callback_->OnEncodedImage(encoded_frame, &info, &header);
  if (result.error != webrtc::EncodedImageCallback::Result::Error::OK) {
    RTC_LOG(LS_ERROR) << "Deliver encoded frame callback failed: "
                      << result.error;
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  return WEBRTC_VIDEO_CODEC_OK;
}


webrtc::VideoEncoder::EncoderInfo PrivateVideoEncoder::GetEncoderInfo() const
{
  EncoderInfo info;
  info.supports_native_handle = true; // this is needed for the fake frame to work
  info.is_hardware_accelerated = false;
  info.has_internal_source = false;
  info.implementation_name = "OWTPassthroughEncoder";
  info.has_trusted_rate_controller = false;
  info.scaling_settings = VideoEncoder::ScalingSettings::kOff;
  return info;
}


////////////////////////////////////////////////////////////////////////////////
// Factory
////////////////////////////////////////////////////////////////////////////////

DualVideoEncoder::DualVideoEncoder()
    : builtin_encoder_factory_(webrtc::CreateBuiltinVideoEncoderFactory())
{}


std::unique_ptr<webrtc::VideoEncoder> DualVideoEncoder::CreateVideoEncoder(
    const webrtc::SdpVideoFormat& format
) {
    RTC_LOG(LS_WARNING) << "[DTAG] " << "Creating video encoder for format: " << format.name;
    auto default_encoder = builtin_encoder_factory_->CreateVideoEncoder(format);
    return std::make_unique<PrivateVideoEncoder>(std::move(default_encoder));
}


std::vector<webrtc::SdpVideoFormat> DualVideoEncoder::GetSupportedFormats() const
{
    RTC_LOG(LS_WARNING) << "[DTAG] " << "Getting supported formats";
    //return builtin_encoder_factory_->GetSupportedFormats();
    std::vector<webrtc::SdpVideoFormat> formats;
    formats.push_back(
      webrtc::SdpVideoFormat(
        cricket::kH264CodecName,
        {{cricket::kH264FmtpProfileLevelId, "42e01f"},
         {cricket::kH264FmtpLevelAsymmetryAllowed, "1"},
         {cricket::kH264FmtpPacketizationMode, "1"}
        }
      )
    );
    return formats;
}


webrtc::VideoEncoderFactory::CodecInfo DualVideoEncoder::QueryVideoEncoder(const webrtc::SdpVideoFormat& format) const
{
    RTC_LOG(LS_WARNING) << "[DTAG] " << "Querying video encoder for format: " << format.name;
    return builtin_encoder_factory_->QueryVideoEncoder(format);
}


} // namespace base
} // namespace owt
