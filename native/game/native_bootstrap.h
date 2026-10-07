#pragma once
#include <filesystem>
#include <memory>
#include <rex/system/interfaces/graphics.h>
namespace superman_returns::android {
std::unique_ptr<rex::system::IGraphicsSystem> CreateNativeVulkanGraphicsSystem(const std::filesystem::path& files_root);
void SetNativePaused(bool paused);
}
