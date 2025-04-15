// Copyright (C) <2018> Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0

#include <string>
#include <vector>
#include "webrtc/api/video/video_frame.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/modules/video_coding/include/video_codec_interface.h"
#include "webrtc/modules/video_coding/include/video_error_codes.h"
#include "webrtc/rtc_base/buffer.h"
#include "webrtc/rtc_base/checks.h"
#include "webrtc/rtc_base/logging.h"
#include "talk/owt/sdk/base/customizedencoderbufferhandle.h"
#include "talk/owt/sdk/base/customizedvideoencoderproxy.h"
#include "talk/owt/sdk/base/mediautils.h"
#include "talk/owt/sdk/base/nativehandlebuffer.h"
#include "talk/owt/sdk/include/cpp/owt/base/commontypes.h"
// H.264 start code length.
#define H264_SC_LENGTH 4
// Maximum allowed NALUs in one output frame.
#define MAX_NALUS_PERFRAME 32
using namespace rtc;


// static std::string VideoCodecToString(const webrtc::VideoCodec& codec) {
//   char string_buf[2048];
//   SimpleStringBuilder ss(string_buf);
//   ss << "VideoCodec {" << "type: " << CodecTypeToPayloadString(codec.codecType)
//      << ", mode: "
//      << (codec.mode == VideoCodecMode::kRealtimeVideo ? "RealtimeVideo"
//                                                 : "Screensharing");
//   if (codec.IsSinglecast()) {
//     ss << ", Singlecast: {" << codec.width << "x" << codec.height << " "
//        << ScalabilityModeToString(codec.GetScalabilityMode())
//        << (codec.active ? ", active" : ", inactive") << "}";
//   } else {
//     ss << ", Simulcast: {";
//     for (size_t i = 0; i < codec.numberOfSimulcastStreams; ++i) {
//       const SimulcastStream stream = codec.simulcastStream[i];
//       ss << "[" << stream.width << "x" << stream.height << " "
//          << ScalabilityModeToString(stream.GetScalabilityMode())
//          << (stream.active ? ", active" : ", inactive") << "]";
//     }
//     ss << "}";
//   }
//   ss << "}";
//   return ss.str();
// }


namespace owt {
namespace base {
CustomizedVideoEncoderProxy::CustomizedVideoEncoderProxy()
    : callback_(nullptr), external_encoder_(nullptr) {
  RTC_LOG(LS_ERROR) << "CustomizedVideoEncoderProxy::(constructor)";
  picture_id_ = 0;
}
CustomizedVideoEncoderProxy::~CustomizedVideoEncoderProxy() {
  RTC_LOG(LS_ERROR) << "CustomizedVideoEncoderProxy::(destructor)";
  if (external_encoder_) {
    delete external_encoder_;
    external_encoder_ = nullptr;
  }
}
int CustomizedVideoEncoderProxy::InitEncode(
    const webrtc::VideoCodec* codec_settings,
    int number_of_cores,
    size_t max_payload_size) {
  RTC_LOG(LS_ERROR) << "CustomizedVideoEncoderProxy::InitEncode():"
    << " codec_settings: " << codec_settings->codecType
    << " number_of_cores: " << number_of_cores
    << " max_payload_size: " << max_payload_size;
  RTC_DCHECK(codec_settings);
  codec_type_ = codec_settings->codecType;
  width_ = codec_settings->width;
  height_ = codec_settings->height;
  bitrate_ = codec_settings->startBitrate * 1000;
  picture_id_ = static_cast<uint16_t>(rand()) & 0x7FFF;
  gof_.SetGofInfoVP9(TemporalStructureMode::kTemporalStructureMode1);
  gof_idx_ = 0;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t CustomizedVideoEncoderProxy::Encode(
    const webrtc::VideoFrame& input_image,
    const std::vector<webrtc::VideoFrameType>* frame_types) {
  RTC_LOG(LS_ERROR) << "CustomizedVideoEncoderProxy::Encode()";

  // Dummy Frame
  const uint8_t orange_frame_values[] = {
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
  const uint8_t blue_frame_values[] = {
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
  uint8_t* fake_data_ptr = nullptr;
  size_t fake_data_size = 0;
  if (rand() % 2 == 0) {
    fake_data_ptr = const_cast<uint8_t*>(&orange_frame_values[0]);
    fake_data_size = sizeof(orange_frame_values);
    (void)blue_frame_values;
  } else {
    fake_data_ptr = const_cast<uint8_t*>(&blue_frame_values[0]);
    fake_data_size = sizeof(blue_frame_values);
    (void)orange_frame_values;
  }
  webrtc::EncodedImage dummy_encodedframe(fake_data_ptr, fake_data_size, fake_data_size);
  dummy_encodedframe._encodedWidth = 1920; //input_image.width();
  dummy_encodedframe._encodedHeight = 1080; //input_image.height();
  dummy_encodedframe._completeFrame = true;
  dummy_encodedframe.capture_time_ms_ = input_image.render_time_ms();
  dummy_encodedframe.SetTimestamp(input_image.timestamp());
  dummy_encodedframe._frameType = webrtc::VideoFrameType::kVideoFrameKey;

  // Dummy Info
  webrtc::CodecSpecificInfo dummy_info;
  memset(&dummy_info, 0, sizeof(dummy_info));
  dummy_info.codecType = codec_type_;
  dummy_info.codecSpecific.H264.temporal_idx = 0;
  dummy_info.codecSpecific.H264.idr_frame = false;
  dummy_info.codecSpecific.H264.base_layer_sync = false;

  // Dummy Header
  webrtc::RTPFragmentationHeader dummy_header;
  memset(&dummy_header, 0, sizeof(dummy_header));
  {
    int32_t scPositions[MAX_NALUS_PERFRAME + 1] = {};
    size_t scLengths[MAX_NALUS_PERFRAME + 1] = {};
    int32_t scPositionsLength = 0;
    int32_t scPosition = 0;
    while (scPositionsLength < MAX_NALUS_PERFRAME) {
      size_t scLength = 0;
      int32_t naluPosition = NextNaluPosition(
          fake_data_ptr + scPosition, fake_data_size - scPosition, &scLength);
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
    scPositions[scPositionsLength] = fake_data_size;
    dummy_header.VerifyAndAllocateFragmentationHeader(scPositionsLength);
    for (int i = 0; i < scPositionsLength; i++) {
      dummy_header.fragmentationOffset[i] = scPositions[i] + scLengths[i];
      dummy_header.fragmentationLength[i] =
          scPositions[i + 1] - dummy_header.fragmentationOffset[i];
    }
  }

  // Finish up
  const auto fake_result = callback_->OnEncodedImage(dummy_encodedframe, &dummy_info, &dummy_header);
  if (fake_result.error != webrtc::EncodedImageCallback::Result::Error::OK) {
    RTC_LOG(LS_ERROR) << "Deliver encoded frame callback failed: "
                      << fake_result.error;
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  return WEBRTC_VIDEO_CODEC_OK;


  assert(false);
  return WEBRTC_VIDEO_CODEC_OK;
  // Get the videoencoderinterface instance from the input video frame.
  CustomizedEncoderBufferHandle* encoder_buffer_handle =
      reinterpret_cast<CustomizedEncoderBufferHandle*>(
          static_cast<owt::base::EncodedFrameBuffer*>(
              input_image.video_frame_buffer().get())
              ->native_handle());
  if (external_encoder_ == nullptr && encoder_buffer_handle != nullptr &&
      encoder_buffer_handle->encoder != nullptr) {
    // First time we get passed in encoder impl. Initialize it. Use codec
    // settings in the natvie handle instead of that passed uplink.
    external_encoder_ = encoder_buffer_handle->encoder->Copy();
    if (external_encoder_ == nullptr) {
      RTC_LOG(LS_ERROR) << "Fail to duplicate video encoder";
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
    size_t width = encoder_buffer_handle->width;
    size_t height = encoder_buffer_handle->height;
    uint32_t fps = encoder_buffer_handle->fps;
    uint32_t bitrate_kbps = encoder_buffer_handle->bitrate_kbps;
    // TODO(jianlin): Add support for H265 and VP9. For VP9/HEVC since the
    // RTPFragmentation information must be extracted by parsing the bitstream,
    // we commented out the support of them temporarily.
    VideoCodec media_codec;
    if (codec_type_ == webrtc::kVideoCodecH264)
      media_codec = VideoCodec::kH264;
    else if (codec_type_ == webrtc::kVideoCodecVP8)
      media_codec = VideoCodec::kVp8;
#ifndef DISABLE_H265
    else if (codec_type_ == webrtc::kVideoCodecH265)
      media_codec = VideoCodec::kH265;
#endif
    else if (codec_type_ == webrtc::kVideoCodecVP9)
      media_codec = VideoCodec::kVp9;
    else {  // Not matching any supported format.
      RTC_LOG(LS_ERROR) << "Requested encoding format not supported";
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
    Resolution resolution(static_cast<int>(width), static_cast<int>(height));
    if (!external_encoder_->InitEncoderContext(resolution, fps, bitrate_kbps,
                                               media_codec)) {
      RTC_LOG(LS_ERROR) << "Failed to init external encoder context";
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
  } else if (encoder_buffer_handle != nullptr &&
             encoder_buffer_handle->encoder == nullptr) {
    RTC_LOG(LS_ERROR) << "Invalid external encoder passed.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  } else if (encoder_buffer_handle == nullptr) {
    RTC_LOG(LS_ERROR) << "Invalid native handle passed.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  } else {  // normal case.
#ifndef DISABLE_H265
    if (codec_type_ != webrtc::kVideoCodecH264 &&
        codec_type_ != webrtc::kVideoCodecVP8 &&
        codec_type_ != webrtc::kVideoCodecVP9 &&
        codec_type_ != webrtc::kVideoCodecH265)
#else
    if (codec_type_ != webrtc::kVideoCodecH264 &&
        codec_type_ != webrtc::kVideoCodecVP8 &&
        codec_type_ != webrtc::kVideoCodecVP9)
#endif
      return WEBRTC_VIDEO_CODEC_ERROR;
  }
  std::vector<uint8_t> buffer;
  bool request_key_frame = false;
  if (frame_types) {
    for (auto frame_type : *frame_types) {
      if (frame_type == webrtc::VideoFrameType::kVideoFrameKey) {
        request_key_frame = true;
        break;
      }
    }
  }
#ifdef WEBRTC_ANDROID
  uint8_t* data_ptr = nullptr;
  uint32_t data_size = 0;
  if (external_encoder_) {
    data_size = external_encoder_->EncodeOneFrame(request_key_frame, &data_ptr);
  }
  if (data_ptr == nullptr) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  webrtc::EncodedImage encodedframe(data_ptr, data_size, data_size);
#else
  if (external_encoder_) {
    if (!external_encoder_->EncodeOneFrame(buffer, request_key_frame))
      return WEBRTC_VIDEO_CODEC_ERROR;
  }
  std::unique_ptr<uint8_t[]> data(new uint8_t[buffer.size()]);
  uint8_t* data_ptr = data.get();
  uint32_t data_size = static_cast<uint32_t>(buffer.size());
  std::copy(buffer.begin(), buffer.end(), data_ptr);
  webrtc::EncodedImage encodedframe(data_ptr, buffer.size(), buffer.size());
#endif
  encodedframe._encodedWidth = input_image.width();
  encodedframe._encodedHeight = input_image.height();
  encodedframe._completeFrame = true;
  encodedframe.capture_time_ms_ = input_image.render_time_ms();
  encodedframe.SetTimestamp(input_image.timestamp());
  // VP9 requires setting the frame type according to actual frame type.
  if (codec_type_ == webrtc::kVideoCodecVP9 && data_size > 2) {
    uint8_t au_key = 1;
    uint8_t first_byte = data_ptr[0], second_byte = data_ptr[1];
    uint8_t shift_bits = 4, profile = (first_byte >> shift_bits) & 0x3;
    shift_bits = (profile == 3) ? 2 : 3;
    uint8_t show_existing_frame = (first_byte >> shift_bits) & 0x1;
    if (profile == 3 && show_existing_frame) {
      au_key = (second_byte >> 6) & 0x1;
    } else if (profile == 3 && !show_existing_frame) {
      au_key = (first_byte >> 1) & 0x1;
    } else if (profile != 3 && show_existing_frame) {
      au_key = second_byte >> 7;
    } else {
      au_key = (first_byte >> 2) & 0x1;
    }
    encodedframe._frameType = (au_key == 0) ? webrtc::VideoFrameType::kVideoFrameKey : webrtc::VideoFrameType::kVideoFrameDelta;
  }
  webrtc::CodecSpecificInfo info;
  memset(&info, 0, sizeof(info));
  info.codecType = codec_type_;
  if (codec_type_ == webrtc::kVideoCodecVP8) {
    info.codecSpecific.VP8.nonReference = false;
    info.codecSpecific.VP8.temporalIdx = webrtc::kNoTemporalIdx;
    info.codecSpecific.VP8.layerSync = false;
    info.codecSpecific.VP8.keyIdx = webrtc::kNoKeyIdx;
    picture_id_ = (picture_id_ + 1) & 0x7FFF;
  } else if (codec_type_ == webrtc::kVideoCodecVP9) {
    bool key_frame = encodedframe._frameType == webrtc::VideoFrameType::kVideoFrameKey;
    if (key_frame) {
      gof_idx_ = 0;
    }
    info.codecSpecific.VP9.inter_pic_predicted = key_frame ? false : true;
    info.codecSpecific.VP9.flexible_mode = false;
    info.codecSpecific.VP9.ss_data_available = key_frame ? true : false;
    info.codecSpecific.VP9.temporal_idx = kNoTemporalIdx;
    //info.codecSpecific.VP9.spatial_idx = kNoSpatialIdx;
    info.codecSpecific.VP9.temporal_up_switch = true;
    info.codecSpecific.VP9.inter_layer_predicted = false;
    info.codecSpecific.VP9.gof_idx =
        static_cast<uint8_t>(gof_idx_++ % gof_.num_frames_in_gof);
    info.codecSpecific.VP9.num_spatial_layers = 1;
    info.codecSpecific.VP9.first_frame_in_picture = true;
    info.codecSpecific.VP9.end_of_picture = true;
    info.codecSpecific.VP9.spatial_layer_resolution_present = false;
    if (info.codecSpecific.VP9.ss_data_available) {
      info.codecSpecific.VP9.spatial_layer_resolution_present = true;
      info.codecSpecific.VP9.width[0] = width_;
      info.codecSpecific.VP9.height[0] = height_;
      info.codecSpecific.VP9.gof.CopyGofInfoVP9(gof_);
    }

  } else if (codec_type_ == webrtc::kVideoCodecH264) {
    int temporal_id = 0, priority_id = 0;
    bool is_idr = false;
    bool need_frame_marking = MediaUtils::GetH264TemporalInfo(
        data_ptr, data_size, temporal_id, priority_id, is_idr);
    if (need_frame_marking) {
      info.codecSpecific.H264.temporal_idx = temporal_id;
      info.codecSpecific.H264.idr_frame = is_idr;
      info.codecSpecific.H264.base_layer_sync = (!is_idr && (temporal_id > 0));
    }
  }
  // Generate a header describing a single fragment.
  webrtc::RTPFragmentationHeader header;
  memset(&header, 0, sizeof(header));
  if (codec_type_ == webrtc::kVideoCodecVP8 ||
      codec_type_ == webrtc::kVideoCodecVP9) {
    header.VerifyAndAllocateFragmentationHeader(1);
    header.fragmentationOffset[0] = 0;
    header.fragmentationLength[0] = encodedframe.size();
#ifndef DISABLE_H265
  } else if (codec_type_ == webrtc::kVideoCodecH264 ||
             codec_type_ == webrtc::kVideoCodecH265) {
#else
  } else if (codec_type_ == webrtc::kVideoCodecH264) {
#endif
    // For H.264/H.265 search for start codes.
    int32_t scPositions[MAX_NALUS_PERFRAME + 1] = {};
    size_t scLengths[MAX_NALUS_PERFRAME + 1] = {};
    int32_t scPositionsLength = 0;
    int32_t scPosition = 0;
    while (scPositionsLength < MAX_NALUS_PERFRAME) {
      size_t scLength = 0;
      int32_t naluPosition = NextNaluPosition(
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
      header.fragmentationLength[i] =
          scPositions[i + 1] - header.fragmentationOffset[i];
    }
  }
  // Finish up
  const auto result = callback_->OnEncodedImage(encodedframe, &info, &header);
  if (result.error != webrtc::EncodedImageCallback::Result::Error::OK) {
    RTC_LOG(LS_ERROR) << "Deliver encoded frame callback failed: "
                      << result.error;
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  return WEBRTC_VIDEO_CODEC_OK;
}
int CustomizedVideoEncoderProxy::RegisterEncodeCompleteCallback(
    webrtc::EncodedImageCallback* callback) {
  RTC_LOG(LS_ERROR) << "CustomizedVideoEncoderProxy::RegisterEncodeCompleteCallback()";
  callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

void CustomizedVideoEncoderProxy::SetRates(
    const RateControlParameters& parameters) {
  RTC_LOG(LS_ERROR) << "CustomizedVideoEncoderProxy::SetRates()";
  if (parameters.framerate_fps < 1.0) {
    RTC_LOG(LS_WARNING) << "Unsupported framerate (must be >= 1.0";
    return;
  }
}

void CustomizedVideoEncoderProxy::OnPacketLossRateUpdate(
    float packet_loss_rate) {
  // Currently not handled.
  return;
}

void CustomizedVideoEncoderProxy::OnRttUpdate(int64_t rtt_ms) {
  // Currently not handled.
  return;
}

void CustomizedVideoEncoderProxy::OnLossNotification(
    const LossNotification& loss_notification) {
  // Currently not handled.
}

int CustomizedVideoEncoderProxy::Release() {
  RTC_LOG(LS_ERROR) << "CustomizedVideoEncoderProxy::Release()";
  callback_ = nullptr;
  if (external_encoder_ != nullptr) {
    external_encoder_->Release();
  }
  return WEBRTC_VIDEO_CODEC_OK;
}
int32_t CustomizedVideoEncoderProxy::NextNaluPosition(uint8_t* buffer,
                                                      size_t buffer_size,
                                                      size_t* sc_length) {
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

webrtc::VideoEncoder::EncoderInfo CustomizedVideoEncoderProxy::GetEncoderInfo()
    const {
  RTC_LOG(LS_ERROR) << "CustomizedVideoEncoderProxy::GetEncoderInfo()";
  EncoderInfo info;
  info.supports_native_handle = true;
  info.is_hardware_accelerated = false;
  info.has_internal_source = false;
  info.implementation_name = "OWTPassthroughEncoder";
  info.has_trusted_rate_controller = false;
  info.scaling_settings = VideoEncoder::ScalingSettings::kOff;
  return info;
}

std::unique_ptr<CustomizedVideoEncoderProxy>
CustomizedVideoEncoderProxy::Create() {
  RTC_LOG(LS_ERROR) << "CustomizedVideoEncoderProxy::Create()";
  return absl::make_unique<CustomizedVideoEncoderProxy>();
}

}  // namespace base
}  // namespace owt
