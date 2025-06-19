#include "order_management.hpp"

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
