// Copyright (C) <2018> Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0
#include <algorithm>
#include "webrtc/rtc_base/third_party/base64/base64.h"
#include "webrtc/rtc_base/critical_section.h"
#include "webrtc/rtc_base/logging.h"
#include "webrtc/rtc_base/task_queue.h"
#include "webrtc/rtc_base/time_utils.h"
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
  auto that = p2p_client_.lock();
  if (that == nullptr || ended_) {
    // A no-op Stop() and a fast one are both stop_ms=0 to the caller, so say which.
    RTC_LOG(LS_ERROR) << "[CONN-DIAG] event=stop_noop peerid=" << target_id_
                      << " ended=" << ended_
                      << " client_gone=" << (that == nullptr);
    return;
  } else {
    // remove_stream on a live node attributes 100% of a hangup's cost to this call
    // (185ms p50, 702ms max, every lock in the appliance path at 0-1ms). Split it.
    const int64_t t0 = rtc::TimeMillis();
    that->Unpublish(target_id_, local_stream_, nullptr, nullptr);
    const int64_t t1 = rtc::TimeMillis();
    ended_ = true;
    const std::lock_guard<std::mutex> lock(observer_mutex_);
    const int64_t t2 = rtc::TimeMillis();
    for (auto its = observers_.begin(); its != observers_.end(); ++its)
      (*its).get().OnEnded();
    RTC_LOG(LS_ERROR) << "[CONN-DIAG] event=stop_phases peerid=" << target_id_
                      << " unpublish_ms=" << (t1 - t0)
                      << " observer_lock_ms=" << (t2 - t1)
                      << " on_ended_ms=" << (rtc::TimeMillis() - t2)
                      << " observers=" << observers_.size()
                      << " total_ms=" << (rtc::TimeMillis() - t0);
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
