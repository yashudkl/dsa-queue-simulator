# DSA Queue Simulator

A practical implementation demonstrating the application of Queue data structure to solve real-world traffic management problems.

##  Overview

This project showcases how the Queue data structure (FIFO - First In First Out) can be effectively applied to simulate and manage traffic flow scenarios, such as vehicle queuing at intersections.


## Raylib for simulation

Instead of using provided SDL2 simulation I am doing everything from scratch with raylib.

### raylib vs SDL2 (Brief Comparison)

SDL2 is a low-level multimedia library that provides window creation, input handling, and basic rendering. It requires more boilerplate code and additional libraries for graphics, audio, and higher-level features.

raylib is a simple, beginner-friendly, and open-source C library designed for learning and rapid development. It provides built-in support for 2D/3D graphics, input handling, and audio, allowing developers to focus on core logic rather than low-level implementation details.

For this project, raylib was chosen because it is more simple, open-source, and helps clearly visualize queue behavior while keeping the data-structure implementation independent of the graphics library.

Reference <br>
Website: https://www.raylib.com <br>
GitHub: https://github.com/raysan5/raylib
