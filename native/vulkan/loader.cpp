#include "loader.h"
#include <string_view>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif
namespace superman_returns::graphics::vulkan {
bool Check(VkResult r, const char *op, Error &e) {
  if (r == VK_SUCCESS)
    return true;
  e = {op, r, std::string(op) + " failed (VkResult " + std::to_string(r) + ")"};
  return false;
}
Loader::~Loader() {
  if (module_) {
#ifdef _WIN32
    FreeLibrary(static_cast<HMODULE>(module_));
#else
    dlclose(module_);
#endif
  }
}
bool Loader::Open(Error &e, std::string path) {
  if (module_)
    return true;
#ifdef _WIN32
  if (path.empty())
    path = "vulkan-1.dll";
  module_ = LoadLibraryExA(path.c_str(), nullptr,
                           LOAD_LIBRARY_SEARCH_SYSTEM32 |
                               LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
  if (module_)
    proc_ = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
        GetProcAddress(static_cast<HMODULE>(module_), "vkGetInstanceProcAddr"));
#else
  if (path.empty()) {
#ifdef __ANDROID__
    path = "libvulkan.so";
#else
    path = "libvulkan.so.1";
#endif
  }
  module_ = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
  if (module_)
    proc_ = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
        dlsym(module_, "vkGetInstanceProcAddr"));
#endif
  if (!module_ || !proc_) {
    e = {"Vulkan loader", VK_ERROR_INITIALIZATION_FAILED,
         "Vulkan loader unavailable: " + path};
    return false;
  }
  return true;
}
bool Loader::LoadGlobal(Dispatch &f, Error &e) {
#define VK_GLOBAL(n)                                                           \
  f.n = reinterpret_cast<PFN_##n>(proc_(VK_NULL_HANDLE, #n));                  \
  if (!f.n && std::string_view(#n) != "vkEnumerateInstanceVersion") {          \
    e = {#n, VK_ERROR_INITIALIZATION_FAILED,                                   \
         "Required Vulkan function unavailable"};                              \
    return false;                                                              \
  }
#define VK_INSTANCE(n)
#define VK_DEVICE(n)
#include "functions.inc"
#undef VK_GLOBAL
#undef VK_INSTANCE
#undef VK_DEVICE
  return true;
}
bool Loader::LoadInstance(Dispatch &f, VkInstance i, Error &e) {
#define VK_GLOBAL(n)
#define VK_INSTANCE(n)                                                         \
  f.n = reinterpret_cast<PFN_##n>(proc_(i, #n));                               \
  if (!f.n && std::string_view(#n) != "vkCreateDebugUtilsMessengerEXT" &&      \
      std::string_view(#n) != "vkDestroyDebugUtilsMessengerEXT") {             \
    e = {#n, VK_ERROR_INITIALIZATION_FAILED,                                   \
         "Required Vulkan function unavailable"};                              \
    return false;                                                              \
  }
#define VK_DEVICE(n)
#include "functions.inc"
#undef VK_GLOBAL
#undef VK_INSTANCE
#undef VK_DEVICE
  return true;
}
bool Loader::LoadDevice(Dispatch &f, VkDevice d, Error &e) {
#define VK_GLOBAL(n)
#define VK_INSTANCE(n)
#define VK_DEVICE(n)                                                           \
  f.n = reinterpret_cast<PFN_##n>(f.vkGetDeviceProcAddr(d, #n));               \
  if (!f.n) {                                                                  \
    e = {#n, VK_ERROR_INITIALIZATION_FAILED,                                   \
         "Required Vulkan function unavailable"};                              \
    return false;                                                              \
  }
#include "functions.inc"
#undef VK_GLOBAL
#undef VK_INSTANCE
#undef VK_DEVICE
  return true;
}
} // namespace superman_returns::graphics::vulkan
