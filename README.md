▗▖  ▗▖▗▄▄▖ ▗▄▄▖  ▗▄▄▖     ▗▄▄▄▖▗▖  ▗▖ ▗▄▄▖▗▄▄▄▖▗▖  ▗▖▗▄▄▄▖
▐▌  ▐▌▐▌ ▐▌▐▌ ▐▌▐▌        ▐▌   ▐▛▚▖▐▌▐▌     █  ▐▛▚▖▐▌▐▌   
▐▌  ▐▌▐▛▀▚▖▐▛▀▘ ▐▌▝▜▌▗▄▄▄▖▐▛▀▀▘▐▌ ▝▜▌▐▌▝▜▌  █  ▐▌ ▝▜▌▐▛▀▀▘
 ▝▚▞▘ ▐▌ ▐▌▐▌   ▝▚▄▞▘     ▐▙▄▄▖▐▌  ▐▌▝▚▄▞▘▗▄█▄▖▐▌  ▐▌▐▙▄▄▖

 
 A small game engine designed to help create games in a similar style to Square Enix's "HD-2D" style.

#Installation:#
Just download the repo and get to work!

 #Usage:#
 The main vulkan rendering code is in Engine/main.cpp but the main way to interact with the engine is through header files in the Game/ directory.

 Game/core/ holds core header files like structs for objects as well as contains libraries like tiny obj loader and stb Image

 Game/project holds user created objects and assets. This is where game development will take place most of the time.
 Inside project/ there are subfolders for gameObjects (which are split into entities for sprites and props for 3d models), scenes, assets and shaders

 Shaders are written in GLSL.
