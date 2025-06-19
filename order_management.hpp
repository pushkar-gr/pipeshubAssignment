#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <stdint.h>
#include <string>
#include <thread>
#include <unordered_map>

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

struct QueuedOrder {
  OrderRequest request;
  RequestType type;
  std::chrono::high_resolution_clock::time_point enqueuedTime;

  QueuedOrder(const OrderRequest &request, RequestType &type)
      : request(request), type(type),
        enqueuedTime(std::chrono::high_resolution_clock::now()) {}
};

struct PendingOrder {
  OrderRequest request;
  std::chrono::high_resolution_clock::time_point sentTime;

  PendingOrder(const OrderRequest &req)
      : request(req), sentTime(std::chrono::high_resolution_clock::now()) {}
};

class OrderManagement {
private:
  // config
  int m_startSeconds; // start time for orders
  int m_endSeconds;   // end time of for orders
  std::string m_timeZone;
  int m_maxOrdersPerSecond;

  // threads and synchronization
  std::thread m_processingThread;
  std::thread m_timeManagerThread;
  std::mutex m_queueMutex;
  std::mutex m_pendingMutex;
  std::condition_variable m_queueCondition;
  std::atomic<bool> m_running{false};
  std::atomic<bool> m_tradingSessionActive{false};

  // order management
  std::queue<QueuedOrder> m_orderQueue;
  std::unordered_map<uint64_t, PendingOrder> m_pendingOrders;

  // throttling
  std::chrono::steady_clock::time_point m_lastSecondStart;
  int m_ordersThisSecond;

  // helper methods
  bool isWithinTradingHours() const;
  void manageTime();
  void processOrderQueue();
  bool canSendOrder();
  void updateThrottling();

public:
  OrderManagement(const std::string &startTime, const std::string &endTime,
                  const std::string &timeZone, int maxOrdersPerSecond);

  void onData(OrderRequest &&request, RequestType type);
  void onData(OrderResponse &&response) {}
  void send(const OrderRequest &request) {}
  void sendLogon() {}
  void sendLogout() {}
};
