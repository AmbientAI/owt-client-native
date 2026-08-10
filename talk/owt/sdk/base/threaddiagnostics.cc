// Copyright (C) <2026> Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0
#include "owt/base/threaddiagnostics.h"

#include "talk/owt/sdk/base/peerconnectiondependencyfactory.h"

namespace owt {
namespace base {

std::vector<InFlightDispatch> ThreadDiagnostics::InFlightDispatches() {
  // PeekExisting(), never Get(): Get() lazily constructs the factory and calls
  // CreatePeerConnectionFactory(), so polling diagnostics through it would spin up
  // libwebrtc on a node that has never published anything.
  PeerConnectionDependencyFactory* factory =
      PeerConnectionDependencyFactory::PeekExisting();
  if (factory == nullptr) {
    return std::vector<InFlightDispatch>();
  }
  return factory->InFlightDispatches();
}

}  // namespace base
}  // namespace owt
