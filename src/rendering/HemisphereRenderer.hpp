#pragma once

#include <vector>
#include <string>
#include <glad/glad.h>
#include "../gui/AutomatedCapturePanel.hpp" // For CapturePosition

namespace SaperaCapturePro {

struct Vertex {
    float position[3];
    float normal[3];
    float color[3];
    
    Vertex() : position{0,0,0}, normal{0,0,0}, color{1,1,1} {}
    Vertex(float x, float y, float z, float r = 1.0f, float g = 1.0f, float b = 1.0f) 
        : position{x,y,z}, normal{x,y,z}, color{r,g,b} {}
};

class HemisphereRenderer {
public:
    HemisphereRenderer();
    ~HemisphereRenderer();
    
    bool Initialize();
    void Shutdown();
    
    void Render(float view_azimuth, float view_elevation, float view_distance,
                int viewport_width, int viewport_height,
                const std::vector<CapturePosition>& positions);

private:
    // OpenGL resources
    GLuint shader_program_ = 0;
    GLuint hemisphere_vao_ = 0;
    GLuint hemisphere_vbo_ = 0;
    GLuint hemisphere_ebo_ = 0;
    GLuint grid_vao_ = 0;
    GLuint grid_vbo_ = 0;
    GLuint camera_vao_ = 0;
    GLuint camera_vbo_ = 0;
    
    // Mesh data
    std::vector<Vertex> hemisphere_vertices_;
    std::vector<unsigned int> hemisphere_indices_;
    std::vector<Vertex> grid_vertices_;
    std::vector<Vertex> camera_vertices_;
    
    // Shader uniforms
    GLint mvp_matrix_location_ = -1;
    GLint model_matrix_location_ = -1;
    GLint color_location_ = -1;
    
    // Generation parameters
    int hemisphere_subdivisions_ = 20;
    int grid_lines_ = 12;
    
    // Internal methods
    bool CreateShaderProgram();
    void GenerateHemisphereMesh();
    void GenerateGridLines();
    void GenerateCameraGizmos();
    void UpdateCameraPositions(const std::vector<CapturePosition>& positions);
    
    GLuint CompileShader(GLenum type, const std::string& source);
    void SetupVertexArrays();
    
    // Matrix math helpers
    void LoadIdentity(float matrix[16]);
    void Perspective(float matrix[16], float fov, float aspect, float near_plane, float far_plane);
    void LookAt(float matrix[16], float eyeX, float eyeY, float eyeZ, 
                float centerX, float centerY, float centerZ, 
                float upX, float upY, float upZ);
    void Multiply(float result[16], const float a[16], const float b[16]);
    void Translate(float matrix[16], float x, float y, float z);
    void Scale(float matrix[16], float x, float y, float z);
    void RotateY(float matrix[16], float angle);
};

} // namespace SaperaCapturePro