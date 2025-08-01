# Razix Engine Release Notes

for verions 0.4x.yy.Dev

#### Version - 0.49.0 [Development] - 23/06/2025

##### Major Changes
*   Added Transient Resource Allocator skeleton with aliasing support
*   Refactored FrameGraph:
    *   Added lifetime tracking to FGResources
    *   Determined aliasing groups per resource entry
    *   Implemented per-frame transient resource reuse
*   Added UUID fix: consistent pretty string ↔ byte conversion (Fixes #340)
*   Renamed "Diana" to "House" for scene-to-GPU data synthesis architecture
*   Unified Gfx structs and added handles across graphics API
*   Switched Vulkan backend to use HLSL
*   Removed QT-based editor; switching to Blender-based asset workflow
*   Moved STL, memory, and Core into monolithic libraries

##### Frame Graph Visualizer Tool (FGVisTool)
*   Lifetime visualization with pass-based lifetime cells
*   Improved styling and layout consistency
*   Removed hardcoded constants
*   Debug utilities for dangling/dangling-writeonly entries
*   Proper aliasing group detection and visualization

##### DX12 Backend Preparation & Restore
These changes provide a stable Vulkan baseline for DX12 backend porting:
*   Restored Debug Draw and ImGui passes for Vulkan
*   Restored basic PBR IBL deferred rendering pipeline:
    *   Materials with SoA layout
    *   GBuffer
    *   PBR + IBL using CookTorrance
    *   Tonemapping pass
*   Restored HelloTriangle, HelloTexture, and Wave Intrinsics tests
*   Enabled GLM compatibility mode to use float4x4 etc. with HLSL style (#401)

##### Barriers and Resource Synchronization
*   Automatic barrier system implemented for Frame Graph resources (#400)
*   Fixed bugs with memory/layout barriers on resized attachments (#405)
*   Removed redundant buffer memory barriers (#407)

##### Testing Framework & Gfx Tests
*   Added GfxTestRunner with automatic test registration
*   Engine GFX tests:
    *   Screenshot comparison (RMS error)
    *   PPM golden image output
    *   Hello World graphics tests (x3)
*   Existing unit tests pass without failure

##### Serialization & Logging
*   Fixed UUID pretty string (de)serialization bug (#340)
*   Added logging-to-file system (#404)

##### Miscellaneous Fixes & Improvements
*   Added basic DualSense controller support
*   Minor engine cleanups
    *   Window resize bugs
    *   Script/test infra for Mac and CI
*   Fixed crash when project resolution > monitor resolution (#410)
*   Removed unwanted/redundant engine code


#### Version - 0.43.0 [Development] - 16/09/2023
    - Graphics API uses handles and resource manager/pools
    - improved frame graph; data-driven frame graph wip;
    - gained even more performance
    - fixed PBR lighting model
    - Editor minor improvements

#### Version - 0.42.0 [Development] - 02/07/2023
    - Single command buffer per frame
    - Improved synchronization and command buffer management 
    - Gained performance
    - Minor improvements and fixes
    - AABB boundingboxes can be visualised for all meshes now
    - Disabled imgui; fg resource barriers WIP

#### Version - 0.41.2 [Development] - 04/06/2023    
    - Patch for crash
#### Version - 0.41.1 [Development] - 04/06/2023
    - Patch for crash

#### Version - 0.41.0 [Development] - 04/06/2023
    
    - Added custom mesh and model formats and loaders for engine and RazixAssetPacker project
    - Improved editor controls
    - Improves scene Hierarchy and resolved issues
    - Added FrameGraph Editor project


#### Version - 0.40.0 [Development] - 08/04/2023
![](./Branding/DemoImages/ReleaseNotes/Razix_Engine_PBR_Lighting_Demo_3.png)
    
    - Added Frame Graph inspired from Frostbiet engine
    - added GBuffer, Shadow mapping, PBR + IBL lighting model
    - Imporved editing => Added material editor
    - Single world renderer to build the entire frame graph
    - Redesigned rendering architecture
    - used new vulkan dynamic rendering extension

#### Known Issues
- Distrubution build in not working
- CSM cascade matrices calculation is wrong
