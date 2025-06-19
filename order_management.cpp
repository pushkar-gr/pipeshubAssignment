#include "order_management.hpp"

#include <iomanip>
#include <iostream>

using namespace std;

OrderManagement::OrderManagement(const string &startTime, const string &endTime,
                                 const string &timeZone, int maxOrdersPerSecond)
    : m_timeZone(timeZone), m_maxOrdersPerSecond(maxOrdersPerSecond),
      m_lastSecondStart(chrono::steady_clock::now()), m_ordersThisSecond(0) {
  // convert HH:MM:SS to seconds
  int startH, startM, startS, endH, endM, endS;

  sscanf(startTime.c_str(), "%d:%d:%d", &startH, &startM, &startS);
  sscanf(endTime.c_str(), "%d:%d:%d", &endH, &endM, &endS);

  m_startSeconds = startH * 3600 + startM * 60 + startS;
  m_endSeconds = endH * 3600 + endM * 60 + endS;

  // open log file for responses
  m_logFile.open("order_responses.log", ios::app);
  if (!m_logFile.is_open()) {
    throw runtime_error("Failed to open log file");
  }

  // header
  m_logFile << "Timestamp,OrderId,ResponseType,LatencyMicros\n";

  start();
}

OrderManagement::~OrderManagement() {
  stop();
  if (m_logFile.is_open()) {
    m_logFile.close();
  }
}

void OrderManagement::start() {
  if (m_running.load()) {
    return;
  }

  m_running.store(true);

  // start process threead
  m_processingThread = thread(&OrderManagement::processOrderQueue, this);

  // start timer thread
  m_timeManagerThread = thread(&OrderManagement::manageTime, this);
}

void OrderManagement::stop() {
  if (!m_running.load()) {
    return;
  }

  m_running.store(false);

  // notify processing threads to wake up
  m_queueCondition.notify_all();

  // join threads
  if (m_processingThread.joinable()) {
    m_processingThread.join();
  }

  if (m_timeManagerThread.joinable()) {
    m_timeManagerThread.join();
  }

  // logout
  if (m_loggedIn.load()) {
    sendLogout();
    m_loggedIn.store(false);
  }
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

void OrderManagement::onData(OrderResponse &&response) {
  unique_lock<mutex> lock(m_pendingMutex);

  auto it = m_pendingOrders.find(response.m_orderId);
  if (it != m_pendingOrders.end()) {
    auto latency = calculateLatency(it->second.sentTime);

    logResponse(response.m_orderId, response.m_responseType, latency);

    m_pendingOrders.erase(it);
  } else {
    cout << "unknown response" << endl;
  }
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
      m_loggedIn.store(true);
      m_tradingSessionActive.store(true);
      cout << "Logged in" << endl;
    } else if (!shouldBeActive && isActive) {
      // end trading
      sendLogout();
      m_loggedIn.store(false);
      m_tradingSessionActive.store(false);
      cout << "Logged out" << endl;
    }

    // check every 30 seconds
    this_thread::sleep_for(chrono::seconds(30));
  }
}

void OrderManagement::processOrderQueue() {
  while (m_running.load()) {
    unique_lock<mutex> lock(m_queueMutex);

    // wait for orders
    m_queueCondition.wait(
        lock, [this]() { return !m_orderQueue.empty() || !m_running.load(); });

    if (!m_running.load()) {
      break;
    }

    while (!m_orderQueue.empty() && m_tradingSessionActive.load() &&
           canSendOrder()) {
      // process order
      QueuedOrder queuedOrder = m_orderQueue.front();
      m_orderQueue.pop();

      lock.unlock();

      send(queuedOrder.request);

      {
        unique_lock<mutex> pendingLock(m_pendingMutex);
        m_pendingOrders.emplace(queuedOrder.request.m_orderId,
                                PendingOrder(queuedOrder.request));
      }

      updateThrottling();

      lock.lock();
    }

    // wait till next second if we cannot take more oreders due to throttling
    if (!m_orderQueue.empty() && !canSendOrder()) {
      lock.unlock();
      this_thread::sleep_for(chrono::milliseconds(100));
      lock.lock();
    }
  }
}

bool OrderManagement::canSendOrder() {
  auto now = chrono::steady_clock::now();
  auto elapsed =
      chrono::duration_cast<std::chrono::seconds>(now - m_lastSecondStart);

  if (elapsed.count() >= 1) {
    // new second started
    m_lastSecondStart = now;
    m_ordersThisSecond = 0;
  }

  return m_ordersThisSecond < m_maxOrdersPerSecond;
}

void OrderManagement::updateThrottling() { m_ordersThisSecond++; }

void OrderManagement::logResponse(uint64_t orderId, ResponseType responseType,
                                  chrono::microseconds latency) {
  unique_lock<mutex> lock(m_logMutex);

  auto now = chrono::system_clock::now();
  auto time_t = chrono::system_clock::to_time_t(now);

  string responseTypeStr;
  switch (responseType) {
  case ResponseType::Accept:
    responseTypeStr = "Accept";
    break;
  case ResponseType::Reject:
    responseTypeStr = "Reject";
    break;
  default:
    responseTypeStr = "Unknown";
    break;
  }

  m_logFile << put_time(localtime(&time_t), "%Y-%m-%d %H:%M:%S") << ","
            << orderId << "," << responseTypeStr << "," << latency.count()
            << endl;

  m_logFile.flush();
}

chrono::microseconds OrderManagement::calculateLatency(
    const chrono::high_resolution_clock::time_point &sentTime) {
  auto now = chrono::high_resolution_clock::now();
  return chrono::duration_cast<chrono::microseconds>(now - sentTime);
}
