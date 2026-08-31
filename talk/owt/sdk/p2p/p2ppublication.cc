// Copyright (C) <2018> Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0
#include <algorithm>
#include <atomic>
#include <chrono>
#include "webrtc/rtc_base/third_party/base64/base64.h"
#include "webrtc/rtc_base/critical_section.h"
#include "webrtc/rtc_base/logging.h"
#include "webrtc/rtc_base/task_queue.h"
#include "talk/owt/sdk/base/stringutils.h"
#include "talk/owt/sdk/include/cpp/owt/p2p/p2pclient.h"
#include "talk/owt/sdk/include/cpp/owt/p2p/p2ppublication.h"
namespace owt {
namespace p2p {
P2PPublication::P2PPublication(std::shared_ptr<P2PClient> client, std::string target_id, std::shared_ptr<LocalStream> stream)
    : target_id_(target_id),
      local_stream_(stream),
      p2p_client_(client),
      ended_(false) {
  auto that = p2p_client_.lock();
  if (that != nullptr)
    event_queue_ = that->event_queue_;
}
/// Deprecated. Get connection stats of current publication.
void P2PPublication::GetStats(
    std::function<void(std::shared_ptr<ConnectionStats>)> on_success,
    std::function<void(std::unique_ptr<Exception>)> on_failure) {
  auto that = p2p_client_.lock();
  if (that == nullptr || ended_) {
    std::string failure_message(
       "Session ended.");
    if (on_failure != nullptr) {
      event_queue_->PostTask([on_failure, failure_message]() {
        std::unique_ptr<Exception> e(new Exception(
           ExceptionType::kP2PUnknown, failure_message));
        on_failure(std::move(e));
      });
    }
  } else {
     that->GetConnectionStats(target_id_, on_success, on_failure);
  }
}

/// Get connection stats of current publication.
void P2PPublication::GetStats(
    std::function<void(std::shared_ptr<RTCStatsReport>)> on_success,
    std::function<void(std::unique_ptr<Exception>)> on_failure) {
  auto that = p2p_client_.lock();
  if (that == nullptr || ended_) {
    std::string failure_message("Session ended.");
    if (on_failure != nullptr) {
      event_queue_->PostTask([on_failure, failure_message]() {
        std::unique_ptr<Exception> e(
            new Exception(ExceptionType::kP2PUnknown, failure_message));
        on_failure(std::move(e));
      });
    }
  } else {
    that->GetConnectionStats(target_id_, on_success, on_failure);
  }
}

/// Stop current publication.
void P2PPublication::Stop() {
  // stop_id is the join key for everything this teardown emits. trackid is not
  // usable for that: the same stream is hung up more than once (retry, or a
  // second hangup that finds nothing), so trackid collides across distinct
  // Stop() calls and would merge them. stop_id is minted per call and is
  // unique for the life of the process.
  static std::atomic<uint64_t> stop_seq{0};
  const uint64_t stop_id = stop_seq.fetch_add(1, std::memory_order_relaxed);
  const auto stop_t0 = std::chrono::steady_clock::now();
  const std::string trackid = local_stream_ ? local_stream_->Id() : std::string();
  auto that = p2p_client_.lock();
  const auto lock_us = std::chrono::duration_cast<std::chrono::microseconds>(
                           std::chrono::steady_clock::now() - stop_t0).count();
  if (that == nullptr || ended_) {
    RTC_LOG(LS_ERROR) << "[CONN-DIAG] event=publication_stop_timing peerid=" << target_id_
                      << " stop_id=" << stop_id
                      << " trackid=" << trackid
                      << " early_out=1 client_lock_us=" << lock_us
                      << " unpublish_us=0 onended_us=0 total_us="
                      << std::chrono::duration_cast<std::chrono::microseconds>(
                             std::chrono::steady_clock::now() - stop_t0).count();
    return;
  } else {
    // The whole Unpublish call, which contains DrainPendingStreams. Subtracting
    // drain_timing.total_us from this closes the gap between the appliance's
    // stop_ms and the drain.
    const auto unpub_t0 = std::chrono::steady_clock::now();
    that->Unpublish(target_id_, local_stream_, nullptr, nullptr, stop_id);
    const auto unpublish_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                  std::chrono::steady_clock::now() - unpub_t0).count();
    ended_ = true;
    // Runs after the drain, still inside the appliance's stop_ms. Observer
    // callbacks are arbitrary code, so they are timed rather than assumed cheap.
    const auto onended_t0 = std::chrono::steady_clock::now();
    size_t n_observers = 0;
    {
      const std::lock_guard<std::mutex> lock(observer_mutex_);
      n_observers = observers_.size();
      for (auto its = observers_.begin(); its != observers_.end(); ++its)
        (*its).get().OnEnded();
    }
    const auto onended_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                std::chrono::steady_clock::now() - onended_t0).count();
    RTC_LOG(LS_ERROR) << "[CONN-DIAG] event=publication_stop_timing peerid=" << target_id_
                      << " stop_id=" << stop_id
                      << " trackid=" << trackid
                      << " early_out=0 client_lock_us=" << lock_us
                      << " unpublish_us=" << unpublish_us
                      << " observers=" << n_observers
                      << " onended_us=" << onended_us
                      << " total_us="
                      << std::chrono::duration_cast<std::chrono::microseconds>(
                             std::chrono::steady_clock::now() - stop_t0).count();
  }
}
void P2PPublication::AddObserver(PublicationObserver& observer) {
  const std::lock_guard<std::mutex> lock(observer_mutex_);
  std::vector<std::reference_wrapper<PublicationObserver>>::iterator it =
      std::find_if(
          observers_.begin(), observers_.end(),
          [&](std::reference_wrapper<PublicationObserver> o) -> bool {
            return &observer == &(o.get());
  });
  if (it != observers_.end()) {
    RTC_LOG(LS_WARNING) << "Adding duplicate observer.";
    return;
  }
  observers_.push_back(observer);
}
void P2PPublication::RemoveObserver(PublicationObserver& observer) {
  const std::lock_guard<std::mutex> lock(observer_mutex_);
  auto it = std::find_if(
    observers_.begin(), observers_.end(),
    [&](std::reference_wrapper<PublicationObserver> o) -> bool {
      return &observer == &(o.get());
  });
  if (it == observers_.end()) {
    RTC_LOG(LS_WARNING) << "Trying to delete non-existing observer.";
    return;
  }
  observers_.erase(it);
}
}
}
