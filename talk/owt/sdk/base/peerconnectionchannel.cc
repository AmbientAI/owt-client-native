// Copyright (C) <2018> Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0
#include "talk/owt/sdk/base/peerconnectionchannel.h"
#include <vector>
#include "owt/base/globalconfiguration.h"
#include "talk/owt/sdk/base/sdputils.h"
#include "webrtc/rtc_base/logging.h"
#include "webrtc/rtc_base/thread.h"
using namespace rtc;
namespace owt {
namespace base {

// EXPERIMENT (video-wall BWE/temporal scalability). Applied to every video
// sender in ApplyBitrateSettings():
//   #1 min_bitrate_bps  -> floor so the allocator can't starve a wall tile to ~0
//   #1 bitrate_priority -> equal weight so spare bandwidth splits fairly
//   #2 num_temporal_layers -> builtin H.264 encoder produces a temporal hierarchy
//      and drops the top layer (lower fps) under BWE pressure instead of freezing.
// Tunable; flag-gate or remove after validation.
static constexpr int kVideoMinBitrateBps = 500000;     // 500 kbps per stream
static constexpr double kVideoBitratePriority = 1.0;   // equal across tiles
static constexpr int kVideoNumTemporalLayers = 3;
PeerConnectionChannel::PeerConnectionChannel(
    PeerConnectionChannelConfiguration configuration)
    : configuration_(configuration),
      peer_connection_(nullptr),
      factory_(nullptr) {}

PeerConnectionChannel::~PeerConnectionChannel() {
  if (peer_connection_ != nullptr) {
    peer_connection_->Close();
    peer_connection_ = nullptr;
  }
}
bool PeerConnectionChannel::InitializePeerConnection() {
  RTC_LOG(LS_INFO) << "Initialize PeerConnection.";
  if (factory_.get() == nullptr)
    factory_ = PeerConnectionDependencyFactory::Get();
  audio_transceiver_direction_ = webrtc::RtpTransceiverDirection::kSendRecv;
  video_transceiver_direction_ = webrtc::RtpTransceiverDirection::kSendRecv;
  configuration_.enable_dtls_srtp = true;
  configuration_.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
  peer_connection_ =
      (factory_->CreatePeerConnection(configuration_, this)).get();
  if (!peer_connection_.get()) {
    RTC_LOG(LS_ERROR) << "Failed to initialize PeerConnection.";
    RTC_DCHECK(false);
    return false;
  }
  RTC_CHECK(peer_connection_);
  rtc::NetworkMonitorInterface* network_monitor = factory_->NetworkMonitor();
  if (network_monitor) {
    network_monitor->SignalNetworksChanged.connect(
        this, &PeerConnectionChannel::OnNetworksChanged);
  }
  return true;
}
void PeerConnectionChannel::ApplyBitrateSettings() {
  RTC_CHECK(peer_connection_);
  std::vector<rtc::scoped_refptr<webrtc::RtpSenderInterface>> senders =
      peer_connection_->GetSenders();
  if (senders.size() == 0) {
    RTC_LOG(LS_WARNING) << "Cannot set max bitrate without stream added.";
    return;
  }
  for (auto sender : senders) {
    auto sender_track = sender->track();
    if (sender_track == nullptr) {
      continue;
    }
    webrtc::RtpParameters rtp_parameters = sender->GetParameters();
    const bool is_audio =
        sender_track->kind() == webrtc::MediaStreamTrackInterface::kAudioKind;
    const bool is_video =
        sender_track->kind() == webrtc::MediaStreamTrackInterface::kVideoKind;
    bool modified = false;
    for (size_t idx = 0; idx < rtp_parameters.encodings.size(); idx++) {
      // TODO(jianlin): It may not be appropriate to set the same settings
      // on all encodings. Update the logic when upstream implement the
      // the per codec settings, and many other encoding specific setttings
      // should move here instead of modifying SDP.
      if (is_audio) {
        if (configuration_.audio.size() > 0 &&
            configuration_.audio[0].max_bitrate > 0) {
          rtp_parameters.encodings[idx].max_bitrate_bps =
              absl::optional<int>(configuration_.audio[0].max_bitrate * 1024);
          modified = true;
        }
      } else if (is_video) {
        // Original behavior: apply the configured max bitrate (if any).
        if (configuration_.video.size() > 0 &&
            configuration_.video[0].max_bitrate > 0) {
          rtp_parameters.encodings[idx].max_bitrate_bps =
              absl::optional<int>(configuration_.video[0].max_bitrate * 1024);
          modified = true;
        }
        // Gated by node config (enable_video_wall_bwe_tuning): apply the
        // BWE floor, equal priority, and temporal layers to every video
        // encoding so wall tiles degrade gracefully under congestion instead
        // of starving/freezing. See the kVideo* constants above.
        if (owt::base::GlobalConfiguration::GetVideoWallBweTuningEnabled()) {
          rtp_parameters.encodings[idx].min_bitrate_bps =
              absl::optional<int>(kVideoMinBitrateBps);
          rtp_parameters.encodings[idx].bitrate_priority =
              kVideoBitratePriority;
          rtp_parameters.encodings[idx].num_temporal_layers =
              kVideoNumTemporalLayers;
          modified = true;
        }
      }
    }
    if (modified) {
      webrtc::RTCError error = sender->SetParameters(rtp_parameters);
      // NB: logged at LS_ERROR on purpose — this build filters RTC_LOG below
      // LS_ERROR, so LS_INFO/LS_WARNING never reach webrtc_server.log.
      if (!error.ok()) {
        RTC_LOG(LS_ERROR) << "[BWE] SetParameters failed for "
                          << (is_video ? "video" : "audio")
                          << " sender: " << error.message();
      } else if (is_video &&
                 owt::base::GlobalConfiguration::GetVideoWallBweTuningEnabled()) {
        RTC_LOG(LS_ERROR)
            << "[BWE] applied video RTP params: min_bitrate_bps="
            << kVideoMinBitrateBps
            << " bitrate_priority=" << kVideoBitratePriority
            << " num_temporal_layers=" << kVideoNumTemporalLayers
            << " encodings=" << rtp_parameters.encodings.size();
      }
    }
  }
  return;
}

void PeerConnectionChannel::AddTransceiver(
    rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track,
    const webrtc::RtpTransceiverInit& init) {
  peer_connection_->AddTransceiver(track, init);
}

void PeerConnectionChannel::AddTransceiver(
    cricket::MediaType media_type,
    const webrtc::RtpTransceiverInit& init) {
  peer_connection_->AddTransceiver(media_type, init);
}

const webrtc::SessionDescriptionInterface*
PeerConnectionChannel::LocalDescription() {
  RTC_CHECK(peer_connection_);
  return peer_connection_->local_description();
}

PeerConnectionInterface::SignalingState PeerConnectionChannel::SignalingState()
    const {
  RTC_CHECK(peer_connection_);
  return peer_connection_->signaling_state();
}

void PeerConnectionChannel::OnSetRemoteSessionDescriptionSuccess() {
  RTC_LOG(LS_INFO) << "Set remote sdp success.";
  if (peer_connection_->remote_description() &&
      peer_connection_->remote_description()->type() == "offer") {
    CreateAnswer();
  }
}

void PeerConnectionChannel::OnSetRemoteSessionDescriptionFailure(
    const std::string& error) {
  RTC_LOG(LS_INFO) << "Set remote sdp failed.";
}

void PeerConnectionChannel::OnSetRemoteDescriptionComplete(
    webrtc::RTCError error) {
  if (error.ok()) {
    OnSetRemoteSessionDescriptionSuccess();
  } else {
    OnSetRemoteSessionDescriptionFailure(error.message());
  }
}

void PeerConnectionChannel::OnCreateSessionDescriptionSuccess(
    webrtc::SessionDescriptionInterface* desc) {
  RTC_LOG(LS_INFO) << "Create sdp success.";
}
void PeerConnectionChannel::OnCreateSessionDescriptionFailure(
    const std::string& error) {
  RTC_LOG(LS_INFO) << "Create sdp failed.";
}
void PeerConnectionChannel::OnSetLocalSessionDescriptionSuccess() {
  RTC_LOG(LS_INFO) << "Set local sdp success.";
}
void PeerConnectionChannel::OnSetLocalSessionDescriptionFailure(
    const std::string& error) {
  RTC_LOG(LS_INFO) << "Set local sdp failed.";
}
void PeerConnectionChannel::OnIceCandidate(
    const webrtc::IceCandidateInterface* candidate) {}
void PeerConnectionChannel::OnIceCandidatesRemoved(
    const std::vector<cricket::Candidate>& candidates) {}
void PeerConnectionChannel::OnSignalingChange(
    PeerConnectionInterface::SignalingState new_state) {}
void PeerConnectionChannel::OnAddStream(
    rtc::scoped_refptr<MediaStreamInterface> stream) {}
void PeerConnectionChannel::OnRemoveStream(
    rtc::scoped_refptr<MediaStreamInterface> stream) {}
void PeerConnectionChannel::OnDataChannel(
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) {}
void PeerConnectionChannel::OnRenegotiationNeeded() {}
void PeerConnectionChannel::OnIceConnectionChange(
    PeerConnectionInterface::IceConnectionState new_state) {}
void PeerConnectionChannel::OnIceGatheringChange(
    PeerConnectionInterface::IceGatheringState new_state) {}
void PeerConnectionChannel::OnNetworksChanged() {
  RTC_LOG(LS_INFO) << "PeerConnectionChannel::OnNetworksChanged.";
}
PeerConnectionChannelConfiguration::PeerConnectionChannelConfiguration()
    : RTCConfiguration() {}
}  // namespace base
}  // namespace owt
