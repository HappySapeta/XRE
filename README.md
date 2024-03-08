# XRE
An Experimental Realtime Rendering Engine made with OpenGL and C++.


https://github.com/AnupamSahu/XRE/assets/35849508/1b2f6472-66c1-497d-9373-2f8544774167


# Features
* Rendering
   * Deferred Rendering
   * Physically Based Rendering
* Post Processing
   * Screen Space Ambient Occlusion (SSAO)
   * Exponential Variance Soft Shadows
   * Bloom
   * HDR
* Model and Texture Support
  * ASSIMP loader
  * Diffuse, Specular, Normal, Occlusion Textures Supported
* Optimization
  * Frustum Culling
  * Cached Shadows
  * G-Buffer optimization (PBR - 120 bits, BlinnPhong - 112 bits)

References :
* https://learnopengl.com/
* https://developer.nvidia.com/gpugems/gpugems/contributors

Libraries used :
* https://www.assimp.org/
* https://glm.g-truc.net/0.9.9/index.html
* https://www.glfw.org/
