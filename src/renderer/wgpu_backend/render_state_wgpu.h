#pragma once

#include "math/skl_math_consts.h"
#include "renderer/render_types.h"

#include <map>
#include <vector>
#include <webgpu/webgpu.h>

// Represents the transformation data of the camera
struct WGPUCameraData
{
    glm::mat4 m_view;
    glm::mat4 m_proj;
    glm::vec3 m_pos;

    WGPUCameraData() :
        m_view(),
        m_proj(),
        m_pos()
    { }

    WGPUCameraData(glm::mat4 setView, glm::mat4 setProj, glm::vec3 setPos) :
        m_view(setView),
        m_proj(setProj),
        m_pos(setPos)
    { }
};

// Represents a single directional light
struct WGPUDirLight {
    glm::mat4x4 m_lightSpace;
    glm::vec3 m_color;

    WGPUDirLight() :
        m_lightSpace(),
        m_color()
    { }

    WGPUDirLight(glm::mat4x4 lightSpace, glm::vec3 color) :
        m_lightSpace(lightSpace),
        m_color(color)
    { }
        
};

// Represents the transformation data of the objects in the scene
struct WGPUObjectData
{
    glm::mat4x4 m_transform;
    glm::vec4 m_color;

    WGPUObjectData() :
        m_transform(),
        m_color()
    { }

    WGPUObjectData(glm::mat4x4 transform, glm::vec4 color) : 
        m_transform(transform),
        m_color(color)
    { }
};

// Represents the input from a scene needed to render a single frame with webgpu
struct WGPURenderState {
    WGPUCameraData m_mainCam;
    std::vector<WGPUDirLight> m_dirLight; // TODO: Currently does nothing
    std::vector<WGPUObjectData> m_objData;
    std::map<MeshID, u32> m_meshCounts;
};
