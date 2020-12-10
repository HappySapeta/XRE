# XRE
Experimental Realtime Rendering Engine made with OpenGL and C++.


![alt text](https://github.com/AnupamSahu/XRE/blob/main/Screenshot%20(17).png)
![alt text](https://github.com/AnupamSahu/XRE/blob/main/Screenshot%20(13).png)

# Feature List
## Existing ##
* Rendering
   * Forward Shading
   * Deferred Shading
   * Blinn-Phong
   * Physically Based Rendering
   * Point Lights & Directional Light
* Advanced Lighting
   * Soft Shadows with Selective PCF only in Forward Shading.
   * Dynamic and Static Shadow Optimization.
   * Screen Space Ambient Occlusion (SSAO).
   * Variance Soft Shadows (currently supported in directional light only).
* Model and Texture Support
  * ASSIMP loader
  * Diffuse, Specular, Normal Textures Supported
* Post Processing
  * Bloom
  * HDR with exposure control

## Work in Progress ##
* Contact Hardening Soft Shadows

## Coming Up ##
* Image Based Lighting

## Planned ##
* Global Illumination

Educational Reference - https://learnopengl.com/

Libraries :
* https://www.assimp.org/
* https://glm.g-truc.net/0.9.9/index.html
* https://www.glfw.org/
