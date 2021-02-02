# XRE
Experimental Realtime Rendering Engine made with OpenGL and C++.


![alt text](https://github.com/AnupamSahu/XRE/blob/main/Screenshot%20(17).png)
![alt text](https://github.com/AnupamSahu/XRE/blob/main/Sponza_sun.png)

# Feature List
## Existing ##
* Rendering
   * Forward Rendering
   * Deferred Rendering
   * Blinn-Phong Shading
   * Physically Based Rendering
   * Point Lights & Directional Light
* Advanced Lighting
   * Screen Space Ambient Occlusion (SSAO).
   * Exponential Variance Soft Shadows (currently works with directional lights only)
   * Light Probes for Diffuse Irradiance and Specular Reflections
* Model and Texture Support
  * ASSIMP loader
  * Diffuse, Specular, Normal, Occlusion Textures Supported
* Post Processing
  * Bloom
  * HDR
* Pipeline Optimization
  * Frustum Culling
  * Dynamic and Static shadow separation
  * Delayed Shadow Updates
  * Thin G-Buffer (PBR - 120 bits, BlinnPhong - 112 bits)

## Work in Progress ##
* Point Soft Shadows


Educational References :
* https://learnopengl.com/
* https://developer.nvidia.com/gpugems/gpugems/contributors

Libraries :
* https://www.assimp.org/
* https://glm.g-truc.net/0.9.9/index.html
* https://www.glfw.org/

(Warning for anyone attempting to clone and build this project - There are multiple external links and paths that I haven't properly defined, and will cause linker problems. I plan to fix that as soon as possible but only after I have completed building my planned set of features.)
