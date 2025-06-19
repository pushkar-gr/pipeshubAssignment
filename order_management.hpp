#pragma once

#include <atomic>
#include <stdint.h>
#include <string>
#include <thread>

struct Logon {
  std::string username;
  std::string password;
};

struct Logout {
  std::string username;
};

struct OrderRequest {
  int m_symbolId;
  double m_price;
  uint64_t m_qty;
  char m_side; // possible values 'B' or 'S'
  uint64_t m_orderId;
};

enum class RequestType {
  Unknown = 0,
  New = 1,
  Modify = 2,
  Cancel = 3,
};

enum class ResponseType {
  Unknown = 0,
  Accept = 1,
  Reject = 2,
};

struct OrderResponse {
  uint64_t m_orderId;
  ResponseType m_responseType;
};

class OrderManagement {
private:
  // config
  int m_startSeconds; // start time for orders
  int m_endSeconds;   // end time of for orders
  std::string m_timeZone;
  int m_maxOrdersPerSecond;

  // threads and synchronization
  std::thread m_timeManagerThread;
  std::atomic<bool> m_running{false};
  std::atomic<bool> m_tradingSessionActive{false};

  // helper methods
  bool isWithinTradingHours() const;
  void manageTime();

public:
  OrderManagement(const std::string &startTime, const std::string &endTime,
                  const std::string &timeZone, int maxOrdersPerSecond);

  void onData(OrderResponse &&response) {}
  void send(const OrderRequest &request) {}
  void sendLogon() {}
  void sendLogout() {}
};
