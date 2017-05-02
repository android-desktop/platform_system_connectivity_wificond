/*
 * Copyright (C) 2016, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <functional>
#include <memory>
#include <vector>
#include <string>

#include <gtest/gtest.h>
#include <android/hardware/wifi/offload/1.0/IOffload.h>

#include "wificond/tests/mock_offload.h"
#include "wificond/tests/mock_offload_service_utils.h"
#include "wificond/tests/offload_test_utils.h"

#include "wificond/scanning/scan_result.h"
#include "wificond/scanning/offload/offload_callback.h"
#include "wificond/scanning/offload/offload_scan_manager.h"
#include "wificond/scanning/offload/offload_callback_handlers.h"

using android::hardware::wifi::offload::V1_0::ScanResult;
using android::hardware::wifi::offload::V1_0::OffloadStatus;
using android::hardware::wifi::offload::V1_0::ScanParam;
using android::hardware::wifi::offload::V1_0::ScanFilter;
using com::android::server::wifi::wificond::NativeScanResult;
using testing::NiceMock;
using testing::_;
using testing::Invoke;
using android::sp;
using std::unique_ptr;
using std::vector;
using std::bind;

using namespace std::placeholders;

namespace android {
namespace wificond {

sp<OffloadCallback> CaptureReturnValue(
    OffloadCallbackHandlers* handler,
    sp<OffloadCallback>* offload_callback) {
  *offload_callback = sp<OffloadCallback>(
      new OffloadCallback(handler));
  return *offload_callback;
}

class OffloadScanManagerTest: public ::testing::Test {
  protected:
    virtual void SetUp() {
      ON_CALL(*mock_offload_service_utils_, GetOffloadCallback(_))
          .WillByDefault(Invoke(bind(CaptureReturnValue,
              _1, &offload_callback_)));
    }

    void TearDown() override {
      offload_callback_.clear();
    }

    sp<NiceMock<MockOffload>> mock_offload_{new NiceMock<MockOffload>()};
    sp<OffloadCallback> offload_callback_;
    unique_ptr<NiceMock<MockOffloadServiceUtils>> mock_offload_service_utils_{
        new NiceMock<MockOffloadServiceUtils>()};
    unique_ptr<OffloadScanManager> offload_scan_manager_;
};

/**
 * Testing OffloadScanManager with OffloadServiceUtils null argument
 */
TEST_F(OffloadScanManagerTest, ServiceUtilsNotAvailableTest) {
  offload_scan_manager_.reset(new OffloadScanManager(nullptr, nullptr));
  EXPECT_EQ(OffloadScanManager::kError,
      offload_scan_manager_->getOffloadStatus());
}

/**
 * Testing OffloadScanManager with no handle on Offloal HAL service
 * and no registered handler for Offload Scan results
 */
TEST_F(OffloadScanManagerTest, ServiceNotAvailableTest) {
  ON_CALL(*mock_offload_service_utils_, GetOffloadService())
      .WillByDefault(testing::Return(nullptr));
  offload_scan_manager_.reset(new OffloadScanManager(
      mock_offload_service_utils_.get(),
      [] (vector<NativeScanResult> scanResult) -> void {}));
  EXPECT_EQ(OffloadScanManager::kNoService,
      offload_scan_manager_->getOffloadStatus());
}

/**
 * Testing OffloadScanManager when service is available and valid handler
 * registered for Offload Scan results
 */
TEST_F(OffloadScanManagerTest, ServiceAvailableTest) {
  EXPECT_CALL(*mock_offload_service_utils_, GetOffloadService());
  ON_CALL(*mock_offload_service_utils_, GetOffloadService())
      .WillByDefault(testing::Return(mock_offload_));
  offload_scan_manager_ .reset(new OffloadScanManager(
      mock_offload_service_utils_.get(),
      [] (vector<NativeScanResult> scanResult) -> void {}));
  EXPECT_EQ(OffloadScanManager::kNoError,
      offload_scan_manager_->getOffloadStatus());
}

/**
 * Testing OffloadScanManager when service is available and valid handler
 * is registered, test to ensure that registered handler is invoked when
 * scan results are available
 */
TEST_F(OffloadScanManagerTest, CallbackInvokedTest) {
  bool callback_invoked = false;
  EXPECT_CALL(*mock_offload_service_utils_, GetOffloadService());
  ON_CALL(*mock_offload_service_utils_, GetOffloadService())
      .WillByDefault(testing::Return(mock_offload_));
  offload_scan_manager_.reset(new OffloadScanManager(
      mock_offload_service_utils_.get(),
      [&callback_invoked] (vector<NativeScanResult> scanResult) -> void {
        callback_invoked = true;
      }));
  vector<ScanResult> dummy_scan_results_ =
      OffloadTestUtils::createOffloadScanResults();
  offload_callback_->onScanResult(dummy_scan_results_);
  EXPECT_EQ(true, callback_invoked);
}

/**
 * Testing OffloadScanManager when service is available and valid handler
 * is registered, ensure that error callback is invoked
 */
TEST_F(OffloadScanManagerTest, ErrorCallbackInvokedTest) {
  EXPECT_CALL(*mock_offload_service_utils_, GetOffloadService());
  ON_CALL(*mock_offload_service_utils_, GetOffloadService())
      .WillByDefault(testing::Return(mock_offload_));
  offload_scan_manager_.reset(new OffloadScanManager(
      mock_offload_service_utils_.get(),
      [this] (vector<NativeScanResult> scanResult) -> void {}));
  offload_callback_->onError(OffloadStatus::OFFLOAD_STATUS_ERROR);
  EXPECT_EQ(offload_scan_manager_->getOffloadStatus(),
      OffloadScanManager::kError);
}

/**
 * Testing OffloadScanManager for subscribing to the scan results from
 * Offload HAL when service is running without errors
 */
TEST_F(OffloadScanManagerTest, StartScanTestWhenServiceIsOk) {
  EXPECT_CALL(*mock_offload_service_utils_, GetOffloadService());
  ON_CALL(*mock_offload_service_utils_, GetOffloadService())
      .WillByDefault(testing::Return(mock_offload_));
  offload_scan_manager_ .reset(new OffloadScanManager(
      mock_offload_service_utils_.get(),
      [] (vector<NativeScanResult> scanResult) -> void {}));
  EXPECT_CALL(*mock_offload_, subscribeScanResults(_));
  EXPECT_CALL(*mock_offload_, configureScans(_, _));
  vector<vector<uint8_t>> scan_ssids { kSsid1, kSsid2};
  vector<vector<uint8_t>> match_ssids { kSsid1, kSsid2 };
  vector<uint8_t> security_flags { kNetworkFlags, kNetworkFlags };
  vector<uint32_t> frequencies { kFrequency1, kFrequency2 };
  OffloadScanManager::ReasonCode reason_code = OffloadScanManager::kNone;
  bool result = offload_scan_manager_->startScan(kDisconnectedModeScanIntervalMs,
      kRssiThreshold,
      scan_ssids,
      match_ssids,
      security_flags,
      frequencies,
      &reason_code);
  EXPECT_EQ(result, true);
}

/**
 * Testing OffloadScanManager for subscribing to the scan results from
 * Offload HAL when service is not available
 */
TEST_F(OffloadScanManagerTest, StartScanTestWhenServiceIsNotAvailable) {
  EXPECT_CALL(*mock_offload_service_utils_, GetOffloadService());
  ON_CALL(*mock_offload_service_utils_, GetOffloadService())
      .WillByDefault(testing::Return(nullptr));
  offload_scan_manager_ .reset(new OffloadScanManager(
      mock_offload_service_utils_.get(),
      [] (vector<NativeScanResult> scanResult) -> void {}));
  vector<vector<uint8_t>> scan_ssids { kSsid1, kSsid2};
  vector<vector<uint8_t>> match_ssids { kSsid1, kSsid2 };
  vector<uint8_t> security_flags { kNetworkFlags, kNetworkFlags };
  vector<uint32_t> frequencies { kFrequency1, kFrequency2 };
  OffloadScanManager::ReasonCode reason_code = OffloadScanManager::kNone;
  bool result = offload_scan_manager_->startScan(kDisconnectedModeScanIntervalMs,
      kRssiThreshold,
      scan_ssids,
      match_ssids,
      security_flags,
      frequencies,
      &reason_code);
  EXPECT_EQ(result, false);
  EXPECT_EQ(reason_code, OffloadScanManager::kNotSupported);
}

/**
 * Testing OffloadScanManager for subscribing to the scan results from
 * Offload HAL when service is not working correctly
 */
TEST_F(OffloadScanManagerTest, StartScanTestWhenServiceIsNotConnected) {
  EXPECT_CALL(*mock_offload_service_utils_, GetOffloadService());
  ON_CALL(*mock_offload_service_utils_, GetOffloadService())
      .WillByDefault(testing::Return(mock_offload_));
  offload_scan_manager_ .reset(new OffloadScanManager(
      mock_offload_service_utils_.get(),
      [] (vector<NativeScanResult> scanResult) -> void {}));
  vector<vector<uint8_t>> scan_ssids { kSsid1, kSsid2};
  vector<vector<uint8_t>> match_ssids { kSsid1, kSsid2 };
  vector<uint8_t> security_flags { kNetworkFlags, kNetworkFlags };
  vector<uint32_t> frequencies { kFrequency1, kFrequency2 };
  offload_callback_->onError(OffloadStatus::OFFLOAD_STATUS_NO_CONNECTION);
  OffloadScanManager::ReasonCode reason_code = OffloadScanManager::kNone;
  bool result = offload_scan_manager_->startScan(kDisconnectedModeScanIntervalMs,
      kRssiThreshold,
      scan_ssids,
      match_ssids,
      security_flags,
      frequencies,
      &reason_code);
  EXPECT_EQ(result, false);
  EXPECT_EQ(reason_code, OffloadScanManager::kNotAvailable);
}

/**
 * Testing OffloadScanManager for subscribing to the scan results from
 * Offload HAL twice when service is okay
 */
TEST_F(OffloadScanManagerTest, StartScanTwiceTestWhenServiceIsOk) {
  EXPECT_CALL(*mock_offload_service_utils_, GetOffloadService());
  ON_CALL(*mock_offload_service_utils_, GetOffloadService())
      .WillByDefault(testing::Return(mock_offload_));
  offload_scan_manager_ .reset(new OffloadScanManager(
      mock_offload_service_utils_.get(),
      [] (vector<NativeScanResult> scanResult) -> void {}));
  EXPECT_CALL(*mock_offload_, subscribeScanResults(_)).Times(1);
  EXPECT_CALL(*mock_offload_, configureScans(_, _)).Times(2);
  vector<vector<uint8_t>> scan_ssids { kSsid1, kSsid2};
  vector<vector<uint8_t>> match_ssids { kSsid1, kSsid2 };
  vector<uint8_t> security_flags { kNetworkFlags, kNetworkFlags };
  vector<uint32_t> frequencies { kFrequency1, kFrequency2 };
  OffloadScanManager::ReasonCode reason_code = OffloadScanManager::kNone;
  bool result = offload_scan_manager_->startScan(kDisconnectedModeScanIntervalMs,
      kRssiThreshold,
      scan_ssids,
      match_ssids,
      security_flags,
      frequencies,
      &reason_code);
  EXPECT_EQ(result, true);
  result = offload_scan_manager_->startScan(kDisconnectedModeScanIntervalMs,
      kRssiThreshold,
      scan_ssids,
      match_ssids,
      security_flags,
      frequencies,
      &reason_code);
  EXPECT_EQ(result, true);
}

/**
 * Testing OffloadScanManager for unsubscribing to the scan results from
 * Offload HAL when service is ok
 */
TEST_F(OffloadScanManagerTest, StopScanTestWhenServiceIsOk) {
  EXPECT_CALL(*mock_offload_service_utils_, GetOffloadService());
  ON_CALL(*mock_offload_service_utils_, GetOffloadService())
      .WillByDefault(testing::Return(mock_offload_));
  offload_scan_manager_ .reset(new OffloadScanManager(
      mock_offload_service_utils_.get(),
      [] (vector<NativeScanResult> scanResult) -> void {}));
  EXPECT_CALL(*mock_offload_, subscribeScanResults(_));
  EXPECT_CALL(*mock_offload_, configureScans(_, _));
  EXPECT_CALL(*mock_offload_, unsubscribeScanResults());
  vector<vector<uint8_t>> scan_ssids { kSsid1, kSsid2};
  vector<vector<uint8_t>> match_ssids { kSsid1, kSsid2 };
  vector<uint8_t> security_flags { kNetworkFlags, kNetworkFlags };
  vector<uint32_t> frequencies { kFrequency1, kFrequency2 };
  OffloadScanManager::ReasonCode reason_code = OffloadScanManager::kNone;
  bool result = offload_scan_manager_->startScan(kDisconnectedModeScanIntervalMs,
      kRssiThreshold,
      scan_ssids,
      match_ssids,
      security_flags,
      frequencies,
      &reason_code);
  EXPECT_EQ(result, true);
  result = offload_scan_manager_->stopScan(&reason_code);
  EXPECT_EQ(result, true);
}

/**
 * Testing OffloadScanManager for unsubscribing to the scan results from
 * Offload HAL without first subscribing
 */
TEST_F(OffloadScanManagerTest, StopScanTestWithoutStartWhenServiceIsOk) {
  EXPECT_CALL(*mock_offload_service_utils_, GetOffloadService());
  ON_CALL(*mock_offload_service_utils_, GetOffloadService())
      .WillByDefault(testing::Return(mock_offload_));
  offload_scan_manager_ .reset(new OffloadScanManager(
      mock_offload_service_utils_.get(),
      [] (vector<NativeScanResult> scanResult) -> void {}));
  OffloadScanManager::ReasonCode reason_code = OffloadScanManager::kNone;
  bool result = offload_scan_manager_->stopScan(&reason_code);
  EXPECT_EQ(result, false);
  EXPECT_EQ(reason_code, OffloadScanManager::kNotSubscribed);
}

/**
 * Testing OffloadScanManager for unsubscribing to the scan results from
 * Offload HAL without first subscribing when service is not working correctly
 */
TEST_F(OffloadScanManagerTest, StopScanTestWhenServiceIsNotConnectedAnymore) {
  EXPECT_CALL(*mock_offload_service_utils_, GetOffloadService());
  ON_CALL(*mock_offload_service_utils_, GetOffloadService())
      .WillByDefault(testing::Return(mock_offload_));
  offload_scan_manager_ .reset(new OffloadScanManager(
      mock_offload_service_utils_.get(),
      [] (vector<NativeScanResult> scanResult) -> void {}));
  EXPECT_CALL(*mock_offload_, subscribeScanResults(_));
  EXPECT_CALL(*mock_offload_, configureScans(_, _));
  EXPECT_CALL(*mock_offload_, unsubscribeScanResults());
  vector<vector<uint8_t>> scan_ssids { kSsid1, kSsid2};
  vector<vector<uint8_t>> match_ssids { kSsid1, kSsid2 };
  vector<uint8_t> security_flags { kNetworkFlags, kNetworkFlags };
  vector<uint32_t> frequencies { kFrequency1, kFrequency2 };
  OffloadScanManager::ReasonCode reason_code = OffloadScanManager::kNone;
  bool result = offload_scan_manager_->startScan(kDisconnectedModeScanIntervalMs,
      kRssiThreshold,
      scan_ssids,
      match_ssids,
      security_flags,
      frequencies,
      &reason_code);
  EXPECT_EQ(result, true);
  offload_callback_->onError(OffloadStatus::OFFLOAD_STATUS_NO_CONNECTION);
  result = offload_scan_manager_->stopScan(&reason_code);
  EXPECT_EQ(result, true);
}

}
}

