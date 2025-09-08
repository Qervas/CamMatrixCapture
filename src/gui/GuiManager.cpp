#include "GuiManager.hpp"
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>

namespace SaperaCapturePro {

GuiManager::GuiManager() = default;

GuiManager::~GuiManager() {
  Shutdown();
}

bool GuiManager::Initialize(const std::string& window_title, int width, int height) {
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

  glfwMakeContextCurrent(window_);
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
  ui_scale_ = scale;
  ImGuiIO& io = ImGui::GetIO();
  io.FontGlobalScale = scale;
  
  // Apply scale to style
  ImGuiStyle& style = ImGui::GetStyle();
  style.ScaleAllSizes(scale);
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
  
  // Rounding
  style.WindowRounding = 0.0f;
  style.ChildRounding = 0.0f;
  style.FrameRounding = 0.0f;
  style.GrabRounding = 0.0f;
  style.PopupRounding = 0.0f;
  style.ScrollbarRounding = 0.0f;
  style.TabRounding = 0.0f;
}

}  // namespace SaperaCapturePro