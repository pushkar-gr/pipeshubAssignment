#include "order_management.hpp"

#include <iostream>

using namespace std;

OrderManagement::OrderManagement(const string &startTime, const string &endTime,
                                 const string &timeZone, int maxOrdersPerSecond)
    : m_timeZone(timeZone), m_maxOrdersPerSecond(maxOrdersPerSecond) {
  // convert HH:MM:SS to seconds
  int startH, startM, startS, endH, endM, endS;

  sscanf(startTime.c_str(), "%d:%d:%d", &startH, &startM, &startS);
  sscanf(endTime.c_str(), "%d:%d:%d", &endH, &endM, &endS);

  m_startSeconds = startH * 3600 + startM * 60 + startS;
  m_endSeconds = endH * 3600 + endM * 60 + endS;
}

void OrderManagement::onData(OrderRequest &&request, RequestType type) {
  // check if in trading hours
  if (!m_tradingSessionActive.load()) {
    cout << "Order Rejected: outside trading hours (Order ID: "
         << request.m_orderId << ")" << endl;
    return;
  }

  unique_lock<mutex> lock(m_queueMutex);

  switch (type) {
  case RequestType::New: {
    m_orderQueue.emplace(request, type);
    break;
  }

  case RequestType::Modify: {
    // find existing orders to update
    bool found = false;
    queue<QueuedOrder> tempQueue;

    while (!m_orderQueue.empty()) {
      QueuedOrder currentOrder = m_orderQueue.front();
      m_orderQueue.pop();

      if (currentOrder.request.m_orderId == request.m_orderId) {
        currentOrder.request.m_price = request.m_price;
        currentOrder.request.m_qty = request.m_qty;
        found = true;
      }

      tempQueue.push(currentOrder);
    }

    // restore queue
    m_orderQueue = move(tempQueue);

    if (!found) {
      // new order if not found
      m_orderQueue.emplace(request, RequestType::New);
    }
    break;
  }

  case RequestType::Cancel: {
    // remove orders from queue
    queue<QueuedOrder> tempQueue;

    while (!m_orderQueue.empty()) {
      QueuedOrder currentOrder = m_orderQueue.front();
      m_orderQueue.pop();

      if (currentOrder.request.m_orderId != request.m_orderId) {
        tempQueue.push(currentOrder);
      }
    }

    // restore new queue without cancled order
    m_orderQueue = move(tempQueue);
    break;
  }
  default:
    cout << "Unknown request type (OrderID: " << request.m_orderId << ")"
         << endl;
    return;
  }

  // notify processing thread
  m_queueCondition.notify_one();
}

bool OrderManagement::isWithinTradingHours() const {
  auto time_now = chrono::system_clock::now();
  auto time_t = chrono::system_clock::to_time_t(time_now);
  auto local_time = *localtime(&time_t);
  int currentSeconds =
      local_time.tm_hour * 3600 + local_time.tm_min * 60 + local_time.tm_sec;

  return currentSeconds >= m_startSeconds && currentSeconds <= m_endSeconds;
}

void OrderManagement::manageTime() {
  while (m_running.load()) {
    bool shouldBeActive = isWithinTradingHours();
    bool isActive = m_tradingSessionActive.load();

    if (shouldBeActive && !isActive) {
      // start trading
      sendLogon();
      m_tradingSessionActive.store(true);
      cout << "Logged in" << endl;
    } else if (!shouldBeActive && isActive) {
      // end trading
      sendLogout();
      m_tradingSessionActive.store(false);
      cout << "Logged out" << endl;
    }

    // check every 30 seconds
    this_thread::sleep_for(chrono::seconds(30));
  }
}
