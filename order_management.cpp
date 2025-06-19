#include "order_management.hpp"

#include <chrono>
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
