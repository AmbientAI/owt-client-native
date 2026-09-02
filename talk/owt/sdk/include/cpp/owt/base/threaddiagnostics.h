// Copyright (C) <2026> Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0
#ifndef OWT_BASE_THREADDIAGNOSTICS_H_
#define OWT_BASE_THREADDIAGNOSTICS_H_

#include <cstdint>
#include <string>
#include <vector>

namespace owt {
namespace base {

/// In-flight dispatch state of one internal libwebrtc thread.
struct InFlightDispatch {
  /// "pc_thread", "signaling_thread", "worker_thread" or "network_thread".
  std::string thread_name;
  /// OS thread id, for /proc/<tid>; 0 if the thread has not started. Reported even when
  /// idle, since an idle thread's wchan distinguishes waiting from blocked.
  int32_t tid = 0;
  /// Milliseconds in the dispatch currently running; 0 when idle.
  int64_t elapsed_ms = 0;
  /// Source file the running message was posted from; empty when idle.
  std::string posted_from_file;
  /// Source line the running message was posted from; 0 when idle.
  int posted_from_line = 0;
};

/// Reads what libwebrtc's internal threads are doing right now. libwebrtc only logs a
/// dispatch once it COMPLETES, so a thread wedged inside a handler is invisible to it;
/// this exposes the dispatch still running.
class ThreadDiagnostics final {
 public:
  /// Safe from any thread. Never constructs the peer-connection factory, so polling an
  /// idle node is inert. Every live thread gets an entry; elapsed_ms == 0 means idle.
  static std::vector<InFlightDispatch> InFlightDispatches();
};

}  // namespace base
}  // namespace owt

#endif  // OWT_BASE_THREADDIAGNOSTICS_H_
