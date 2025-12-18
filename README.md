Skyline Engine
=============

# Directory Overview

- `build`: Artifacts of the build process. Delete the contents of this folder and add back in the `.gitkeep` in order to build from scratch.
- `cmake`: For CMake add-ons and scripts, used by our build process.
- `data`: The assets for our game, containing models and fonts.
- `include`: Contains headers of external libraries.
- `shaders`: Contains shader code.
  - The root holds `slang` shaders.
  - `webgpu`: Contains WebGPU shaders.
- `src`: Contains the source code for the game engine. See the components overview below for more details.
- `.gitignore`: Files we do not want in our Git repository.
- `CMakeLists.txt`: The configuration file for CMake, the development tool we rely on for building the game engine.
- `README.md`: Provides informative and instructional content related to the game engine.
- `notes.txt`: Notes taken from code walks.



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


# Components Overview

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




# Building and running the Project

## Prerequisites

1. Generator: Download Ninja or make
   - We know Ninja works.
2. C++ Compiler: On windows, use Clang. On Unix, either gcc or Clang works.
3. Vulkan:
   - https://vulkan.lunarg.com/sdk/home
   - Install the latest version. As of writing this, we know that 1.4.328.1 works.

## Steps

1. Clone the project
2. To build the build system, in project root:
   `cmake -B . -G {Generator} -DCMAKE_C_COMPILER={path/to/c/compiler} -DCMAKE_CXX_COMPILER={path/to/cxx/compiler} -DSKL_RENDER_SYS="Vulkan" -DSKL_ENABLE_EDITOR_MODE=1 -DSKL_ENABLE_LOGGING=1 -DSKL_INTERNAL=1`
3. To use the build system that was just generated:
   `make`
4. To run the game engine:
   `skyline-engine.exe`


# Design Notes

- The reason why `u64` is used for EntityID is to avoid narrowing. We use
  `std::vector::size` for getting unique EntityIDs, which outputs `u64`.



