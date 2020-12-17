# XRE
Experimental Realtime Rendering Engine made with OpenGL and C++.


![alt text](https://github.com/AnupamSahu/XRE/blob/main/Screenshot%20(17).png)
![alt text](https://github.com/AnupamSahu/XRE/blob/main/Sponza_sun.png)

# Feature List
## Existing ##
* Rendering
   * Forward Shading
   * Deferred Shading
   * Blinn-Phong
   * Physically Based Rendering
   * Point Lights & Directional Light
* Advanced Lighting
   * Dynamic and Static Shadow Optimization.
   * Screen Space Ambient Occlusion (SSAO).
   * Variance Soft Shadows (currently works with directional lights only)
* Model and Texture Support
  * ASSIMP loader
  * Diffuse, Specular, Normal Textures Supported
* Post Processing
  * Bloom
  * HDR with exposure control
* Pipeline Optimization
  * Frustum Culling
  * Occlusion Culling (WIP)

## Work in Progress ##
* Image Based Lighting

## Coming Up ##
* Basic Particle Simulation

## Planned ##
* Screen Space Reflections
* Summed Area Variance Shadow Maps

Educational References :
* https://learnopengl.com/
* https://developer.nvidia.com/gpugems/gpugems/contributors

Libraries :
* https://www.assimp.org/
* https://glm.g-truc.net/0.9.9/index.html
* https://www.glfw.org/

(Warning for anyone attempting to clone and build this project - There are multiple external links and paths that I haven't properly defined, and will cause linker problems. I plan to fix that as soon as possible but only after I have made some important features.)
