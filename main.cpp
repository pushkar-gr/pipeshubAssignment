#include "order_management.hpp"
#include <chrono>
#include <iostream>
#include <thread>

int main() {
  try {
    // create order management system
    OrderManagement orderMgmt("9:00:00", "13:00:00", "IST", 100);

    // start the system
    orderMgmt.start();

    std::cout << "Order Management System started..." << std::endl;

    // simulate some orders
    std::thread orderGenerator([&orderMgmt]() {
      for (int i = 1; i <= 10; ++i) {
        OrderRequest req;
        req.m_symbolId = 1001;
        req.m_price = 100.0 + i;
        req.m_qty = 100;
        req.m_side = (i % 2 == 0) ? 'B' : 'S';
        req.m_orderId = i;

        orderMgmt.onData(std::move(req), RequestType::New);

        std::cout << "OrderID: " << req.m_orderId << std::endl;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    });

    // simulate responses from exchange
    std::thread responseGenerator([&orderMgmt]() {
      std::this_thread::sleep_for(std::chrono::seconds(2));

      for (int i = 1; i <= 10; ++i) {
        OrderResponse resp;
        resp.m_orderId = i;
        resp.m_responseType =
            (i % 3 == 0) ? ResponseType::Reject : ResponseType::Accept;

        orderMgmt.onData(std::move(resp));

        std::cout << "OrderID: " << resp.m_orderId << " "
                  << (resp.m_responseType == ResponseType::Reject ? "Reject"
                                                                  : "Accept")
                  << std::endl;

        std::this_thread::sleep_for(std::chrono::milliseconds(150));
      }
    });

    // run for 30 seconds
    std::this_thread::sleep_for(std::chrono::seconds(30));

    // wait for threads to complete
    if (orderGenerator.joinable()) {
      orderGenerator.join();
    }
    if (responseGenerator.joinable()) {
      responseGenerator.join();
    }

    // stop the system
    orderMgmt.stop();

    std::cout << "Order Management System stopped." << std::endl;

  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
