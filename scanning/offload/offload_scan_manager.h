/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef WIFICOND_OFFLOAD_SCAN_MANAGER_H_
#define WIFICOND_OFFLOAD_SCAN_MANAGER_H_

#include <android/hardware/wifi/offload/1.0/IOffload.h>
#include "wificond/scanning/offload/offload_callback.h"
#include "wificond/scanning/offload/offload_callback_handlers.h"
#include "wificond/scanning/offload/offload_service_utils.h"

#include <vector>

namespace com {
namespace android {
namespace server {
namespace wifi {
namespace wificond {

class NativeScanResult;
class NativeScanStats;

}  // namespace wificond
}  // namespace wifi
}  // namespace server
}  // namespace android
}  // namespace com

namespace android {
namespace wificond {

class OffloadScanManager;

typedef std::function<void(
    const std::vector<::com::android::server::wifi::wificond::NativeScanResult>
        scanResult)>
    OnNativeScanResultsReadyHandler;

// Provides callback interface implementation from Offload HAL
class OffloadCallbackHandlersImpl : public OffloadCallbackHandlers {
 public:
  OffloadCallbackHandlersImpl(OffloadScanManager* parent);
  ~OffloadCallbackHandlersImpl() override;

  void OnScanResultHandler(
      const std::vector<android::hardware::wifi::offload::V1_0::ScanResult>&
          scanResult) override;
  void OnErrorHandler(
      const android::hardware::wifi::offload::V1_0::OffloadStatus& status)
      override;

 private:
  OffloadScanManager* offload_scan_manager_;
};

// Provides methods to interact with Offload HAL
class OffloadScanManager {
 public:
  enum StatusCode {
    /* Corresponds to OffloadStatusCode::OK */
    kNoError,
    /* Offload HAL service not avaialble */
    kNoService,
    /* Corresponds to OffloadStatusCode::NO_CONNECTION */
    kNotConnected,
    /* Corresponds to OffloadStatusCode::TIMEOUT */
    kTimeOut,
    /* Corresponds to OffloadStatusCode::ERROR */
    kError
  };

  enum ReasonCode {
    /* Default value */
    kNone,
    /* Offload HAL scans is not available */
    kNotAvailable,
    /* Offload HAL service is not subscribed to */
    kNotSubscribed,
    /* Offload HAL requested operation failure */
    kOperationFailed,
    /* Binder failed to deliver message to Offload HAL*/
    kTransactionFailed,
  };

  explicit OffloadScanManager(std::weak_ptr<OffloadServiceUtils> utils,
                              OnNativeScanResultsReadyHandler handler);
  virtual ~OffloadScanManager();
  /* Request start of offload scans with scan parameters and scan filter
   * settings. Internally calls Offload HAL service with configureScans()
   * and subscribeScanResults() APIs. If already subscribed, it updates
   * the scan configuration only. Reason code is updated in failure case
   */
  bool startScan(uint32_t /* interval_ms */, int32_t /* rssi_threshold */,
                 const std::vector<std::vector<uint8_t>>& /* scan_ssids */,
                 const std::vector<std::vector<uint8_t>>& /* match_ssids */,
                 const std::vector<uint8_t>& /* match_security */,
                 const std::vector<uint32_t>& /* freqs */,
                 ReasonCode* /* failure reason */);
  /* Request stop of offload scans, returns true if scans were subscribed
   * to from the Offload HAL service. Otherwise, returns false. Reason code
   * is updated in case of failure.
   */
  bool stopScan(ReasonCode* /* failure reason */);
  /* Get statistics for scans performed by Offload HAL */
  bool getScanStats(
      ::com::android::server::wifi::wificond::NativeScanStats* /* scanStats */);
  /* Otain status of the Offload HAL service */
  StatusCode getOffloadStatus() const;

 private:
  void ReportScanResults(
      const std::vector<android::hardware::wifi::offload::V1_0::ScanResult>&
          scanResult);
  void ReportError(
      const android::hardware::wifi::offload::V1_0::OffloadStatus& status);
  bool VerifyAndConvertHIDLStatus(
      std::pair<android::hardware::wifi::offload::V1_0::OffloadStatus, bool>
          result,
      OffloadScanManager::ReasonCode* reason_code);
  bool GetScanStats(
      ::com::android::server::wifi::wificond::NativeScanStats* stats);
  bool SubscribeScanResults(OffloadScanManager::ReasonCode* reason_code);
  bool ConfigureScans(android::hardware::wifi::offload::V1_0::ScanParam,
                      android::hardware::wifi::offload::V1_0::ScanFilter,
                      OffloadScanManager::ReasonCode* reason_code);
  /* Handle binder death */
  void OnObjectDeath(uint64_t /* cookie */);

  android::sp<android::hardware::wifi::offload::V1_0::IOffload>
      wifi_offload_hal_;
  android::sp<OffloadCallback> wifi_offload_callback_;
  android::sp<OffloadDeathRecipient> death_recipient_;
  StatusCode offload_status_;
  bool subscription_enabled_;

  const std::unique_ptr<OffloadCallbackHandlersImpl> offload_callback_handlers_;
  OnNativeScanResultsReadyHandler scan_result_handler_;

  friend class OffloadCallbackHandlersImpl;
};

}  // namespace wificond
}  // namespace android

#endif  // WIFICOND_OFFLOAD_SCAN_MANAGER_H_
