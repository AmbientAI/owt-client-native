// Copyright (C) <2018> Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0
#include "owt/base/globalconfiguration.h"
#include "talk/owt/sdk/base/peerconnectiondependencyfactory.h"
#include "webrtc/api/ambient_flags.h"
namespace owt {
namespace base {
#if defined(WEBRTC_WIN)
// Enable hardware acceleration by default is on.
bool GlobalConfiguration::hardware_acceleration_enabled_ = true;
#endif
bool GlobalConfiguration::encoded_frame_ = false;
bool GlobalConfiguration::dual_video_encoder_ = false;
bool GlobalConfiguration::bwe_optimization_settings_enabled_ = false;
bool GlobalConfiguration::network_thread_realtime_enabled_ = false;
bool GlobalConfiguration::webrtc_message_execution_optimization_enabled_ = false;
int GlobalConfiguration::peer_connection_factory_shards_ = 1;
// Out-of-line definition: owt builds as C++14, where an ODR-used static
// constexpr member (e.g. passed to std::min by reference) needs one.
constexpr int GlobalConfiguration::kMaxPeerConnectionFactoryShards;

int GlobalConfiguration::GetPeerConnectionFactoryShardForKey(
    const std::string& key) {
  return static_cast<int>(
      PeerConnectionDependencyFactory::PeekShardIndexForKey(key));
}
std::unique_ptr<AudioFrameGeneratorInterface>
    GlobalConfiguration::audio_frame_generator_ = nullptr;
std::unique_ptr<VideoDecoderInterface>
    GlobalConfiguration::video_decoder_ = nullptr;
int GlobalConfiguration::h264_temporal_layers_ = 1;
#if defined(WEBRTC_IOS)
AudioProcessingSettings GlobalConfiguration::audio_processing_settings_ = {
    true, true, true, false};
#else
AudioProcessingSettings GlobalConfiguration::audio_processing_settings_ = {
    true, true, true, true};
#endif
}
}

void owt::base::GlobalConfiguration::SetWebrtcMessageExecutionOptimizationEnabled(bool enabled) {
  webrtc_message_execution_optimization_enabled_ = enabled;
  webrtc::AmbientFlags::SetMessageExecutionOptimization(enabled);
}
