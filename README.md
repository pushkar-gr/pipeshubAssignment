# Order Management System (OMS)

**Author:** pushkar-gr  
**Language:** C++17

## Summary

High-performance, multi-threaded Order Management System designed for financial trading operations. Implements producer-consumer architecture with microsecond latency optimization, automatic session management, and robust error handling suitable for institutional trading environments.

## Architecture

**Multi-threaded Design:**
- **Time Manager Thread:** Handles trading session control, login/logout automation, and timezone-aware trading hours validation
- **Order Processing Thread:** Queue-based order processing with throttling and exchange communication
- **Main Thread:** Order submission, response handling, and system control

**Core Components:**
- Thread-safe order queue with condition variable signaling
- Atomic session state management for lockless validation
- Hash-based response correlation with O(1) lookup
- RAII resource management with exception safety guarantees

## Key Features

- **Session Management:** Automatic login/logout based on configurable trading hours with timezone support
- **Order Lifecycle:** Complete tracking from submission to exchange response with latency measurement
- **Throttling:** Configurable orders-per-second rate limiting to prevent exchange overload
- **Multi-threading:** Lock-free hot path with fine-grained locking for maximum performance
- **Order Types:** Support for New, Modify, and Cancel operations with queue-based processing
- **Logging:** Comprehensive response logging with timestamp and latency metrics
- **Error Handling:** Graceful degradation and clean shutdown with resource cleanup

## Complexity Analysis

**Time Complexity:**
- Order validation: O(1) - atomic boolean check
- Order queueing: O(1) - queue insertion
- Response correlation: O(1) - hash map lookup
- Throttling check: O(1) - timestamp comparison

**Space Complexity:**
- O(n) where n is number of pending orders
- Memory-efficient with automatic cleanup
- No memory leaks with RAII design

**Performance Characteristics:**
- Order processing: ~1-2 nanoseconds per validation
- Thread synchronization: Event-driven (zero CPU when idle)
- Scalability: Handles burst traffic with queue buffering
- Latency: Optimized for microsecond-scale requirements

## Technical Highlights

- **Lock-free validation** using atomic operations in critical path
- **Producer-consumer pattern** with condition variables for efficient threading
- **Exception-safe design** with comprehensive RAII implementation
- **Timezone-aware** trading session management
- **Fine-grained locking** strategy to minimize thread contention
- **Clean shutdown protocol** ensuring graceful system termination
