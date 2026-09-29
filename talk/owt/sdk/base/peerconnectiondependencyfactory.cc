// Copyright (C) <2018> Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>
#include "webrtc/api/task_queue/default_task_queue_factory.h"
#include "webrtc/modules/audio_device/include/audio_device.h"
#if defined(WEBRTC_WIN) || defined(WEBRTC_LINUX)
#include "talk/owt/sdk/base/customizedaudiodevicemodule.h"
#endif
#include "talk/owt/sdk/base/encodedvideoencoderfactory.h"
#include "talk/owt/sdk/base/peerconnectiondependencyfactory.h"
#include "webrtc/api/audio_codecs/builtin_audio_decoder_factory.h"
#include "webrtc/api/audio_codecs/builtin_audio_encoder_factory.h"
#include "webrtc/api/create_peerconnection_factory.h"
#include "webrtc/api/video_codecs/builtin_video_decoder_factory.h"
#include "webrtc/api/video_codecs/builtin_video_encoder_factory.h"
#include "webrtc/media/base/media_channel.h"
#if defined(WEBRTC_WIN)
#include "webrtc/modules/audio_device/include/audio_device_factory.h"
#endif
#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/rtc_base/bind.h"
#include "webrtc/rtc_base/ssl_adapter.h"
#include "webrtc/rtc_base/thread.h"
#include "webrtc/rtc_base/logging.h"
#if defined(WEBRTC_LINUX)
// POSIX real-time scheduling for network_thread — Linux only.
#include <pthread.h>
#include <sched.h>
#include <cstring>
#include <cerrno>
#endif
#include "webrtc/system_wrappers/include/field_trial.h"
#if defined(WEBRTC_WIN)
#include "talk/owt/sdk/base/win/msdkvideodecoderfactory.h"
#include "talk/owt/sdk/base/win/msdkvideoencoderfactory.h"
#elif defined(WEBRTC_IOS)
#include "talk/owt/sdk/base/ios/networkmonitorios.h"
#include "talk/owt/sdk/base/objc/ObjcVideoCodecFactory.h"
#endif
#if defined(WEBRTC_LINUX) || defined(WEBRTC_WIN)
#include "talk/owt/sdk/base/customizedvideodecoderfactory.h"
#endif
#if defined(WEBRTC_LINUX)
#include "talk/owt/sdk/base/dualvideoencoder.h"
#endif
#include "owt/base/clientconfiguration.h"
#include "owt/base/globalconfiguration.h"
using namespace rtc;
namespace owt {
namespace base {
void PeerConnectionThread::Run() {
  ProcessMessages(kForever);
}
PeerConnectionThread::~PeerConnectionThread() {
  RTC_LOG(LS_INFO) << "Quit a PeerConnectionThread.";
  Stop();
}
rtc::scoped_refptr<PeerConnectionDependencyFactory>
    PeerConnectionDependencyFactory::dependency_factory_;
std::once_flag get_pcdf_once;
std::once_flag get_shards_once;

namespace {
// FNV-1a, 64-bit. Chosen over std::hash so the peer -> shard mapping is the
// same in every build and every process: logs and dashboards can then be
// read against it, and a restart does not reshuffle peers.
uint64_t Fnv1a64(const std::string& s) {
  uint64_t h = 14695981039346656037ULL;
  for (unsigned char c : s) {
    h ^= c;
    h *= 1099511628211ULL;
  }
  return h;
}

size_t ClampedShardCount() {
  const int requested = GlobalConfiguration::GetPeerConnectionFactoryShards();
  const int max_shards = GlobalConfiguration::kMaxPeerConnectionFactoryShards;
  if (requested < 1) return 1;
  if (requested > max_shards) return static_cast<size_t>(max_shards);
  return static_cast<size_t>(requested);
}

// Thread names are capped at 15 characters by the kernel. Shard 0 keeps the
// historical names so existing tooling that looks threads up by name (the
// [CONN-DIAG][OS] network_thread probe, chrt/top recipes) keeps working.
std::string ShardThreadName(const char* base, const char* short_base,
                            size_t shard) {
  if (shard == 0) return base;
  return std::string(short_base) + "_s" + std::to_string(shard);
}
}  // namespace

PeerConnectionDependencyFactory::PeerConnectionDependencyFactory(
    size_t shard_index)
    : shard_index_(shard_index),
      pc_thread_(rtc::Thread::CreateWithSocketServer()),
      callback_thread_(rtc::Thread::CreateWithSocketServer()),
      field_trial_("WebRTC-H264HighProfile/Enabled/") {
#if defined(WEBRTC_WIN)
  if (GlobalConfiguration::GetVideoHardwareAccelerationEnabled()) {
    render_hardware_acceleration_enabled_ = true;
  } else {
    render_hardware_acceleration_enabled_ = false;
  }
#endif
#if defined(WEBRTC_IOS)
  network_monitor_ = nullptr;
#endif
  encoded_frame_ = GlobalConfiguration::GetEncodedVideoFrameEnabled();
  dual_video_encoder_ = GlobalConfiguration::GetDualVideoEncoderEnabled();
  pc_thread_->SetName(
      ShardThreadName("peerconnection_dependency_factory_thread", "pcdf_thr",
                      shard_index_),
      nullptr);
  pc_thread_->Start();
}
PeerConnectionDependencyFactory::~PeerConnectionDependencyFactory() {}
rtc::scoped_refptr<webrtc::PeerConnectionInterface>
PeerConnectionDependencyFactory::CreatePeerConnection(
    const webrtc::PeerConnectionInterface::RTCConfiguration& config,
    webrtc::PeerConnectionObserver* observer) {
  return pc_thread_
      ->Invoke<scoped_refptr<webrtc::PeerConnectionInterface>>(
          RTC_FROM_HERE, Bind(&PeerConnectionDependencyFactory::
                                  CreatePeerConnectionOnCurrentThread,
                              this, config, observer))
      .get();
}
PeerConnectionDependencyFactory* PeerConnectionDependencyFactory::Get() {
  std::call_once(get_pcdf_once, []() {
    dependency_factory_ =
        new rtc::RefCountedObject<PeerConnectionDependencyFactory>();
    dependency_factory_->CreatePeerConnectionFactory();
  });
  return dependency_factory_.get();
}

const std::vector<PeerConnectionDependencyFactory*>&
PeerConnectionDependencyFactory::Shards() {
  // Heap-allocated and never freed, like the factories in it: shards are
  // process-lifetime, and tearing them down in static destruction order,
  // while their threads may still be running, would be the riskier choice.
  static std::vector<PeerConnectionDependencyFactory*>* shards =
      new std::vector<PeerConnectionDependencyFactory*>();
  std::call_once(get_shards_once, []() {
    const size_t count = ClampedShardCount();
    if (count != static_cast<size_t>(
                     GlobalConfiguration::GetPeerConnectionFactoryShards())) {
      RTC_LOG(LS_ERROR) << "[CONN-DIAG][WARN] event=factory_shards_clamped"
                        << " requested="
                        << GlobalConfiguration::GetPeerConnectionFactoryShards()
                        << " effective=" << count;
    }
    shards->reserve(count);
    // Shard 0 IS the Get() singleton. It is built first, so the process-wide
    // setup it does (SSL, field trials, the real audio device) happens once,
    // before any other shard exists, and with one shard nothing else runs.
    shards->push_back(Get());
    for (size_t i = 1; i < count; ++i) {
      PeerConnectionDependencyFactory* factory =
          new rtc::RefCountedObject<PeerConnectionDependencyFactory>(i);
      factory->AddRef();  // Process-lifetime; the matching Release never runs.
      factory->CreatePeerConnectionFactory();
      shards->push_back(factory);
    }
    // LS_ERROR only because OWT runs at kError; this is informational.
    RTC_LOG(LS_ERROR) << "[CONN-DIAG] event=factory_shards_ready shards="
                      << count;
  });
  return *shards;
}

size_t PeerConnectionDependencyFactory::ShardCount() {
  return Shards().size();
}

size_t PeerConnectionDependencyFactory::ShardIndexForKey(
    const std::string& key) {
  const size_t count = ShardCount();
  if (count <= 1 || key.empty()) return 0;
  return static_cast<size_t>(Fnv1a64(key) % count);
}

PeerConnectionDependencyFactory* PeerConnectionDependencyFactory::GetForKey(
    const std::string& key) {
  return Shards()[ShardIndexForKey(key)];
}
const scoped_refptr<PeerConnectionFactoryInterface>&
PeerConnectionDependencyFactory::GetPeerConnectionFactory() {
  if (!pc_factory_.get())
    CreatePeerConnectionFactory();
  RTC_CHECK(pc_factory_.get());
  return pc_factory_;
}
void PeerConnectionDependencyFactory::
    CreatePeerConnectionFactoryOnCurrentThread() {
  RTC_LOG(LS_INFO) << "CreatePeerConnectionOnCurrentThread";
  // Field trials and SSL are process-wide, not per factory. Only shard 0 sets
  // them up, and shard 0 is always built before any other shard (see
  // Shards()). Beyond being redundant, repeating this per shard is unsafe:
  // InitFieldTrialsFromString() stores the raw c_str() pointer of this
  // factory's own string in a global that every running shard's threads
  // already read, so a second call would swap that pointer under them.
  if (shard_index_ == 0) {
    if (GlobalConfiguration::GetAECEnabled() &&
        GlobalConfiguration::GetAEC3Enabled()) {
      field_trial_ += "OWT-EchoCanceller3/Enabled/";
    }
    // Set H.264 temporal layers. Ideally it should be set via RtpSenderParam
    int h264_temporal_layers = GlobalConfiguration::GetH264TemporalLayers();
    field_trial_ += "OWT-H264TemporalLayers/" +
                    std::to_string(h264_temporal_layers) + std::string("/");
    webrtc::field_trial::InitFieldTrialsFromString(field_trial_.c_str());
    if (!rtc::InitializeSSL()) {
      RTC_LOG(LS_ERROR) << "Failed to initialize SSL.";
      RTC_NOTREACHED();
      return;
    }
  }
  worker_thread = rtc::Thread::CreateWithSocketServer();

  worker_thread->SetName(
      ShardThreadName("worker_thread", "worker_thr", shard_index_), nullptr);
  signaling_thread = rtc::Thread::CreateWithSocketServer();

  signaling_thread->SetName(
      ShardThreadName("signaling_thread", "signal_thr", shard_index_),
      nullptr);
  network_thread = rtc::Thread::CreateWithSocketServer();

  const std::string network_thread_name =
      ShardThreadName("network_thread", "network_thr", shard_index_);
  network_thread->SetName(network_thread_name, nullptr);
  RTC_CHECK(worker_thread->Start() && signaling_thread->Start() &&
            network_thread->Start())
      << "Failed to start threads";

#if defined(WEBRTC_LINUX)
  // [CONN-DIAG] Give network_thread SCHED_RR so it preempts co-located
  // transcoders to answer STUN keepalives; otherwise it starves, the kernel
  // drops keepalives, and the browser declares ICE failed (~15s) and drops the
  // stream. Best-effort: never crashes; logs and continues if it can't apply.
  // Non-throwing: none of pthread_setschedparam / RTC_LOG / strerror throw, and
  // OWT is built with -fno-exceptions, so no try/catch is possible or needed —
  // a scheduling failure just logs and continues, never crashing the binary.
  // Linux-only: pthread_setschedparam / SCHED_RR are POSIX and this binary only
  // ships on Linux appliances.
  auto make_realtime = [](rtc::Thread* t, const std::string& tname) {
    if (t == nullptr) return;
    t->Invoke<void>(RTC_FROM_HERE, [tname]() {
      struct sched_param sp;
      std::memset(&sp, 0, sizeof(sp));
      sp.sched_priority = 20;  // modest RT priority, well below kernel threads
      // pthread_setschedparam returns the error code directly and does NOT set
      // errno — must use the return value, not errno, to report the failure.
      int rc = pthread_setschedparam(pthread_self(), SCHED_RR, &sp);
      if (rc != 0) {
        RTC_LOG(LS_ERROR) << "[CONN-DIAG][ERROR] event=sched_rr_failed thread=" << tname
                          << " prio=20 err=" << std::strerror(rc);
      } else {
        // LS_ERROR (not LS_INFO): OWT is set to kError severity which drops
        // LS_INFO. This is informational, not a real error.
        RTC_LOG(LS_ERROR) << "[CONN-DIAG] event=sched_rr_applied thread=" << tname
                          << " prio=20";
      }
    });
  };
  // ON by default fleet-wide; node config kill-switch (bridged from NodeConfig
  // in main.cc) can disable it on a node where network_thread spins and pegs a
  // core, without a binary rollback.
  // Applies to every shard's network thread, so N shards means N real-time
  // threads when enabled.
  if (GlobalConfiguration::GetNetworkThreadRealtimeEnabled()) {
    make_realtime(network_thread.get(), network_thread_name);
  } else {
    RTC_LOG(LS_ERROR) << "[CONN-DIAG] event=sched_rr_skipped thread="
                      << network_thread_name << " reason=disabled_by_config";
  }
#endif  // WEBRTC_LINUX

  // Use webrtc::VideoEn(De)coderFactory on iOS.
  std::unique_ptr<webrtc::VideoEncoderFactory> encoder_factory;
  std::unique_ptr<webrtc::VideoDecoderFactory> decoder_factory;
#if defined(WEBRTC_IOS)
  encoder_factory = ObjcVideoCodecFactory::CreateObjcVideoEncoderFactory();
  decoder_factory = ObjcVideoCodecFactory::CreateObjcVideoDecoderFactory();
#elif defined(WEBRTC_WIN)
  // Configure codec factories. MSDK factory will internally use built-in codecs
  // if hardware acceleration is not in place. For H.265/H.264, if hardware acceleration
  // is turned off at application level, negotiation will fail.
  if (encoded_frame_) {
    encoder_factory.reset(new EncodedVideoEncoderFactory());
  } else if (render_hardware_acceleration_enabled_) {
    encoder_factory.reset(new MSDKVideoEncoderFactory());
  } else {
    encoder_factory = webrtc::CreateBuiltinVideoEncoderFactory();
  }

  if (GlobalConfiguration::GetCustomizedVideoDecoderEnabled()) {
    decoder_factory.reset(new CustomizedVideoDecoderFactory(
        GlobalConfiguration::GetCustomizedVideoDecoder()));
  } else if (render_hardware_acceleration_enabled_) {
    decoder_factory.reset(new MSDKVideoDecoderFactory());
  } else {
    decoder_factory = webrtc::CreateBuiltinVideoDecoderFactory();
  }

#elif defined(WEBRTC_LINUX)
  // MSDK support for Linux is not in place. Use default.
  if (dual_video_encoder_) {
    RTC_LOG(LS_WARNING) << "Using DualVideoEncoder. The EncodedVideoFrameEnabled configuration is ignored.";
    encoder_factory.reset(new DualVideoEncoder());
  } else if (encoded_frame_) {
    encoder_factory.reset(new EncodedVideoEncoderFactory());
  } else {
    encoder_factory = webrtc::CreateBuiltinVideoEncoderFactory();
  }

  if (GlobalConfiguration::GetCustomizedVideoDecoderEnabled()) {
    decoder_factory.reset(new CustomizedVideoDecoderFactory(
        GlobalConfiguration::GetCustomizedVideoDecoder()));
  } else {
    decoder_factory = webrtc::CreateBuiltinVideoDecoderFactory();
  }
#else
#error "Unsupported platform."
#endif
  rtc::scoped_refptr<AudioDeviceModule> adm;

  if (shard_index_ != 0) {
    // Shards other than 0 cannot reuse the path below. It builds the
    // customized ADM from GlobalConfiguration::GetAudioFrameGenerator(),
    // which std::move()s the single generator out, so shard 0 takes it and
    // every later shard would get nullptr and abort in ADM Init(). (That,
    // not the audio hardware, is why an earlier sharding attempt crashed.)
    // Shard 0 keeps exactly today's audio setup; the others get a dummy
    // device, created on the worker thread like the customized one is.
    adm = worker_thread->Invoke<rtc::scoped_refptr<AudioDeviceModule>>(
        RTC_FROM_HERE,
        Bind(&PeerConnectionDependencyFactory::
                 CreateShardAudioDeviceModuleOnCurrentThread,
             this));
  }
#if defined(WEBRTC_WIN) || defined(WEBRTC_LINUX)
  // Raw audio frame
  // if adm is nullptr, voe_base will initilize it with the default internal
  // adm.
  else if (GlobalConfiguration::GetCustomizedAudioInputEnabled()) {
    // Create ADM on worker thred as RegisterAudioCallback is invoked there.
    adm = worker_thread->Invoke<rtc::scoped_refptr<AudioDeviceModule>>(
        RTC_FROM_HERE,
        Bind(&PeerConnectionDependencyFactory::
                 CreateCustomizedAudioDeviceModuleOnCurrentThread,
             this));
  } else {
#if defined(WEBRTC_WIN)
    // For Widnows we create the audio device with non audio_device_impl
    // dependent factory to facilitate switching of playback devices.
    task_queue_factory_ = CreateDefaultTaskQueueFactory();
    com_initializer_ = std::make_unique<webrtc::ScopedCOMInitializer>(
        webrtc::ScopedCOMInitializer::kMTA);
    if (com_initializer_->succeeded())
      adm = CreateWindowsCoreAudioAudioDeviceModule(
          task_queue_factory_.get(), true);
#endif
  }
#endif

  pc_factory_ = webrtc::CreatePeerConnectionFactory(
      network_thread.get(), worker_thread.get(), signaling_thread.get(), adm,
      webrtc::CreateBuiltinAudioEncoderFactory(),
      webrtc::CreateBuiltinAudioDecoderFactory(), std::move(encoder_factory),
      std::move(decoder_factory), nullptr, nullptr);
  pc_factory_->AddRef();
  RTC_LOG(LS_INFO) << "CreatePeerConnectionOnCurrentThread finished.";
}

scoped_refptr<webrtc::PeerConnectionInterface>
PeerConnectionDependencyFactory::CreatePeerConnectionOnCurrentThread(
    const webrtc::PeerConnectionInterface::RTCConfiguration& config,
    webrtc::PeerConnectionObserver* observer) {
  return (pc_factory_->CreatePeerConnection(config, nullptr, nullptr, observer))
      .get();
}
void PeerConnectionDependencyFactory::CreatePeerConnectionFactory() {
  RTC_CHECK(!pc_factory_.get());
  RTC_LOG(LS_INFO)
      << "PeerConnectionDependencyFactory::CreatePeerConnectionFactory()";
  RTC_CHECK(pc_thread_);
  pc_thread_->Invoke<void>(RTC_FROM_HERE,
                           Bind(&PeerConnectionDependencyFactory::
                                    CreatePeerConnectionFactoryOnCurrentThread,
                                this));
  RTC_CHECK(pc_factory_.get());
}
scoped_refptr<webrtc::MediaStreamInterface>
PeerConnectionDependencyFactory::CreateLocalMediaStream(
    const std::string& label) {
  RTC_CHECK(pc_thread_);
  // pc_factory_ is a proxy: calling it only marshals
  // PeerConnectionFactory::CreateLocalMediaStream onto signaling_thread and
  // does nothing else, so the pc_thread_ hop is a redundant second queue.
  if (GlobalConfiguration::GetWebrtcMessageExecutionOptimizationEnabled()) {
    return pc_factory_->CreateLocalMediaStream(label);
  }
  return pc_thread_->Invoke<scoped_refptr<webrtc::MediaStreamInterface>>(
      RTC_FROM_HERE,
      Bind(&PeerConnectionFactoryInterface::CreateLocalMediaStream,
           pc_factory_.get(), label));
}
scoped_refptr<VideoTrackInterface>
PeerConnectionDependencyFactory::CreateLocalVideoTrack(
    const std::string& id,
    webrtc::VideoTrackSourceInterface* video_source) {
  // Same as CreateLocalMediaStream: the proxy only marshals
  // PeerConnectionFactory::CreateVideoTrack onto signaling_thread and does
  // nothing else, so the pc_thread_ hop is a redundant second queue.
  if (GlobalConfiguration::GetWebrtcMessageExecutionOptimizationEnabled()) {
    return pc_factory_->CreateVideoTrack(id, video_source);
  }
  return pc_thread_
      ->Invoke<scoped_refptr<VideoTrackInterface>>(
          RTC_FROM_HERE, Bind(&PeerConnectionFactoryInterface::CreateVideoTrack,
                              pc_factory_.get(), id, video_source))
      .get();
}
scoped_refptr<AudioTrackInterface>
PeerConnectionDependencyFactory::CreateLocalAudioTrack(const std::string& id) {
  bool aec_enabled, agc_enabled, ns_enabled;
  aec_enabled = GlobalConfiguration::GetAECEnabled();
  agc_enabled = GlobalConfiguration::GetAGCEnabled();
  ns_enabled = GlobalConfiguration::GetNSEnabled();
  if (!aec_enabled || !agc_enabled || !ns_enabled) {
    cricket::AudioOptions options;
    options.echo_cancellation =
        absl::optional<bool>(aec_enabled ? true : false);
    options.auto_gain_control =
        absl::optional<bool>(agc_enabled ? true : false);
    options.noise_suppression = absl::optional<bool>(ns_enabled ? true : false);
    options.residual_echo_detector =
        absl::optional<bool>(aec_enabled ? true : false);
    scoped_refptr<webrtc::AudioSourceInterface> audio_source =
        CreateAudioSource(options);
    return pc_thread_
        ->Invoke<scoped_refptr<AudioTrackInterface>>(
            RTC_FROM_HERE,
            Bind(&PeerConnectionFactoryInterface::CreateAudioTrack,
                 pc_factory_.get(), id, audio_source.get()))
        .get();
  } else {
    return pc_thread_
        ->Invoke<scoped_refptr<AudioTrackInterface>>(
            RTC_FROM_HERE,
            Bind(&PeerConnectionFactoryInterface::CreateAudioTrack,
                 pc_factory_.get(), id, nullptr))
        .get();
  }
}
scoped_refptr<AudioTrackInterface>
PeerConnectionDependencyFactory::CreateLocalAudioTrack(
    const std::string& id,
    webrtc::AudioSourceInterface* audio_source) {
  return pc_thread_
      ->Invoke<scoped_refptr<AudioTrackInterface>>(
          RTC_FROM_HERE, Bind(&PeerConnectionFactoryInterface::CreateAudioTrack,
                              pc_factory_.get(), id, audio_source))
      .get();
}
rtc::scoped_refptr<AudioSourceInterface>
PeerConnectionDependencyFactory::CreateAudioSource(
    const cricket::AudioOptions& options) {
  return pc_thread_
      ->Invoke<scoped_refptr<webrtc::AudioSourceInterface>>(
          RTC_FROM_HERE,
          Bind((rtc::scoped_refptr<AudioSourceInterface>(
                   PeerConnectionFactoryInterface::*)(
                   const cricket::AudioOptions&)) &
                   PeerConnectionFactoryInterface::CreateAudioSource,
               pc_factory_.get(), options))
      .get();
}
rtc::scoped_refptr<PeerConnectionFactoryInterface>
PeerConnectionDependencyFactory::PeerConnectionFactory() const {
  return pc_factory_;
}
rtc::NetworkMonitorInterface*
PeerConnectionDependencyFactory::NetworkMonitor() {
#if defined(WEBRTC_IOS)
  pc_thread_->Invoke<void>(
      RTC_FROM_HERE,
      Bind(
          &PeerConnectionDependencyFactory::CreateNetworkMonitorOnCurrentThread,
          this));
  return network_monitor_;
#else
  return nullptr;
#endif
}
void PeerConnectionDependencyFactory::CreateNetworkMonitorOnCurrentThread() {
#if defined(WEBRTC_IOS)
  if (!network_monitor_) {
    network_monitor_ = new NetworkMonitorIos();
    network_monitor_->Start();
  }
#endif
}

scoped_refptr<webrtc::AudioDeviceModule> PeerConnectionDependencyFactory::
    CreateShardAudioDeviceModuleOnCurrentThread() {
  // A dummy device needs no hardware and no frame generator, so each shard
  // gets its own and they initialise independently. That is sufficient
  // because webrtc_server is video-only: every published stream disables
  // audio, so no shard ever records or plays real audio.
  if (!shard_task_queue_factory_) {
    shard_task_queue_factory_ = webrtc::CreateDefaultTaskQueueFactory();
  }
  rtc::scoped_refptr<webrtc::AudioDeviceModule> adm =
      webrtc::AudioDeviceModule::Create(webrtc::AudioDeviceModule::kDummyAudio,
                                        shard_task_queue_factory_.get());
  RTC_CHECK(adm) << "Failed to create dummy audio device for factory shard "
                 << shard_index_;
  return adm;
}

#if defined(WEBRTC_WIN) || defined(WEBRTC_LINUX)
scoped_refptr<webrtc::AudioDeviceModule> PeerConnectionDependencyFactory::
    CreateCustomizedAudioDeviceModuleOnCurrentThread() {
  return CustomizedAudioDeviceModule::Create(
      GlobalConfiguration::GetAudioFrameGenerator());
}
#endif

}  // namespace base
}  // namespace owt
