// SPDX-License-Identifier: MIT

#pragma once

#include <any>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

// Forward declarations for existing global classes
class SessionManager;
class AudioEngine;

namespace orpheus {

// Forward declarations for future orpheus namespace classes
class EventLogger;
class PlayoutLogger;
class SettingsService;
class UndoManager;
class DisplayPreferences;
class HotKeyManager;
class MIDIDeviceManager;
class ExternalToolManager;
class ApplicationPaths;
class Database;
class UndoManager;

/**
 * @brief Dependency injection container for managing application services.
 *
 * ServiceContext provides centralized lifecycle management for all application
 * services (managers, engines, etc.) without manual pointer passing.
 *
 * @section usage Usage
 * @code
 * // Registration (at startup)
 * auto& ctx = ServiceContext::getInstance();
 * ctx.registerService<SessionManager>(std::make_shared<SessionManager>());
 * ctx.registerService<AudioEngine>(std::make_shared<AudioEngine>());
 *
 * // Retrieval (anywhere in app)
 * auto sessionMgr = ctx.getService<SessionManager>();
 * auto audioEngine = ctx.getService<AudioEngine>();
 * @endcode
 *
 * @section lifecycle Lifecycle
 * Services are registered in startup order and shut down in reverse order.
 * Call shutdown() before application exit to ensure proper cleanup.
 *
 * @section threading Thread Safety
 * All operations are thread-safe via internal mutex.
 */
class ServiceContext {
public:
  /**
   * @brief Get the singleton instance.
   * @return Reference to the global ServiceContext
   */
  static ServiceContext& getInstance() {
    static ServiceContext instance;
    return instance;
  }

  // Delete copy/move to enforce singleton
  ServiceContext(const ServiceContext&) = delete;
  ServiceContext& operator=(const ServiceContext&) = delete;
  ServiceContext(ServiceContext&&) = delete;
  ServiceContext& operator=(ServiceContext&&) = delete;

  /**
   * @brief Register a service with the container.
   *
   * @tparam T Service type (must be a class type)
   * @param service Shared pointer to the service instance
   * @throws std::runtime_error if service of this type already registered
   *
   * @note Services are stored in registration order for ordered shutdown.
   */
  template <typename T> void registerService(std::shared_ptr<T> service) {
    static_assert(std::is_class_v<T>, "Service must be a class type");

    std::lock_guard<std::mutex> lock(m_mutex);

    auto typeIdx = std::type_index(typeid(T));
    if (m_services.find(typeIdx) != m_services.end()) {
      throw std::runtime_error("Service already registered: " + std::string(typeid(T).name()));
    }

    m_services[typeIdx] = service;
    m_registrationOrder.push_back(typeIdx);
  }

  /**
   * @brief Get a registered service.
   *
   * @tparam T Service type to retrieve
   * @return Shared pointer to the service
   * @throws std::runtime_error if service not found
   */
  template <typename T> std::shared_ptr<T> getService() {
    static_assert(std::is_class_v<T>, "Service must be a class type");

    std::lock_guard<std::mutex> lock(m_mutex);

    auto typeIdx = std::type_index(typeid(T));
    auto it = m_services.find(typeIdx);
    if (it == m_services.end()) {
      throw std::runtime_error("Service not found: " + std::string(typeid(T).name()));
    }

    return std::any_cast<std::shared_ptr<T>>(it->second);
  }

  /**
   * @brief Try to get a service without throwing.
   *
   * @tparam T Service type to retrieve
   * @return Shared pointer to service, or nullptr if not found
   */
  template <typename T> std::shared_ptr<T> tryGetService() {
    static_assert(std::is_class_v<T>, "Service must be a class type");

    std::lock_guard<std::mutex> lock(m_mutex);

    auto typeIdx = std::type_index(typeid(T));
    auto it = m_services.find(typeIdx);
    if (it == m_services.end()) {
      return nullptr;
    }

    return std::any_cast<std::shared_ptr<T>>(it->second);
  }

  /**
   * @brief Check if a service is registered.
   *
   * @tparam T Service type to check
   * @return true if service is registered
   */
  template <typename T> bool hasService() {
    static_assert(std::is_class_v<T>, "Service must be a class type");

    std::lock_guard<std::mutex> lock(m_mutex);
    return m_services.find(std::type_index(typeid(T))) != m_services.end();
  }

  /**
   * @brief Shutdown all services in reverse registration order.
   *
   * Clears all service references, allowing shared_ptr cleanup.
   * Call this before application exit.
   */
  void shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Clear in reverse registration order
    for (auto it = m_registrationOrder.rbegin(); it != m_registrationOrder.rend(); ++it) {
      m_services.erase(*it);
    }
    m_registrationOrder.clear();
  }

  /**
   * @brief Get the number of registered services.
   * @return Count of registered services
   */
  size_t getServiceCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_services.size();
  }

  /**
   * @brief Check if any services are registered.
   * @return true if at least one service is registered
   *
   * @note For type-specific validation, use hasService<T>() with complete types.
   */
  bool isValid() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return !m_services.empty();
  }

  /**
   * @brief Reset the context (for testing only).
   *
   * Clears all services without ordered shutdown.
   * @warning Only use in test fixtures.
   */
  void reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_services.clear();
    m_registrationOrder.clear();
  }

private:
  ServiceContext() = default;
  ~ServiceContext() = default;

  mutable std::mutex m_mutex;
  std::unordered_map<std::type_index, std::any> m_services;
  std::vector<std::type_index> m_registrationOrder;
};

} // namespace orpheus
