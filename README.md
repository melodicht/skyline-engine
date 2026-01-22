Skyline Engine
=============

# Directory Overview

## Development Folders
- `cmake`: For CMake add-ons and scripts, used by our build process.
- `data`: The assets for our game, containing models and fonts.
- `include`: Contains headers of src files and external libraries.
- `shaders`: Contains shader code.
  - The root holds `slang` shaders.
  - `webgpu`: Contains WebGPU shaders.
- `src`: Contains the source code for the game engine. See the components overview below for more details.
- `.gitignore`: Files we do not want in our Git repository.
- `CMakeLists.txt`: The configuration file for CMake, the development tool we rely on for building the game engine.
- `README.md`: Provides informative and instructional content related to the game engine.
- `notes.txt`: Notes taken from code walks.

## Asset Folders
- `scenes`: Contains specific scene toml layouts.
- `fonts`: Contains text fonts in ttf folder.
- `textures`: Contains textures.
- `models`: Contains gltf models.

## Build Folders
- `build`: Artifacts of the build process. Delete the contents of this folder and add back in the `.gitkeep` in order to build from scratch.
- `bin`: Holds binaries of dynamic libraries.
- `shaderbin`: Holds shaders processed on compilation.
- `build_external`: Holds build artifacts from external libraries.
- `build-release`: ???? <IDK what that does someone else please specify> ????

# Game-as-a-service Architecture

The game engine uses a game-as-a-service architecture. In this architecture, there are two components: the platform component and the game component. Each must be built separately. The platform component is the main entry point of the program, and dynamically loads the game component. From this point onwards, we shall refer to the "game component" as the game module.

The main idea behind this architecture is that for every new platform we would like to support, we can write a new implementation of the platform component, and the game module would remain the same. The game module is our game as a mathematical object, insulated from the non-mathematical nature of operating systems.

Currently, we only have one implementation for the platform component, which uses SDL3. Conveniently, SDL3 works for all operating systems used by most people, so can get away with just using that as our sole implementation for the platform component. It is still beneficial to write platform components tailor-made to a specific operating system (or more broadly, a platform), as we can squeeze the full capability of that specific operating system without having to worry about compatibility with another operating system.

The greatest benefit of this architecture is that while the game is running, the game module can be replaced with a new game module, also known as hot reloading.

# Single Translation Unit Build / Unity Build

The way build the code is that we essentially concatenate all the C++ files into one giant C++ file and then compile that. Why do this? Because it's faster and simplifies the build process. The trade off is that we lose the ability to enforce encapsulation between C++ files. As in, nothing is stopping one file from reading values from another file.

We call one concatentation of a set of C++ files into one giant C++ file that gets compiled a translation unit.

We have one translation unit per component. That is, one for the platform, and one for the game. That is why the hot reloading works. We can compile the entire game module independently from the platform one.

When there are several translation units in a project, and some of them reference the same C++ file, what this means is the contents of the C++ file will get duplicared across the translation units. This is usually an acceptable cost. Though, when it's not (for other reasons that we won't go into here), it justifies a new translation unit for the shared piece of code. 

# Folder Structure

## src/platform
Contains the source files for the platform executable
## src/game
Contains the source files for the game module dll
## src/renderer/vk_backend, gl_backend, wgpu_backend
Contains the source files for the various rendering backends
## src/utils
Contains the source file for the utils static library that contains functionality shared between the different modules, such as math and debug functionality.
## include/platform
Contains the header files used by the platform executable
## include/game
Contains the header files used by the game module dll
## include/renderer/vulkan, gl, webgpu
Contains the header files used by the various rendering backends
## include/shared
Contains the header files that are shared between multiple different modules

# Components Overview

- Renderer implementations
  - Only one of which can be used.

- Sub components that are used by both components:
  - meta_definitions.h
    - It's meta because we are inserting our own features into the programming language, almost as if we are programming in a variation of C++.
    - We hvae shorthands for fixed width numerical types.
    - We split C++'s `static` up to their more specific use-cases
    - We add built-in "functions" (they are actually macro)
  - game_platform.h.
    - It's the interface between the platform module and the game module.
    - The platform only needs to know the signatures.
    - The game module actually needs to implement it.
  - render_game.h
    - The subset of renderer interface used by the game module.
  - math (our math, not the stdlib one)
  - asset_utils.cpp



- Game.cpp is the entry point for the game module.
  - game.h (holds the components of the ECS)
    - game_platform.h (mentioning it here because it is within game.h)
    - thread_safe_primtives.h
    - debug.h
    - GLM
    - Jolt
    - renderer
  - stb_image.h
  - asset_types.h
  - renderer
  - ecs (form)
  - physics
  - systems (substance for ECS)

- main.cpp is the entry point for the platform module.
  - SDL3
    - emscripten
  - imgui.h
  - asset_types.h
  - render_backend.h

  - main.h


# Render Backend

## Vulkan 
Currently the most up to date graphics backend.

Install instructions:
- https://vulkan.lunarg.com/sdk/home
- Install the latest version. As of writing this, we know that 1.4.328.1
  works.
- When installing, don't need to tick any of the boxes when asking to install addons.

## WebGPU
May not yet support certain features that Vulkan backend has, however works on more platforms. 

Install Instructions:
- Installation instructions depend on what lower level graphics API you want to use underneath WebGPU
- Find supported WebGPU backends [here] (https://github.com/google/dawn/blob/main/docs/support.md)

# Build options

### SKL_RENDER_SYS
Currently defines what graphics API backend that the game engine will use.

Current options include "Vulkan", "WebGPU", and "Default"

Defaults to Vulkan, however if Vulkan found or able to be run natively the WebGPU backend is used instead.

### SKL_INTERNAL
To be turned on when the engine itself is being developed, 
allows certain logging and debugging functionality.

By default this is on.

### SKL_ENABLE_EDITOR
When turned on, enables game editing features to be run.

By default this is on.

### SKL_ENABLE_LOGGING
When turned on, allows for console logging code to be run.

It will be default turned on when SKL_Internal is on.

Can only be turned on if SKL_INTERNAL is on.

### SKL_SLOW
When turned on enables slower code that either avoids or breaks on certain issues.

It will be default turned on when SKL_Internal is on.

Can only be turned on if SKL_INTERNAL is on.

# Building and running the Project

## Prerequisites

1. Generator: Download Ninja or make
   - We know Ninja works.
2. C++ Compiler: On windows, use Clang. On Unix, either gcc or Clang works.
3. Installed necessary SDK for rendering backend (See [Render Backend](#Render Backend) for more detail)
4. CMake
5. Install the latest graphic drivers on your system.

Any other dependencies of our project are installed when cmake is run.

## Command Line Example Build Steps

1. Clone the project
2. To build the build system, in the `build` directory:
   `cmake .. -G {Generator} -DCMAKE_C_COMPILER={path/to/c/compiler} -DCMAKE_CXX_COMPILER={path/to/cxx/compiler} -DSKL_RENDER_SYS="Default" -DSKL_ENABLE_EDITOR_MODE=1 -DSKL_ENABLE_LOGGING=1 -DSKL_INTERNAL=1 -DSKL_SLOW=0`
3. To use the build system that was just generated:
   `make` or `ninja`, whichever you put as the generator, you can also add `-j <number of threads>` to make it use multiple threads while compiling
4. To run the game engine, in the `bin` directory: `skyline-engine.exe`

## XCode Example Build Steps

1. Clone the project.
2. To build the build system, in the 'xcodebuild' directory (It could be named really anything xcodebuild just differentiates it from build folder).
3. `cmake -G Xcode -DSKL_RENDER_SYS="Default" -DSKL_ENABLE_EDITOR_MODE=1 -DSKL_ENABLE_LOGGING=1 -DSKL_INTERNAL=1 -DSKL_SLOW=0`.
4. Open up xcodebuild/skyline-engine.xcodeproj while in Xcode.
5. Click auto generate schemes and select 'platform' scheme.
6. Hit run.

# Design Notes

- The reason why `u64` is used for EntityID is to avoid narrowing. We use
  `std::vector::size` for getting unique EntityIDs, which outputs `u64`.



