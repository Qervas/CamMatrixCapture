#pragma once

#include <memory>
#include <string>
#include <GLFW/glfw3.h>
#include <imgui.h>

namespace SaperaCapturePro {

class GuiManager {
 public:
  GuiManager();
  ~GuiManager();

  bool Initialize(const std::string& window_title, int width, int height, int x = -1, int y = -1);
  void Shutdown();

  bool ShouldClose() const;
  void BeginFrame();
  void EndFrame();

  void SetUIScale(float scale);
  float GetUIScale() const { return ui_scale_; }

  GLFWwindow* GetWindow() const { return window_; }
  
  void SetVSyncEnabled(bool enabled);
  
 private:
  GLFWwindow* window_ = nullptr;
  float ui_scale_ = 1.0f;
  
  void SetupImGuiStyle();
  void ApplyDarkTheme();
};

}  // namespace SaperaCapturePro