#pragma once

class GatewayConfigBootState {
 public:
  bool markRecoveryResult(bool succeeded) {
    _storage_ready = succeeded;
    return succeeded;
  }
  bool storageReady() const { return _storage_ready; }
  void disableStorage() { _storage_ready = false; }

 private:
  bool _storage_ready = false;
};
