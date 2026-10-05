// Copyright (C) <2018> Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0
#ifndef OWT_BASE_PEERCONNECTIONDEPENDENCYFACTORY_H_
#define OWT_BASE_PEERCONNECTIONDEPENDENCYFACTORY_H_
#include <mutex>
#include <string>
#include <vector>
#include "webrtc/api/peer_connection_interface.h"
#include "webrtc/api/task_queue/task_queue_factory.h"
#include "webrtc/api/media_stream_interface.h"
#if defined(WEBRTC_WIN)
#include "webrtc/api/task_queue/task_queue_factory.h"
#include "webrtc/modules/audio_device/win/audio_device_core_win.h"
#endif
#include "webrtc/sdk/media_constraints.h"
#include "webrtc/rtc_base/bind.h"
namespace owt {
namespace base {
using webrtc::MediaStreamInterface;
using webrtc::AudioDeviceModule;
using webrtc::AudioTrackInterface;
using webrtc::AudioSourceInterface;
using webrtc::VideoTrackInterface;
using webrtc::VideoTrackSourceInterface;
using webrtc::PeerConnectionFactoryInterface;
using webrtc::MediaConstraints;
using rtc::scoped_refptr;
using rtc::Thread;
using rtc::Bind;
// PeerConnectionThread allows blocking calls so other thread can invoke
// synchronized methods on this thread.
class PeerConnectionThread : public rtc::Thread {
 public:
  virtual void Run() override;
  ~PeerConnectionThread() override;
};
// Object factory for WebRTC PeerConnections.
class PeerConnectionDependencyFactory : public rtc::RefCountInterface {
 public:
  // Get a PeerConnectionDependencyFactory instance. It doesn't create a new
  // instance. It always return the same instance. This is also shard 0.
  static PeerConnectionDependencyFactory* Get();
  // Returns the factory shard that owns |key| (a peer id). Every shard has its
  // own network, worker and signaling threads, so spreading peers across
  // shards spreads their ICE/DTLS/RTP, media and SDP work across threads.
  //
  // The mapping is a pure function of |key| and the shard count, so the same
  // peer always lands on the same shard. That is load-bearing: a track made
  // by one factory must not be added to a PeerConnection of another, because
  // their proxies are bound to different threads. A peer's streams and its
  // PeerConnection must therefore be created with the same key.
  //
  // With one shard (the default), and for an empty key, this is Get().
  static PeerConnectionDependencyFactory* GetForKey(const std::string& key);
  // Number of shards, latched from GlobalConfiguration on first use.
  static size_t ShardCount();
  // Shard index GetForKey(|key|) resolves to. Exposed for logging and tests.
  static size_t ShardIndexForKey(const std::string& key);
  // Same index as ShardIndexForKey(), from the same code, but never builds a
  // factory: before the shards exist it uses the count they will be built
  // with. For callers that only label work (logs) and may run before the
  // first PeerConnection exists.
  static size_t PeekShardIndexForKey(const std::string& key);
  size_t shard_index() const { return shard_index_; }
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> CreatePeerConnection(
      const webrtc::PeerConnectionInterface::RTCConfiguration& config,
      webrtc::PeerConnectionObserver* observer);
  rtc::scoped_refptr<MediaStreamInterface> CreateLocalMediaStream(
      const std::string& label);
  rtc::scoped_refptr<AudioTrackInterface> CreateLocalAudioTrack(
      const std::string& id);
  // Make it public to allow passing in user-defined AudioSource.
  rtc::scoped_refptr<AudioTrackInterface> CreateLocalAudioTrack(
      const std::string& id,
      webrtc::AudioSourceInterface* audio_source);
  rtc::scoped_refptr<VideoTrackInterface> CreateLocalVideoTrack(
      const std::string& id,
      webrtc::VideoTrackSourceInterface* video_source);
  rtc::scoped_refptr<AudioSourceInterface> CreateAudioSource(
      const cricket::AudioOptions& options);
  rtc::NetworkMonitorInterface* NetworkMonitor();
  // Returns current |pc_factory_|.
  rtc::scoped_refptr<PeerConnectionFactoryInterface> PeerConnectionFactory()
      const;
  // The thread every PeerConnection proxy method marshals to. Exposed so a caller
  // holding several proxy handles can do its work in one Invoke rather than one
  // marshal per call.
  rtc::Thread* SignalingThread() const { return signaling_thread.get(); }
  ~PeerConnectionDependencyFactory();
 protected:
  explicit PeerConnectionDependencyFactory(size_t shard_index = 0);
  virtual const rtc::scoped_refptr<PeerConnectionFactoryInterface>&
  GetPeerConnectionFactory();
 private:
  // All shards, index 0 being Get(). Built once, never destroyed.
  static const std::vector<PeerConnectionDependencyFactory*>& Shards();
  // Effective shard count: the configured value clamped to [1, 16], and 1 when
  // the app set a customized video decoder (see ClampedShardCount in the .cc).
  // A member so it may read GlobalConfiguration's private decoder state.
  static size_t EffectiveShardCount();
  // Audio device for shards other than 0. See the .cc for why.
  rtc::scoped_refptr<webrtc::AudioDeviceModule>
  CreateShardAudioDeviceModuleOnCurrentThread();
  const size_t shard_index_;
  // Owns the task queues of a non-zero shard's dummy audio device. Lives as
  // long as the factory, which is as long as the process.
  std::unique_ptr<webrtc::TaskQueueFactory> shard_task_queue_factory_;
  // Create a PeerConnectionDependencyFactory instance.
  // static rtc::scoped_refptr<PeerConnectionDependencyFactory> Create();
  void CreatePeerConnectionFactory();
  void CreatePeerConnectionFactoryOnCurrentThread();
  rtc::scoped_refptr<webrtc::PeerConnectionInterface>
  CreatePeerConnectionOnCurrentThread(
      const webrtc::PeerConnectionInterface::RTCConfiguration& config,
      webrtc::PeerConnectionObserver* observer);
  void CreateNetworkMonitorOnCurrentThread();
#if defined(WEBRTC_WIN) || defined(WEBRTC_LINUX)
  rtc::scoped_refptr<webrtc::AudioDeviceModule> CreateCustomizedAudioDeviceModuleOnCurrentThread();
#endif
  scoped_refptr<PeerConnectionFactoryInterface> pc_factory_;
  static scoped_refptr<PeerConnectionDependencyFactory>
      dependency_factory_;  // Get() always return this instance.
  // This thread performs all operations on pcfactory and pc.
  std::unique_ptr<Thread> pc_thread_;
  // This thread performs all callbacks.
  std::unique_ptr<Thread> callback_thread_;
  std::unique_ptr<rtc::Thread> worker_thread;
  std::unique_ptr<rtc::Thread> signaling_thread;
  std::unique_ptr<rtc::Thread> network_thread;
#if defined(WEBRTC_WIN)
  bool render_hardware_acceleration_enabled_;  // Enabling HW acceleration for
                                               // VP8, H.264 & HEVC enc/dec
#endif
  bool encoded_frame_;
  bool dual_video_encoder_;
#if defined(WEBRTC_IOS)
  rtc::NetworkMonitorInterface* network_monitor_;
#endif
  std::string field_trial_;
#if defined(WEBRTC_WIN)
  std::unique_ptr<webrtc::ScopedCOMInitializer> com_initializer_;
  std::unique_ptr<webrtc::TaskQueueFactory> task_queue_factory_;
#endif
};
}
}  // namespace owt
#endif  // OWT_BASE_PEERCONNECTIONDEPENDENCYFACTORY_H_
