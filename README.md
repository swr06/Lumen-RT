<!-- Allow this file to not have a first line heading -->
<!-- markdownlint-disable-file MD041 -->

<!-- inline html -->
<!-- markdownlint-disable-file MD033 -->

<div align="center">

# The Candela Engine
  
<img src="https://github.com/swr06/Candela/blob/MainBranch/Branding/logo.png" data-canonical-src="https://github.com/swr06/Candela/blob/MainBranch/Branding/logo.png" width="240" height="240" />
  
</div>
</br>

Candela is an ***experimental*** engine that prioritizes both performance and visuals. The primary objective of the engine was to serve as a tool for enhancing my knowledge and understanding of light transport, filtering, physically-based rendering, volumetrics and intersection algorithms. The engine is entirely built from scratch using C++17 and the modern OpenGL programmable pipeline.

</div>

# Project Status 
Since I have to primarily focus on school, development on the engine will be slow. However, I have no intentions of abandoning it as of yet. 
It is also worth noting that I also work on other projects and shaders in my free time.

# Current Feature List 
- Model loading, abstracted program/application API
- Procedural Normal Map Generation (Using sobel operator)
- Custom SAH BVH Constructor and Ray Intersection API
- Support for Stack/Stackless BVH traversal
- Physically based direct lighting (Cook Torrance BRDF)
- Cascaded Shadow Mapping + PCF Filtering 
- Screenspace shadowing 
- Ray traced Diffuse Global illumination 
- Screenspace global illumination
- Infinite bounce GI using irradiance probes
- Ray traced Ambient Occlusion (RTAO)
- Hemispherical Shadow Maps for a sky shadowing approximation
- Hybrid Specular GI (SSR + World Space RT)
- SVGF + Specialized Specular Denoiser
- Temporal Anti Aliasing + Upscaling
- Fast Approximate Anti Aliasing (Based on FXAA 3.11 by Nvidia)
- Spatial Image Upscaling (Custom CAS + AMD FSR EASU)
- Volumetrics (Direct + Indirect light contribution)
- Upscaling (Temporal/Spatial)
- Culling (Frustum/Face Culling)
- HDR Environment Map Support 
- Post Processing Pipeline (Bloom, DoF, Purkinje Effect, Grain, Chromatic Aberration, Color Dithering, ACES Tonemapping, Procedural Lens Flare etc.)
- Basic Editor Features + Debug Views
- Transparent/Refractive Material Support (Weighted Blended OIT, Stochastic OIT, Screenspace Refractions and Screenspace caustics)

# Planned Features
- Realistic sky rendering (Volumetric atmosphere and clouds) 
- Antileak algorithm for the probe system
- ReStir GI
- Fast Transparent Shadows
- IES lights
- LTC for area lighting 
- RSM 
- Bent Normals 
- GTAO
- Particle system with screenspace lighting 
- Voxelization and VXCT 
- Screenspace cone tracing 
- Constant time parallax mapping
- Raytraced Acoustics (WIP) 


# Requirements  
- GPU : One with >= 2 GB of vRAM that supports OpenGL 4.5 and the bindless texture OpenGL extension (ARB_bindless_texture).
- CPU : A decent 64 bit CPU should be fine. 
- RAM : >= 4 GB should be fine.

# Performance 
Runs at around 24 fps on a AMD Vega 11 (desktop) iGPU. 

# Known Issues
- Volumetrics and infinite bounce GI prone to light leaking.
- Temporal/Spatial artifacts (usually in the form of noise, ghosting or temporal lag) on aggressive movement or sudden lighting changes.

# Reports 
If you find a bug and want to report it, you could contact me by email/discord (see profile page) or alternatively, open a GitHub issue.

# Notes
- Refer `Controls.txt` for engine controls/shortcuts.
- Refer `Additional Notes.txt` for additional general info.

# License 
See `LICENSE` in the project's root directory.

# Credits 
See `Credits.txt`.

# Screenshots

</br>

![s18](https://github.com/swr06/Candela/blob/MainBranch/Screenshots/18.png)

</br>

</br>

![s19](https://github.com/swr06/Candela/blob/MainBranch/Screenshots/19.png)

</br>

</br>

![s20](https://github.com/swr06/Candela/blob/MainBranch/Screenshots/20.png)

</br>

</br>

![i0](https://github.com/swr06/Candela/blob/MainBranch/Screenshots/image.webp)

</br>

</br>

![i1](https://github.com/swr06/Candela/blob/MainBranch/Screenshots/image1.webp)

</br>


</br>

![i2](https://github.com/swr06/Candela/blob/MainBranch/Screenshots/image2.webp)

</br>

</br>

![i3](https://github.com/swr06/Candela/blob/MainBranch/Screenshots/image3.webp)

</br>

</br>

![i4](https://github.com/swr06/Candela/blob/MainBranch/Screenshots/image4.webp)

</br>

</br>

![i5](https://github.com/swr06/Candela/blob/MainBranch/Screenshots/image5.webp)

</br>

</br>

![s1](https://github.com/swr06/Candela/blob/MainBranch/Screenshots/1.png)

</br>

</br>

![s2](https://github.com/swr06/Candela/blob/MainBranch/Screenshots/2.png)

</br>

</br>

![s3](https://github.com/swr06/Candela/blob/MainBranch/Screenshots/3.png)

</br>

</br>

![s12](https://github.com/swr06/Candela/blob/MainBranch/Screenshots/12.png)

</br>

</br>

![s13](https://github.com/swr06/Candela/blob/MainBranch/Screenshots/13.png)

</br>

</br>

![s4](https://github.com/swr06/Candela/blob/MainBranch/Screenshots/4.png)

</br>

</br>

![s5](https://github.com/swr06/Candela/blob/MainBranch/Screenshots/5.png)

</br>

</br>

![s6](https://github.com/swr06/Candela/blob/MainBranch/Screenshots/6.png)

</br>

</br>

![s7](https://github.com/swr06/Candela/blob/MainBranch/Screenshots/7.png)

</br>

</br>

![s8](https://github.com/swr06/Candela/blob/MainBranch/Screenshots/8.png)

</br>

</br>

![s9](https://github.com/swr06/Candela/blob/MainBranch/Screenshots/9.png)

</br>


</br>

![s10](https://github.com/swr06/Candela/blob/MainBranch/Screenshots/10.png)

</br>

</br>

![s14](https://github.com/swr06/Candela/blob/MainBranch/Screenshots/14.png)

</br>

</br>

![s15](https://github.com/swr06/Candela/blob/MainBranch/Screenshots/15.png)

</br>

</br>

![s16](https://github.com/swr06/Candela/blob/MainBranch/Screenshots/16.png)

</br>

</br>

![s17](https://github.com/swr06/Candela/blob/MainBranch/Screenshots/17.png)

</br>

</br>

![s21](https://github.com/swr06/Candela/blob/MainBranch/Screenshots/21.png)

</br>


# Supporting

If you like this project and would like to show your support, please consider starring the project on github. :)




