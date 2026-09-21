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
         continual_ice_gathering(true) {}
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
};
}
}
#endif  // OWT_BASE_CLIENTCONFIGURATION_H_
