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
  /// "signaling_thread", "worker_thread" or "network_thread".
  std::string thread_name;
  /// Milliseconds spent in the dispatch currently running; 0 when the thread is idle.
  int64_t elapsed_ms = 0;
  /// Source file the running message was posted from; empty when idle.
  std::string posted_from_file;
  /// Source line the running message was posted from; 0 when idle.
  int posted_from_line = 0;
};

/**
 @brief Reads what libwebrtc's own threads are doing right now.

 libwebrtc already logs "Message took Nms to dispatch" — but only for messages that
 have COMPLETED, and Thread::Send never times its waiting side. A thread wedged inside
 a handler is therefore invisible to logging: the stall is silence.

 This exposes the dispatch a thread is currently running, so an application-side
 watchdog can observe a wedge while it is still happening and report the call site the
 message was posted from.
*/
class ThreadDiagnostics final {
 public:
  /**
   @brief In-flight dispatch state of the three internal threads.

   Safe to call from any thread. Never constructs the peer-connection factory: returns
   an empty vector if it does not already exist, so polling this on an idle node has no
   side effects. Entries with elapsed_ms == 0 are idle.
   */
  static std::vector<InFlightDispatch> InFlightDispatches();
};

}  // namespace base
}  // namespace owt

#endif  // OWT_BASE_THREADDIAGNOSTICS_H_
