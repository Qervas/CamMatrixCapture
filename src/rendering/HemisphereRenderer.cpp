#include "HemisphereRenderer.hpp"
#include <glad/glad.h>
#include "../capture/AutomatedCaptureController.hpp" // For CapturePosition
#include <cmath>
#include <iostream>
#include <string>
#include <algorithm>

#define _USE_MATH_DEFINES
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace SaperaCapturePro {

// Vertex shader source
const char* vertex_shader_source = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aColor;

uniform mat4 uMVP;
uniform mat4 uModel;
uniform vec3 uColor;

out vec3 FragColor;
out vec3 Normal;
out vec3 WorldPos;

void main() {
    WorldPos = vec3(uModel * vec4(aPos, 1.0));
    Normal = normalize(mat3(uModel) * aNormal);
    FragColor = uColor.r >= 0.0 ? uColor : aColor; // Use uniform color if set, otherwise vertex color
    
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

// Fragment shader source
const char* fragment_shader_source = R"(
#version 330 core
out vec4 color;

in vec3 FragColor;
in vec3 Normal;
in vec3 WorldPos;

void main() {
    // Simple lighting
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
    float diff = max(dot(Normal, lightDir), 0.6); // Higher ambient for dark theme
    
    color = vec4(FragColor * diff, 1.0);
}
)";

HemisphereRenderer::HemisphereRenderer() = default;

HemisphereRenderer::~HemisphereRenderer() {
    Shutdown();
}

bool HemisphereRenderer::Initialize() {
    // GLAD should already be initialized by GuiManager
    
    // Create shader program
    if (!CreateShaderProgram()) {
        std::cout << "[HEMISPHERE] Failed to create shader program" << std::endl;
        return false;
    }
    
    // Generate mesh data
    GenerateHemisphereMesh();
    GenerateGridLines();
    GenerateCameraGizmos();
    
    // Set up vertex arrays
    SetupVertexArrays();
    
    std::cout << "[HEMISPHERE] Renderer initialized successfully" << std::endl;
    return true;
}

void HemisphereRenderer::Shutdown() {
    if (shader_program_) {
        glDeleteProgram(shader_program_);
        shader_program_ = 0;
    }
    
    GLuint buffers[] = {hemisphere_vbo_, hemisphere_ebo_, grid_vbo_, camera_vbo_};
    glDeleteBuffers(4, buffers);
    
    GLuint vaos[] = {hemisphere_vao_, grid_vao_, camera_vao_};
    glDeleteVertexArrays(3, vaos);
    
    hemisphere_vao_ = hemisphere_vbo_ = hemisphere_ebo_ = 0;
    grid_vao_ = grid_vbo_ = 0;
    camera_vao_ = camera_vbo_ = 0;
}

void HemisphereRenderer::Render(float view_azimuth, float view_elevation, float view_distance,
                               int viewport_width, int viewport_height,
                               const std::vector<CapturePosition>& positions) {
    if (!shader_program_) return;
    
    glUseProgram(shader_program_);
    
    // Set up view and projection matrices
    float projection[16], view[16], mvp[16], model[16];
    
    float aspect = static_cast<float>(viewport_width) / static_cast<float>(viewport_height);
    Perspective(projection, 45.0f * M_PI / 180.0f, aspect, 0.1f, 100.0f);
    
    // Calculate camera position based on spherical coordinates
    float cam_x = view_distance * std::cos(view_elevation) * std::sin(view_azimuth);
    float cam_y = view_distance * std::sin(view_elevation);
    float cam_z = view_distance * std::cos(view_elevation) * std::cos(view_azimuth);
    
    LookAt(view, cam_x, cam_y, cam_z, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    
    // Render hemisphere surface (semi-transparent)
    LoadIdentity(model);
    Multiply(mvp, projection, view);
    Multiply(mvp, mvp, model);
    
    glUniformMatrix4fv(mvp_matrix_location_, 1, GL_FALSE, mvp);
    glUniformMatrix4fv(model_matrix_location_, 1, GL_FALSE, model);
    glUniform3f(color_location_, 0.3f, 0.4f, 0.6f); // Blue hemisphere
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glBindVertexArray(hemisphere_vao_);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(hemisphere_indices_.size()), GL_UNSIGNED_INT, 0);
    
    glDisable(GL_BLEND);
    
    // Render grid lines
    glUniform3f(color_location_, 0.7f, 0.7f, 0.7f); // Light gray grid
    glBindVertexArray(grid_vao_);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(grid_vertices_.size()));
    
    // Update and render camera positions
    UpdateCameraPositions(positions);
    glBindVertexArray(camera_vao_);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(camera_vertices_.size()));
    
    glBindVertexArray(0);
    glUseProgram(0);
}

bool HemisphereRenderer::CreateShaderProgram() {
    GLuint vertex_shader = CompileShader(GL_VERTEX_SHADER, vertex_shader_source);
    if (!vertex_shader) return false;
    
    GLuint fragment_shader = CompileShader(GL_FRAGMENT_SHADER, fragment_shader_source);
    if (!fragment_shader) {
        glDeleteShader(vertex_shader);
        return false;
    }
    
    shader_program_ = glCreateProgram();
    glAttachShader(shader_program_, vertex_shader);
    glAttachShader(shader_program_, fragment_shader);
    glLinkProgram(shader_program_);
    
    GLint success;
    glGetProgramiv(shader_program_, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(shader_program_, 512, nullptr, infoLog);
        std::cout << "[HEMISPHERE] Shader program linking failed: " << infoLog << std::endl;
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        return false;
    }
    
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    
    // Get uniform locations
    mvp_matrix_location_ = glGetUniformLocation(shader_program_, "uMVP");
    model_matrix_location_ = glGetUniformLocation(shader_program_, "uModel");
    color_location_ = glGetUniformLocation(shader_program_, "uColor");
    
    return true;
}

GLuint HemisphereRenderer::CompileShader(GLenum type, const std::string& source) {
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cout << "[HEMISPHERE] Shader compilation failed: " << infoLog << std::endl;
        glDeleteShader(shader);
        return 0;
    }
    
    return shader;
}

void HemisphereRenderer::GenerateHemisphereMesh() {
    hemisphere_vertices_.clear();
    hemisphere_indices_.clear();
    
    const float radius = 3.5f;
    
    // Generate vertices
    for (int lat = 0; lat <= hemisphere_subdivisions_; ++lat) {
        float theta = (M_PI / 2.0f) * lat / hemisphere_subdivisions_; // 0 to π/2 for hemisphere
        float sin_theta = std::sin(theta);
        float cos_theta = std::cos(theta);
        
        for (int lon = 0; lon <= hemisphere_subdivisions_ * 2; ++lon) {
            float phi = (2.0f * M_PI) * lon / (hemisphere_subdivisions_ * 2);
            float sin_phi = std::sin(phi);
            float cos_phi = std::cos(phi);
            
            float x = radius * sin_theta * cos_phi;
            float y = radius * cos_theta;
            float z = radius * sin_theta * sin_phi;
            
            hemisphere_vertices_.emplace_back(x, y, z, 0.2f, 0.7f, 0.9f); // Bright cyan for dark theme
        }
    }
    
    // Generate indices
    for (int lat = 0; lat < hemisphere_subdivisions_; ++lat) {
        for (int lon = 0; lon < hemisphere_subdivisions_ * 2; ++lon) {
            int current = lat * (hemisphere_subdivisions_ * 2 + 1) + lon;
            int next = current + hemisphere_subdivisions_ * 2 + 1;
            
            hemisphere_indices_.push_back(current);
            hemisphere_indices_.push_back(next);
            hemisphere_indices_.push_back(current + 1);
            
            hemisphere_indices_.push_back(current + 1);
            hemisphere_indices_.push_back(next);
            hemisphere_indices_.push_back(next + 1);
        }
    }
}

void HemisphereRenderer::GenerateGridLines() {
    grid_vertices_.clear();
    
    const float radius = 2.0f;
    const float grid_color[3] = {0.7f, 0.7f, 0.7f};
    
    // Longitude lines (meridians)
    for (int i = 0; i < grid_lines_; ++i) {
        float phi = (2.0f * M_PI) * i / grid_lines_;
        float sin_phi = std::sin(phi);
        float cos_phi = std::cos(phi);
        
        for (int j = 0; j <= hemisphere_subdivisions_; ++j) {
            float theta = (M_PI / 2.0f) * j / hemisphere_subdivisions_;
            float sin_theta = std::sin(theta);
            float cos_theta = std::cos(theta);
            
            float x = radius * sin_theta * cos_phi;
            float y = radius * cos_theta;
            float z = radius * sin_theta * sin_phi;
            
            grid_vertices_.emplace_back(x, y, z, grid_color[0], grid_color[1], grid_color[2]);
        }
    }
    
    // Latitude lines (parallels)
    for (int i = 1; i < hemisphere_subdivisions_; ++i) {
        float theta = (M_PI / 2.0f) * i / hemisphere_subdivisions_;
        float sin_theta = std::sin(theta);
        float cos_theta = std::cos(theta);
        
        for (int j = 0; j <= grid_lines_ * 2; ++j) {
            float phi = (2.0f * M_PI) * j / (grid_lines_ * 2);
            float sin_phi = std::sin(phi);
            float cos_phi = std::cos(phi);
            
            float x = radius * sin_theta * cos_phi;
            float y = radius * cos_theta;
            float z = radius * sin_theta * sin_phi;
            
            grid_vertices_.emplace_back(x, y, z, grid_color[0], grid_color[1], grid_color[2]);
        }
    }
}

void HemisphereRenderer::GenerateCameraGizmos() {
    // Simple camera representation as small pyramids/cones
    camera_vertices_.clear();
    // Will be populated in UpdateCameraPositions
}

void HemisphereRenderer::UpdateCameraPositions(const std::vector<CapturePosition>& positions) {
    camera_vertices_.clear();
    
    const float radius = 3.5f; // Match hemisphere radius
    const float camera_size = 0.08f; // Slightly larger for visibility
    
    for (size_t i = 0; i < positions.size(); ++i) {
        const auto& pos = positions[i];
        
        // Convert spherical to cartesian
        float theta = (90.0f - pos.elevation) * M_PI / 180.0f; // Convert elevation to polar angle
        float phi = pos.azimuth * M_PI / 180.0f;
        
        float x = radius * std::sin(theta) * std::cos(phi);
        float y = radius * std::cos(theta);
        float z = radius * std::sin(theta) * std::sin(phi);
        
        // Color coding: bright green for captured, bright red for not captured 
        float r = pos.captured ? 0.1f : 1.0f;
        float g = pos.captured ? 1.0f : 0.1f;
        float b = pos.captured ? 0.1f : 0.1f;
        
        // Simple cube representation for cameras
        float vertices[][3] = {
            {x - camera_size, y - camera_size, z - camera_size},
            {x + camera_size, y - camera_size, z - camera_size},
            {x + camera_size, y + camera_size, z - camera_size},
            {x - camera_size, y + camera_size, z - camera_size},
            {x - camera_size, y - camera_size, z + camera_size},
            {x + camera_size, y - camera_size, z + camera_size},
            {x + camera_size, y + camera_size, z + camera_size},
            {x - camera_size, y + camera_size, z + camera_size}
        };
        
        // Define cube faces (2 triangles per face)
        int faces[][6] = {
            {0, 1, 2, 0, 2, 3}, // Front
            {4, 7, 6, 4, 6, 5}, // Back
            {0, 4, 5, 0, 5, 1}, // Bottom
            {2, 6, 7, 2, 7, 3}, // Top
            {0, 3, 7, 0, 7, 4}, // Left
            {1, 5, 6, 1, 6, 2}  // Right
        };
        
        for (int face = 0; face < 6; ++face) {
            for (int vertex = 0; vertex < 6; ++vertex) {
                int idx = faces[face][vertex];
                camera_vertices_.emplace_back(
                    vertices[idx][0], vertices[idx][1], vertices[idx][2],
                    r, g, b
                );
            }
        }
    }
    
    // Update camera VBO if it exists
    if (camera_vbo_) {
        glBindBuffer(GL_ARRAY_BUFFER, camera_vbo_);
        glBufferData(GL_ARRAY_BUFFER, 
                     camera_vertices_.size() * sizeof(Vertex), 
                     camera_vertices_.data(), 
                     GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
}

void HemisphereRenderer::SetupVertexArrays() {
    // Hemisphere VAO
    glGenVertexArrays(1, &hemisphere_vao_);
    glGenBuffers(1, &hemisphere_vbo_);
    glGenBuffers(1, &hemisphere_ebo_);
    
    glBindVertexArray(hemisphere_vao_);
    
    glBindBuffer(GL_ARRAY_BUFFER, hemisphere_vbo_);
    glBufferData(GL_ARRAY_BUFFER, 
                 hemisphere_vertices_.size() * sizeof(Vertex), 
                 hemisphere_vertices_.data(), 
                 GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, hemisphere_ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 
                 hemisphere_indices_.size() * sizeof(unsigned int), 
                 hemisphere_indices_.data(), 
                 GL_STATIC_DRAW);
    
    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(0);
    
    // Normal attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(1);
    
    // Color attribute
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));
    glEnableVertexAttribArray(2);
    
    // Grid VAO
    glGenVertexArrays(1, &grid_vao_);
    glGenBuffers(1, &grid_vbo_);
    
    glBindVertexArray(grid_vao_);
    glBindBuffer(GL_ARRAY_BUFFER, grid_vbo_);
    glBufferData(GL_ARRAY_BUFFER, 
                 grid_vertices_.size() * sizeof(Vertex), 
                 grid_vertices_.data(), 
                 GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));
    glEnableVertexAttribArray(2);
    
    // Camera VAO
    glGenVertexArrays(1, &camera_vao_);
    glGenBuffers(1, &camera_vbo_);
    
    glBindVertexArray(camera_vao_);
    glBindBuffer(GL_ARRAY_BUFFER, camera_vbo_);
    glBufferData(GL_ARRAY_BUFFER, 
                 camera_vertices_.size() * sizeof(Vertex), 
                 camera_vertices_.data(), 
                 GL_DYNAMIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));
    glEnableVertexAttribArray(2);
    
    glBindVertexArray(0);
}

// Matrix math implementations
void HemisphereRenderer::LoadIdentity(float matrix[16]) {
    for (int i = 0; i < 16; ++i) {
        matrix[i] = (i % 5 == 0) ? 1.0f : 0.0f;
    }
}

void HemisphereRenderer::Perspective(float matrix[16], float fov, float aspect, float near_plane, float far_plane) {
    LoadIdentity(matrix);
    float f = 1.0f / std::tan(fov / 2.0f);
    matrix[0] = f / aspect;
    matrix[5] = f;
    matrix[10] = (far_plane + near_plane) / (near_plane - far_plane);
    matrix[11] = -1.0f;
    matrix[14] = (2.0f * far_plane * near_plane) / (near_plane - far_plane);
    matrix[15] = 0.0f;
}

void HemisphereRenderer::LookAt(float matrix[16], float eyeX, float eyeY, float eyeZ,
                               float centerX, float centerY, float centerZ,
                               float upX, float upY, float upZ) {
    // Calculate forward vector
    float fx = centerX - eyeX;
    float fy = centerY - eyeY;
    float fz = centerZ - eyeZ;
    
    // Normalize forward
    float flen = std::sqrt(fx*fx + fy*fy + fz*fz);
    fx /= flen; fy /= flen; fz /= flen;
    
    // Calculate right vector (forward × up)
    float rx = fy * upZ - fz * upY;
    float ry = fz * upX - fx * upZ;
    float rz = fx * upY - fy * upX;
    
    // Normalize right
    float rlen = std::sqrt(rx*rx + ry*ry + rz*rz);
    rx /= rlen; ry /= rlen; rz /= rlen;
    
    // Calculate up vector (right × forward)
    upX = ry * fz - rz * fy;
    upY = rz * fx - rx * fz;
    upZ = rx * fy - ry * fx;
    
    // Build view matrix
    LoadIdentity(matrix);
    matrix[0] = rx; matrix[4] = rx; matrix[8] = -fx; matrix[12] = -(rx*eyeX + ry*eyeY + rz*eyeZ);
    matrix[1] = ry; matrix[5] = upY; matrix[9] = -fy; matrix[13] = -(upX*eyeX + upY*eyeY + upZ*eyeZ);
    matrix[2] = rz; matrix[6] = upZ; matrix[10] = -fz; matrix[14] = -(-fx*eyeX + -fy*eyeY + -fz*eyeZ);
    matrix[3] = 0; matrix[7] = 0; matrix[11] = 0; matrix[15] = 1;
}

void HemisphereRenderer::Multiply(float result[16], const float a[16], const float b[16]) {
    float temp[16];
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            temp[i*4 + j] = a[i*4 + 0] * b[0*4 + j] +
                           a[i*4 + 1] * b[1*4 + j] +
                           a[i*4 + 2] * b[2*4 + j] +
                           a[i*4 + 3] * b[3*4 + j];
        }
    }
    for (int i = 0; i < 16; ++i) {
        result[i] = temp[i];
    }
}

void HemisphereRenderer::Translate(float matrix[16], float x, float y, float z) {
    matrix[12] += x;
    matrix[13] += y;
    matrix[14] += z;
}

void HemisphereRenderer::Scale(float matrix[16], float x, float y, float z) {
    matrix[0] *= x; matrix[4] *= x; matrix[8] *= x; matrix[12] *= x;
    matrix[1] *= y; matrix[5] *= y; matrix[9] *= y; matrix[13] *= y;
    matrix[2] *= z; matrix[6] *= z; matrix[10] *= z; matrix[14] *= z;
}

void HemisphereRenderer::RotateY(float matrix[16], float angle) {
    float c = std::cos(angle);
    float s = std::sin(angle);
    float temp[16];
    for (int i = 0; i < 16; ++i) temp[i] = matrix[i];
    
    matrix[0] = c * temp[0] + s * temp[8];
    matrix[4] = c * temp[4] + s * temp[12];
    matrix[8] = c * temp[8] - s * temp[0];
    matrix[12] = c * temp[12] - s * temp[4];
    
    matrix[2] = c * temp[2] + s * temp[10];
    matrix[6] = c * temp[6] + s * temp[14];
    matrix[10] = c * temp[10] - s * temp[2];
    matrix[14] = c * temp[14] - s * temp[6];
}

} // namespace SaperaCapturePro