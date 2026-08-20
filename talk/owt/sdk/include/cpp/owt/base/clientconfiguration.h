// Copyright (C) <2018> Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0
#ifndef OWT_BASE_CLIENTCONFIGURATION_H_
#define OWT_BASE_CLIENTCONFIGURATION_H_
#include <vector>
#include <string>
#include "owt/base/commontypes.h"
#include "owt/base/network.h"
namespace owt {
namespace base{
/// Client configurations
struct ClientConfiguration {
  enum class CandidateNetworkPolicy : int { kAll = 1, kLowCost };
  ClientConfiguration()
       : candidate_network_policy(CandidateNetworkPolicy::kAll),
         continual_ice_gathering(true),
         unpublish_on_signaling_thread(false) {}
  /// List of ICE servers
  std::vector<IceServer> ice_servers;
  /**
   @brief Candidate collection policy.
   @details If you do not want cellular network when WiFi is available, please
   use CandidateNetworkPolicy::kLowCost. Using low cost policy may not have good
   network experience. Default policy is collecting all candidates.
   */
  CandidateNetworkPolicy candidate_network_policy;
  /**
   @brief ICE gathering lifetime for each peer connection.
   @details true (default) maps to GATHER_CONTINUALLY: the port allocator stays
   alive and gathers new candidates on network changes for the lifetime of the
   connection. false maps to GATHER_ONCE: candidates are gathered only during
   initial negotiation; recovering from a network change then requires an ICE
   restart.
   */
  bool continual_ice_gathering;
  /**
   @brief Which thread runs the unpublish transceiver walk in DrainPendingStreams.
   @details false (default) leaves it on the calling thread, which is an appliance
   thread-pool worker. Every step of the walk -- transceiver->sender(),
   RemoveTrack, transceiver->Stop() -- is a PROXY method on a
   BEGIN_SIGNALING_PROXY_MAP class, so each one marshals to signaling_thread and
   blocks. The transceiver list only grows, since RemoveTrack stops a transceiver
   but leaves it in place and nothing rebuilds the PeerConnection, so the walk
   lengthens for the life of the connection. Measured on a production node whose
   list had reached 462 entries: 131-307 marshals per teardown at 1239-3325us
   each, 97.9% of a 445ms unpublish, and hangups taking 1-3s.

   true wraps the same walk in one Invoke on signaling_thread. The loop body is
   unchanged; only the thread it runs on differs. Inside that lambda every proxy
   call short-circuits -- SynchronousMethodCall::Invoke calls the handler directly
   when t->IsCurrent(), as does Thread::Send -- so the whole walk costs one
   marshal instead of one per element.
   */
  bool unpublish_on_signaling_thread;
};
}
}
#endif  // OWT_BASE_CLIENTCONFIGURATION_H_
