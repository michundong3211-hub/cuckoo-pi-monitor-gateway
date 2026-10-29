#ifndef SRC_GATEWAY_PUSH_TEST_POLICY_HPP_
#define SRC_GATEWAY_PUSH_TEST_POLICY_HPP_

#include <stdint.h>

inline bool gatewayPushCandidateIsNewer(bool currentFound,
                                        uint32_t currentAgeMilliseconds,
                                        uint32_t candidateAgeMilliseconds) {
  return !currentFound || candidateAgeMilliseconds <= currentAgeMilliseconds;
}

#endif  // SRC_GATEWAY_PUSH_TEST_POLICY_HPP_
