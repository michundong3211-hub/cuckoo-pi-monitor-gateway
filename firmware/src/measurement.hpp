/*
MIT License

Copyright (c) 2025 Magnus

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
 */
#ifndef SRC_MEASUREMENT_HPP_
#define SRC_MEASUREMENT_HPP_

#include <Arduino.h>
#include <FS.h>

#include <cstdio>
#include <cmath>
#include <cstring>
#include <deque>
#include <gateway_device_metadata.hpp>
#include <gateway_measurement_policy.hpp>
#include <gateway_push_test_policy.hpp>
#include <log.hpp>
#include <map>
#include <memory>
#include <sdcard_mmc.hpp>
#include <sdcard_sd.hpp>
#include <utility>
#include <utils.hpp>

#if defined(ARDUINO_ARCH_ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#endif

#if defined(ENABLE_MMC)
extern SdCardMMC mySdStorage;
#elif defined(ENABLE_SD)
extern SdCardSD mySdStorage;
#endif

enum MeasurementType {
  NoType = 0,
  Tilt = 1,
  TiltPro = 2,
  Gravitymon = 3,
  Pressuremon = 4,
  Chamber = 5,
  Rapt = 6,
};

enum MeasurementSource {
  NoSource = 0,
  BleBeacon = 1,
  BleEddyStone = 2,
  HttpPost = 3,
};

struct GatewayPushTestSample {
  MeasurementType type = MeasurementType::NoType;
  char id[64] = {};
  char name[64] = {};
  char token[64] = {};
  float temperature_c = 0;
  float gravity = 0;
  float angle = 0;
  float pressure = 0;
  float pressure1 = 0;
  float battery = 0;
  int rssi = 0;
  int interval = 0;
  int http_post_sleep_interval = 0;
  int ble_sleep_interval = 0;
  GatewayDeviceMetadata device_metadata;
};

struct GatewayMeasurementSnapshot {
  char id[64] = {};
  MeasurementType type = MeasurementType::NoType;
  uint32_t generation = 0;
  uint32_t update_age_seconds = 0;
  float temperature_c = NAN;
  bool has_temperature = false;
};

// Container for the measurement data
class MeasurementBaseData {
 private:
  MeasurementType _type = MeasurementType::NoType;
  MeasurementSource _source = MeasurementSource::NoSource;
  String _id;
  String _created;

 public:
  MeasurementBaseData(String id, MeasurementType type, MeasurementSource src) {
    _id = id;
    _type = type;
    _source = src;

    struct tm time;
    getLocalTime(&time);

    char buf[40];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
             time.tm_year + 1900, time.tm_mon + 1, time.tm_mday, time.tm_hour,
             time.tm_min, time.tm_sec);
    _created = String(buf);
  }
  virtual ~MeasurementBaseData() {}

  virtual void writeToFile(File& file) const {}

  const char* getCreatedAsString() const { return _created.c_str(); }

  MeasurementType getType() const { return _type; }
  const char* getTypeAsString() const {
    switch (_type) {
      case MeasurementType::Tilt:
        return "Tilt";
      case MeasurementType::TiltPro:
        return "Tilt Pro";
      case MeasurementType::Gravitymon:
        return "Gravitymon";
      case MeasurementType::Pressuremon:
        return "Pressuremon";
      case MeasurementType::Chamber:
        return "Chamber Controller";
      case MeasurementType::Rapt:
        return "RAPT";
      default:
        return "";
    }
  }

  MeasurementSource getSource() const { return _source; }
  const char* getSourceAsString() const {
    switch (_source) {
      case MeasurementSource::BleBeacon:
        return "BLE Beacon";
      case MeasurementSource::BleEddyStone:
        return "BLE Eddystone";
      case MeasurementSource::HttpPost:
        return "HTTP Post";
      default:
        return "";
    }
  }

  const char* getId() const { return _id.c_str(); }
};

enum TiltColor {
  None = -1,
  Red = 0,
  Green = 1,
  Black = 2,
  Purple = 3,
  Orange = 4,
  Blue = 5,
  Yellow = 6,
  Pink = 7
};

class TiltData : public MeasurementBaseData {
 private:
  float _tempF = 0;
  float _gravity = 0;
  int _txPower = 0;
  int _rssi = 0;
  TiltColor _tiltColor;

  const char* colorToString(TiltColor color) const {
    switch (color) {
      case TiltColor::Red:
        return "Red";
      case TiltColor::Green:
        return "Green";
      case TiltColor::Black:
        return "Black";
      case TiltColor::Purple:
        return "Purple";
      case TiltColor::Orange:
        return "Orange";
      case TiltColor::Blue:
        return "Blue";
      case TiltColor::Yellow:
        return "Yellow";
      case TiltColor::Pink:
        return "Pink";
      default:
        return "";
    }
  }

 public:
  TiltData(MeasurementSource source, TiltColor color, float tempF,
           float gravity, int txPower, int rssi, bool pro)
      : MeasurementBaseData(
            colorToString(color),
            pro ? MeasurementType::TiltPro : MeasurementType::Tilt, source) {
    _tiltColor = color;
    _tempF = tempF;
    _gravity = gravity;
    _txPower = txPower;
    _rssi = rssi;
  }
  virtual ~TiltData() {}

  float getTempF() const { return _tempF; }
  float getTempC() const { return convertFtoC(_tempF); }

  float getGravity() const { return _gravity; }
  int getTxPower() const { return _txPower; }
  int getRssi() const { return _rssi; }
  TiltColor getTiltColor() const { return _tiltColor; }

  void writeToFile(File& file) const {
    char buffer[300];

    // Data parameters
    // ----------------------------------------
    // 0, Format Version (1)
    // 1, Type
    // 2, Source
    // 3, Created (timestamp)
    // 4, ID
    // 5, Color
    // 6, Temperature (C)
    // 7, Gravity (SG)
    // 8, Tx Power
    // 9, Rssi
    // 10,
    // 11,
    // 12,
    // 13,

    snprintf(buffer, sizeof(buffer),
             "1,%s,%s,%s,%s,%s,"
             "%.2f,%.4f,%d,%d,,,,",
             getTypeAsString(), getSourceAsString(), getCreatedAsString(),
             getId(), colorToString(_tiltColor), getTempC(), getGravity(),
             getTxPower(), getRssi());
    file.println(buffer);
  }
};

class GravityData : public MeasurementBaseData {
 private:
  String _name = "";
  String _token = "";
  float _tempC = 0;
  float _gravity = 0;
  float _angle = 0;
  float _battery = 0;
  int _rssi = 0;
  int _txPower = 0;
  int _interval = 0;
  int _httpPostSleepInterval = 0;
  int _bleSleepInterval = 0;
  GatewayDeviceMetadata _deviceMetadata;

 public:
  GravityData(MeasurementSource source, String id, String name, String token,
               float tempC, float gravity, float angle, float battery,
               int txPower, int rssi, int interval,
               int httpPostSleepInterval = 0, int bleSleepInterval = 0,
               const GatewayDeviceMetadata& deviceMetadata =
                   GatewayDeviceMetadata())
      : MeasurementBaseData(id, MeasurementType::Gravitymon, source),
        _deviceMetadata(deviceMetadata) {
    _name = name;
    _token = token;
    _tempC = tempC;
    _gravity = gravity;
    _angle = angle;
    _battery = battery;
    _txPower = txPower;
    _rssi = rssi;
    _interval = interval;
    _httpPostSleepInterval = httpPostSleepInterval;
    _bleSleepInterval = bleSleepInterval;
  }
  virtual ~GravityData() {}

  const char* getName() const { return _name.c_str(); }
  const char* getToken() const { return _token.c_str(); }
  float getTempC() const { return _tempC; }
  float getGravity() const { return _gravity; }
  float getAngle() const { return _angle; }
  float getBattery() const { return _battery; }
  int getTxPower() const { return _txPower; }
  int getRssi() const { return _rssi; }
  int getInterval() const { return _interval; }
  int getHttpPostSleepInterval() const { return _httpPostSleepInterval; }
  int getBleSleepInterval() const { return _bleSleepInterval; }
  const GatewayDeviceMetadata& getDeviceMetadata() const {
    return _deviceMetadata;
  }

  void writeToFile(File& file) const {
    char buffer[300];

    // Data parameters
    // ----------------------------------------
    // 0, Format Version (1)
    // 1, Type
    // 2, Source
    // 3, Created (timestamp)
    // 4, ID
    // 5, Name
    // 6, Token
    // 7, Temperature (C)
    // 8, Gravity (SG)
    // 9, Angle
    // 10, Battery
    // 11, Tx Power
    // 12, Rssi
    // 13, Interval

    snprintf(buffer, sizeof(buffer),
             "1,%s,%s,%s,%s,%s,%s,"
             "%.2f,%.4f,%.4f,%.2f,%d,%d,%d",
             getTypeAsString(), getSourceAsString(), getCreatedAsString(),
             getId(), getName(), getToken(), getTempC(), getGravity(),
             getAngle(), getBattery(), getTxPower(), getRssi(), getInterval());
    file.println(buffer);
  }
};

class PressureData : public MeasurementBaseData {
 private:
  String _name = "";
  String _token = "";
  float _tempC = 0;
  float _pressure = 0;
  float _pressure1 = 0;
  float _battery = 0;
  int _rssi = 0;
  int _txPower = 0;
  int _interval = 0;
  int _httpPostSleepInterval = 0;
  int _bleSleepInterval = 0;

 public:
  PressureData(MeasurementSource source, String id, String name, String token,
               float tempC, float pressure, float pressure1, float battery,
               int txPower, int rssi, int interval,
               int httpPostSleepInterval = 0, int bleSleepInterval = 0)
      : MeasurementBaseData(id, MeasurementType::Pressuremon, source) {
    _name = name;
    _token = token;
    _tempC = tempC;
    _pressure = pressure;
    _pressure1 = pressure1;
    _battery = battery;
    _txPower = txPower;
    _rssi = rssi;
    _interval = interval;
    _httpPostSleepInterval = httpPostSleepInterval;
    _bleSleepInterval = bleSleepInterval;
  }
  virtual ~PressureData() {}

  const char* getName() const { return _name.c_str(); }
  const char* getToken() const { return _token.c_str(); }
  float getTempC() const { return _tempC; }
  float getPressure() const { return _pressure; }
  float getPressure1() const { return _pressure1; }
  float getBattery() const { return _battery; }
  int getTxPower() const { return _txPower; }
  int getRssi() const { return _rssi; }
  int getInterval() const { return _interval; }
  int getHttpPostSleepInterval() const { return _httpPostSleepInterval; }
  int getBleSleepInterval() const { return _bleSleepInterval; }

  void writeToFile(File& file) const {
    char buffer[300];

    // Data parameters
    // ----------------------------------------
    // 0, Format Version (1)
    // 1, Type
    // 2, Source
    // 3, Created (timestamp)
    // 4, ID
    // 5, Name
    // 6, Token
    // 7, Temperature (C)
    // 8, Pressure (PSI)
    // 9, Pressure1 (PSI)
    // 10, Battery
    // 11, Tx Power
    // 12, Rssi
    // 13, Interval

    snprintf(buffer, sizeof(buffer),
             "1,%s,%s,%s,%s,%s,%s,"
             "%.2f,%.4f,%.4f,%.2f,%d,%d,%d",
             getTypeAsString(), getSourceAsString(), getCreatedAsString(),
             getId(), getName(), getToken(), getTempC(), getPressure(),
             getPressure1(), getBattery(), getTxPower(), getRssi(),
             getInterval());
    file.println(buffer);
  }
};

class ChamberData : public MeasurementBaseData {
 private:
  float _chamberTempC = 0;
  float _beerTempC = 0;
  int _txPower = 0;
  int _rssi = 0;
  String _name = "";

 public:
  ChamberData(MeasurementSource source, String id, String name,
              float chamberTempC, float beerTempC, int txPower, int rssi)
      : MeasurementBaseData(id, MeasurementType::Chamber, source) {
    _chamberTempC = chamberTempC;
    _beerTempC = beerTempC;
    _rssi = rssi;
    _txPower = txPower;
    _name = name;
  }
  virtual ~ChamberData() {}

  float getChamberTempC() const { return _chamberTempC; }
  float getBeerTempC() const { return _beerTempC; }
  int getTxPower() const { return _txPower; }
  int getRssi() const { return _rssi; }
  const char* getName() const { return _name.c_str(); }

  void writeToFile(File& file) const {
    char buffer[300];

    // Data parameters
    // ----------------------------------------
    // 0, Format Version (1)
    // 1, Type
    // 2, Source
    // 3, Created (timestamp)
    // 4, ID
    // 5, ChamberTemp (C)
    // 6, BeerTemp (C)
    // 7, Rssi
    // 8, Name (new in 0.9.0)
    // 9, TxPower (new 0.9.0)
    // 10,
    // 11,
    // 12,
    // 13,

    snprintf(buffer, sizeof(buffer),
             "1,%s,%s,%s,%s,"
             "%.2f,%.2f,%d,%s,%d,,,,",
             getTypeAsString(), getSourceAsString(), getCreatedAsString(),
             getId(), getChamberTempC(), getBeerTempC(), getRssi(), getName(),
             getTxPower());
    file.println(buffer);
  }
};

class RaptData : public MeasurementBaseData {
 private:
  float _tempC = 0;
  float _gravity = 0;
  float _angle = 0;
  float _velocity = 0;
  float _batteryPercent = 0;
  int _rssi = 0;
  int _txPower = 0;

 public:
  // Note! For RAPT the last part of the MAC adress is used as ID since the
  // payload does not contain that.
  RaptData(MeasurementSource source, String id, float tempC, float gravity,
           float velocity, float angle, float batteryPercent, int txPower,
           int rssi)
      : MeasurementBaseData(id, MeasurementType::Rapt, source) {
    _tempC = tempC;
    _velocity = velocity;
    _gravity = gravity;
    _angle = angle;
    _batteryPercent = batteryPercent;
    _txPower = txPower;
    _rssi = rssi;
  }
  virtual ~RaptData() {}

  float getTempC() const { return _tempC; }
  float getGravity() const { return _gravity; }
  float getVelocity() const { return _velocity; }
  float getAngle() const { return _angle; }
  float getBatteryPercent() const { return _batteryPercent; }
  int getTxPower() const { return _txPower; }
  int getRssi() const { return _rssi; }

  void writeToFile(File& file) const {
    char buffer[300];

    // Data parameters
    // ----------------------------------------
    // 0, Format Version (1)
    // 1, Type
    // 2, Source
    // 3, Created (timestamp)
    // 4, ID
    // 5, Temperature (C)
    // 6, Gravity (SG)
    // 7, Angle
    // 8, BatteryPercent
    // 9, Tx Power
    // 10, Rssi

    snprintf(buffer, sizeof(buffer),
             "1,%s,%s,%s,%s,"
             "%.2f,%.4f,%.4f,%.2f,%d,%d,,,",
             getTypeAsString(), getSourceAsString(), getCreatedAsString(),
             getId(), getTempC(), getGravity(), getAngle(), getBatteryPercent(),
             getTxPower(), getRssi());
    file.println(buffer);
  }
};

// Base class for measurement data keeping track of last updated and pushed
class MeasurementEntry {
 private:
  std::unique_ptr<MeasurementBaseData> _measurement;
  bool _updated = false;
  struct tm _timeinfoUpdated;
  uint32_t _timeUpdated = 0;
  uint32_t _timePushed = 0;
  uint32_t _updateGeneration = 0;
  String _id = "";

 public:
  explicit MeasurementEntry(String id) { _id = id; }
  ~MeasurementEntry() {}

  const MeasurementBaseData* getData() const { return _measurement.get(); }
  const TiltData* getTiltData() {
    return static_cast<TiltData*>(_measurement.get());
  }
  const GravityData* getGravityData() const {
    return static_cast<GravityData*>(_measurement.get());
  }
  const RaptData* getRaptData() const {
    return static_cast<RaptData*>(_measurement.get());
  }
  const PressureData* getPressureData() const {
    return static_cast<PressureData*>(_measurement.get());
  }
  const ChamberData* getChamberData() const {
    return static_cast<ChamberData*>(_measurement.get());
  }

  void setMeasurement(std::unique_ptr<MeasurementBaseData> measurement) {
    _measurement = std::move(measurement);
    setUpdated();
  }

  MeasurementType getType() const {
    if (_measurement != nullptr) return _measurement->getType();
    return MeasurementType::NoType;
  }

  bool isUpdated() const { return _updated; }
  const String& getId() const { return _id; }

  void setUpdated() {
    _updated = true;
    _updateGeneration =
        gatewayNextMeasurementGeneration(_updateGeneration);
    _timeUpdated = millis();
    getLocalTime(&_timeinfoUpdated);
  }

  void setPushed() {
    _updated = false;
    _timePushed = millis();
  }

  uint32_t getUpdateAge() const { return (millis() - _timeUpdated) / 1000; }
  uint32_t getUpdateAgeMilliseconds() const { return millis() - _timeUpdated; }
  uint32_t getPushAge() const { return (millis() - _timePushed) / 1000; }
  uint32_t getUpdateGeneration() const { return _updateGeneration; }
  const struct tm* getTimeinfoUpdated() const { return &_timeinfoUpdated; }
};

// List of data measurements
class MeasurementList {
 private:
  std::deque<std::unique_ptr<MeasurementEntry>> _list;
  std::map<String, uint32_t> _lastLogTimes;

#if defined(ARDUINO_ARCH_ESP32)
  mutable SemaphoreHandle_t _mutex = nullptr;
#endif

  void lock() const {
#if defined(ARDUINO_ARCH_ESP32)
    if (_mutex != nullptr) xSemaphoreTakeRecursive(_mutex, portMAX_DELAY);
#endif
  }

  void unlock() const {
#if defined(ARDUINO_ARCH_ESP32)
    if (_mutex != nullptr) xSemaphoreGiveRecursive(_mutex);
#endif
  }

  class ScopedLock {
   public:
    explicit ScopedLock(const MeasurementList& owner) : _owner(owner) {
      _owner.lock();
    }
    ~ScopedLock() { _owner.unlock(); }

   private:
    const MeasurementList& _owner;
  };

 public:
  MeasurementList() {
#if defined(ARDUINO_ARCH_ESP32)
    _mutex = xSemaphoreCreateRecursiveMutex();
#endif
  }
  MeasurementList(const MeasurementList&) = delete;
  MeasurementList& operator=(const MeasurementList&) = delete;
  ~MeasurementList() {
    clear();
#if defined(ARDUINO_ARCH_ESP32)
    if (_mutex != nullptr) {
      vSemaphoreDelete(_mutex);
      _mutex = nullptr;
    }
#endif
  }

  void updateData(std::unique_ptr<MeasurementBaseData>& data,
                  int minWaitTimeMinutes = 0) {
    if (data.get() == nullptr) {
      return;
    }

    ScopedLock guard(*this);

    String id = data->getId();
    int32_t MIN_WAIT_TIME =
        minWaitTimeMinutes * 60 * 1000;  // Convert minutes to milliseconds
    uint32_t now = millis();
    bool shouldWrite = (_lastLogTimes.find(id) == _lastLogTimes.end()) ||
                       (now - _lastLogTimes[id]) >= MIN_WAIT_TIME;

#if defined(ENABLE_MMC) || defined(ENABLE_SD)
    if (shouldWrite) {
      _lastLogTimes[id] = now;

      Log.notice(F("Meas: Logging data from %s to SD." CR), id.c_str());

      if (mySdStorage.hasCard()) {
        File file = mySdStorage.open("/data.csv", FILE_APPEND, true);
        if (file) {
          data->writeToFile(file);
          file.close();
        } else {
          Log.error(F("SD  : Failed to open data.csv for writing." CR));
        }
      }
    } else {
      Log.notice(F("Meas: Skip logging data from %s, to frequent "
                   "logging." CR),
                 id.c_str());
    }
#endif

    int i = findMeasurementById(id);

    if (!gatewayMeasurementCanStore(_list.size(), i != -1)) return;

    if (i == -1) {
      std::unique_ptr<MeasurementEntry> entry;

      entry.reset(new MeasurementEntry(data->getId()));
      entry->setMeasurement(std::move(data));
      _list.push_back(std::move(entry));
    } else {
      MeasurementEntry* entry = getMeasurementEntry(i);

      if (entry != nullptr) entry->setMeasurement(std::move(data));
    }
  }

  MeasurementType getMeasurementType(int index) {
    ScopedLock guard(*this);
    if (index < 0 || index >= size()) return MeasurementType::NoType;
    return getMeasurementEntry(index)->getType();
  }

  int findMeasurementById(const String id) const {
    ScopedLock guard(*this);
    int i = 0;
    for (const std::unique_ptr<MeasurementEntry>& item : _list) {
      if (id == item->getId()) {
        return i;
      }
      i++;
    }

    return -1;
  }

  MeasurementEntry* getMeasurementEntry(int index) {
    ScopedLock guard(*this);
    if (index < 0 || index >= static_cast<int>(_list.size())) return nullptr;
    return _list[index].get();
  }

  size_t copySnapshot(GatewayMeasurementSnapshot* output,
                      size_t outputCapacity) const {
    if (output == nullptr || outputCapacity == 0) return 0;
    ScopedLock guard(*this);
    const size_t limit =
        outputCapacity < GATEWAY_MEASUREMENT_MAX_ENTRIES
            ? outputCapacity
            : GATEWAY_MEASUREMENT_MAX_ENTRIES;
    size_t copied = 0;
    for (const std::unique_ptr<MeasurementEntry>& entry : _list) {
      if (copied >= limit || entry == nullptr || entry->getData() == nullptr)
        break;
      const String& id = entry->getId();
      if (id.length() == 0 || id.length() >= sizeof(output[copied].id))
        continue;

      GatewayMeasurementSnapshot snapshot{};
      memcpy(snapshot.id, id.c_str(), id.length() + 1);
      snapshot.type = entry->getType();
      snapshot.generation = entry->getUpdateGeneration();
      snapshot.update_age_seconds = entry->getUpdateAge();

      switch (snapshot.type) {
        case MeasurementType::Gravitymon: {
          const GravityData* data = entry->getGravityData();
          if (data != nullptr) snapshot.temperature_c = data->getTempC();
        } break;
        case MeasurementType::Pressuremon: {
          const PressureData* data = entry->getPressureData();
          if (data != nullptr) snapshot.temperature_c = data->getTempC();
        } break;
        case MeasurementType::Chamber: {
          const ChamberData* data = entry->getChamberData();
          if (data != nullptr)
            snapshot.temperature_c = data->getChamberTempC();
        } break;
        case MeasurementType::Tilt:
        case MeasurementType::TiltPro: {
          const TiltData* data = entry->getTiltData();
          if (data != nullptr) snapshot.temperature_c = data->getTempC();
        } break;
        case MeasurementType::Rapt: {
          const RaptData* data = entry->getRaptData();
          if (data != nullptr) snapshot.temperature_c = data->getTempC();
        } break;
        default:
          break;
      }
      snapshot.has_temperature = isfinite(snapshot.temperature_c);
      output[copied++] = snapshot;
    }
    return copied;
  }

  bool copyLatestPushTestSample(MeasurementType type,
                                GatewayPushTestSample* output) const {
    if (output == nullptr ||
        (type != MeasurementType::Gravitymon &&
         type != MeasurementType::Pressuremon)) {
      return false;
    }

    ScopedLock guard(*this);
    const MeasurementEntry* latest = nullptr;
    uint32_t latestAgeMilliseconds = 0;

    for (const std::unique_ptr<MeasurementEntry>& entry : _list) {
      if (entry == nullptr || entry->getData() == nullptr ||
          entry->getType() != type) {
        continue;
      }

      const uint32_t candidateAgeMilliseconds =
          entry->getUpdateAgeMilliseconds();
      if (gatewayPushCandidateIsNewer(latest != nullptr,
                                      latestAgeMilliseconds,
                                      candidateAgeMilliseconds)) {
        latest = entry.get();
        latestAgeMilliseconds = candidateAgeMilliseconds;
      }
    }

    if (latest == nullptr) return false;

    GatewayPushTestSample sample{};
    sample.type = type;
    const MeasurementBaseData* base = latest->getData();
    snprintf(sample.id, sizeof(sample.id), "%s", base->getId());

    if (type == MeasurementType::Gravitymon) {
      const GravityData* data = latest->getGravityData();
      if (data == nullptr) return false;
      snprintf(sample.name, sizeof(sample.name), "%s", data->getName());
      snprintf(sample.token, sizeof(sample.token), "%s", data->getToken());
      sample.temperature_c = data->getTempC();
      sample.gravity = data->getGravity();
      sample.angle = data->getAngle();
      sample.battery = data->getBattery();
      sample.rssi = data->getRssi();
      sample.interval = data->getInterval();
      sample.http_post_sleep_interval = data->getHttpPostSleepInterval();
      sample.ble_sleep_interval = data->getBleSleepInterval();
      sample.device_metadata = data->getDeviceMetadata();
    } else {
      const PressureData* data = latest->getPressureData();
      if (data == nullptr) return false;
      snprintf(sample.name, sizeof(sample.name), "%s", data->getName());
      snprintf(sample.token, sizeof(sample.token), "%s", data->getToken());
      sample.temperature_c = data->getTempC();
      sample.pressure = data->getPressure();
      sample.pressure1 = data->getPressure1();
      sample.battery = data->getBattery();
      sample.rssi = data->getRssi();
      sample.interval = data->getInterval();
      sample.http_post_sleep_interval = data->getHttpPostSleepInterval();
      sample.ble_sleep_interval = data->getBleSleepInterval();
    }

    *output = sample;
    return true;
  }

  void clear() {
    ScopedLock guard(*this);
    _list.clear();
  }
  int size() const {
    ScopedLock guard(*this);
    return static_cast<int>(_list.size());
  }
};

extern MeasurementList myMeasurementList;

#endif  // SRC_MEASUREMENT_HPP_
