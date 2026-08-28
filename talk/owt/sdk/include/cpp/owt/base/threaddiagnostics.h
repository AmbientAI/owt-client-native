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
  /// OS thread id, for /proc/<tid>; 0 if the thread has not started. Reported whether or
  /// not the thread is dispatching, because an idle thread's wchan is what separates
  /// "waiting for work" from "blocked acquiring the queue lock".
  int32_t tid = 0;
  /// Milliseconds in the dispatch currently running; 0 when idle.
  int64_t elapsed_ms = 0;
  /// Source file the running message was posted from; empty when idle.
  std::string posted_from_file;
  /// Source line the running message was posted from; 0 when idle.
  int posted_from_line = 0;
};

/**
 @brief Reads what libwebrtc's own threads are doing right now.

 libwebrtc logs "Message took Nms to dispatch", but only once a message COMPLETES, and
 Thread::Send never times its waiting side. A thread wedged inside a handler is therefore
 invisible to logging. This exposes the dispatch still running, so a watchdog can observe
 a wedge while it is happening and report the call site.
*/
class ThreadDiagnostics final {
 public:
  /**
   @brief In-flight dispatch state of the internal threads.

   Safe to call from any thread. Never constructs the peer-connection factory — returns
   an empty vector if it does not already exist — so polling an idle node is inert.
   Every live thread gets an entry; elapsed_ms == 0 means idle, not absent.
   */
  static std::vector<InFlightDispatch> InFlightDispatches();
};

}  // namespace base
}  // namespace owt

#endif  // OWT_BASE_THREADDIAGNOSTICS_H_
