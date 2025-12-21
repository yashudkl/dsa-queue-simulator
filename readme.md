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
<br>

## Before (Problem)<br>
* Vehicles arrive randomly from roads A, B, C, and D. <br>
* Traffic lights operate without considering queue size. <br>
* All lanes are treated equally, even if some are heavily congested. <br>
* Priority lane (AL2) is not recognized as special. <br>
* Green light duration is fixed, not based on number of vehicles. <br>
This causes: <br>
   *Long waiting times <br>
   *Unfair vehicle dispatch <br>
   *Traffic congestion at busy roads <br>
   
<img width="595" height="467" alt="{7D39CF37-901A-4B93-AC19-0A2E86005916}" src="https://github.com/user-attachments/assets/34590d67-00e3-447d-93ff-4425ad78d015" />
<br>
Here red AUD at top left shows that lane has crossed 10 or more than 10 vehicles
