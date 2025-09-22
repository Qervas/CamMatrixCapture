#include "GuiManager.hpp"
#include <glad/glad.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#include <filesystem>
#include <algorithm>

namespace SaperaCapturePro {

GuiManager::GuiManager() = default;

GuiManager::~GuiManager() {
  Shutdown();
}

bool GuiManager::Initialize(const std::string& window_title, int width, int height, int x, int y) {
  // Initialize GLFW
  if (!glfwInit()) {
    std::cerr << "Failed to initialize GLFW" << std::endl;
    return false;
  }

  // Configure GLFW
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  // Create window
  window_ = glfwCreateWindow(width, height, window_title.c_str(), nullptr, nullptr);
  if (!window_) {
    std::cerr << "Failed to create GLFW window" << std::endl;
    glfwTerminate();
    return false;
  }
  
  // Set window position if provided
  if (x >= 0 && y >= 0) {
    glfwSetWindowPos(window_, x, y);
  }

  glfwMakeContextCurrent(window_);
  
  // Initialize GLAD after OpenGL context is current
  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    std::cerr << "Failed to initialize GLAD" << std::endl;
    glfwTerminate();
    return false;
  }
  
  glfwSwapInterval(1);  // Enable vsync

  // Setup ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

  // Setup ImGui style
  SetupImGuiStyle();

  // Determine DPI/content scale and choose comfortable font size + geometry scaling
  float xscale = 1.0f, yscale = 1.0f;
  glfwGetWindowContentScale(window_, &xscale, &yscale);
  float dpi_scale = std::max(xscale, yscale);
  if (dpi_scale <= 0.0f) dpi_scale = 1.0f;

  // Base comfortable size: 18px at 96 DPI; clamp to a reasonable range
  float font_size_px = std::clamp(18.0f * dpi_scale, 18.0f, 28.0f);

  // Load larger, crisp default fonts (prefer system Segoe UI on Windows)
  io.Fonts->Clear();
  bool loaded_font = false;
  try {
    const char* segoe_path = "C:/Windows/Fonts/segoeui.ttf";
    if (std::filesystem::exists(segoe_path)) {
      io.Fonts->AddFontFromFileTTF(segoe_path, font_size_px);
      loaded_font = true;
    }
  } catch (...) {
    // ignore
  }
  if (!loaded_font) {
    ImFontConfig cfg;
    cfg.SizePixels = font_size_px;
    io.Fonts->AddFontDefault(&cfg);
  }
  io.FontDefault = io.Fonts->Fonts.back();
  io.FontGlobalScale = 1.0f; // keep text crisp

  // Scale geometry (widgets, padding) to match DPI, without scaling the font rendering
  ImGuiStyle& style = ImGui::GetStyle();
  style.ScaleAllSizes(dpi_scale);

  // Setup Platform/Renderer backends
  ImGui_ImplGlfw_InitForOpenGL(window_, true);
  ImGui_ImplOpenGL3_Init("#version 330");

  return true;
}

void GuiManager::Shutdown() {
  if (window_) {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    glfwDestroyWindow(window_);
    glfwTerminate();
    window_ = nullptr;
  }
}

bool GuiManager::ShouldClose() const {
  return window_ && glfwWindowShouldClose(window_);
}

void GuiManager::BeginFrame() {
  glfwPollEvents();
  
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
}

void GuiManager::EndFrame() {
  ImGui::Render();
  
  int display_w, display_h;
  glfwGetFramebufferSize(window_, &display_w, &display_h);
  glViewport(0, 0, display_w, display_h);
  glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  
  // Update and Render additional Platform Windows
  ImGuiIO& io = ImGui::GetIO();
  if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    GLFWwindow* backup_current_context = glfwGetCurrentContext();
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
    glfwMakeContextCurrent(backup_current_context);
  }
  
  glfwSwapBuffers(window_);
}

void GuiManager::SetUIScale(float scale) {
  // Clamp scale to valid range
  ui_scale_ = std::clamp(scale, 0.5f, 8.0f);
  
  ImGuiIO& io = ImGui::GetIO();
  io.FontGlobalScale = ui_scale_;
  
  // Reset style first to avoid cumulative scaling
  ImGuiStyle& style = ImGui::GetStyle();
  style = ImGuiStyle();  // Reset to defaults
  ApplyDarkTheme();      // Reapply theme
  
  // Now apply the scale
  style.ScaleAllSizes(ui_scale_);
  
  // Adjust window min size for extreme scales
  if (ui_scale_ > 2.0f) {
    io.ConfigWindowsResizeFromEdges = true;
    style.WindowMinSize = ImVec2(100 * ui_scale_, 50 * ui_scale_);
  }
}

void GuiManager::SetVSyncEnabled(bool enabled) {
  glfwSwapInterval(enabled ? 1 : 0);
}

void GuiManager::SetupImGuiStyle() {
  ApplyDarkTheme();
}

void GuiManager::ApplyDarkTheme() {
  ImGuiStyle& style = ImGui::GetStyle();
  
  // Dark color scheme
  style.Colors[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
  style.Colors[ImGuiCol_Header] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
  style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
  style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
  style.Colors[ImGuiCol_Button] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
  style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
  style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
  style.Colors[ImGuiCol_FrameBg] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
  style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
  style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
  style.Colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
  style.Colors[ImGuiCol_TabHovered] = ImVec4(0.38f, 0.38f, 0.38f, 1.0f);
  style.Colors[ImGuiCol_TabActive] = ImVec4(0.28f, 0.28f, 0.28f, 1.0f);
  style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
  style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
  style.Colors[ImGuiCol_TitleBg] = ImVec4(0.04f, 0.04f, 0.04f, 1.0f);
  style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.16f, 0.16f, 1.0f);
  style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.0f, 0.0f, 0.0f, 0.6f);
  style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.0f);
  
  // Spacing & sizing for better readability
  style.WindowPadding = ImVec2(16.0f, 12.0f);
  style.FramePadding = ImVec2(12.0f, 8.0f);
  style.ItemSpacing = ImVec2(12.0f, 10.0f);
  style.ItemInnerSpacing = ImVec2(10.0f, 8.0f);
  style.ScrollbarSize = 18.0f;
  style.GrabMinSize = 12.0f;
  style.FrameBorderSize = 1.0f;
  style.WindowBorderSize = 1.0f;

  // Rounding for modern look
  style.WindowRounding = 6.0f;
  style.ChildRounding = 6.0f;
  style.FrameRounding = 6.0f;
  style.GrabRounding = 6.0f;
  style.PopupRounding = 6.0f;
  style.ScrollbarRounding = 6.0f;
  style.TabRounding = 6.0f;
}

}  // namespace SaperaCapturePro