# DSA Queue Simulator

A practical implementation demonstrating the application of Queue data structure to solve real-world traffic management problems.

##  Overview

This project showcases how the Queue data structure (FIFO - First In First Out) can be effectively applied to simulate and manage traffic flow scenarios, such as vehicle queuing at intersections.

## Video

[![Traffic Simulation Demo](demo.gif)](https://youtu.be/VIDEO_LINK)

▶ Click to watch full video in youtube

## Raylib for simulation

Instead of using provided SDL2 simulation I am doing everything from scratch with raylib.It is a simple, beginner-friendly, and open-source C library designed for learning and rapid development. It provides built-in support for 2D/3D graphics, input handling, and audio, allowing developers to focus on core logic rather than low-level implementation details. For this project, raylib was chosen because it is more simple, open-source, and helps clearly visualize queue behavior while keeping the data-structure implementation independent of the graphics library.

## Traffic Queue Simulator — Installation & Running Guide

### 🐧 Arch Linux — Build & Run

### 1️⃣ Install dependencies
bash <br>
`sudo pacman -S gcc raylib pkgconf`
### 2️⃣ Compile programs
Traffic generator (console-only) <br>
`gcc -Wall -O2 traffic_generator.c -o traffic_generator.exe`
<br> <br>
Simulator (raylib GUI) <br>
`gcc -Wall -O2 simulator.c -o simulator.exe $(pkg-config --cflags --libs raylib)`

### 3️⃣ Run 
`touch vehicles.data 
./traffic_generator &
./simulator`

### 🪟 Windows — Build & Run (MSYS2 MinGW64)

#### ⚠️ Must be executed inside MSYS2 MinGW64 shell

### 1️⃣ Install MSYS2

`Download from: https://www.msys2.org` <br>

Open MSYS2 MinGW64 from the Start Menu.

### 2️⃣ Install dependencies

`pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-raylib mingw-w64-x86_64-pkg-config`

### 3️⃣ Compile programs
Traffic generator
`gcc traffic_generator.c -o traffic_generator.exe`

Simulator
`gcc -Wall -O2 simulator.c -o simulator.exe $(pkg-config --cflags --libs raylib)`

### 4️⃣ Run
`traffic_generator.exe and
simulator.exe`

## Before (Problem)<br>
* Vehicles arrive randomly from roads A, B, C, and D. <br>
* Traffic lights operate without considering queue size. <br>
* All lanes are treated equally, even if some are heavily congested. <br>
* Priority lane (AL2) is not recognized as special. <br>
* Green light duration is fixed, not based on number of vehicles. <br>
This causes: <br>
   * Long waiting times <br>
   * Unfair vehicle dispatch <br>
   * Traffic congestion at busy roads <br>
  <br>
  <br>


## After (Solution) <br>
* AL2 is served immediately when congested
* Waiting time in AL2 is reduced
* Normal lanes are served fairly
* System performance improves partially

<br>
<br>

## References
### Raylib Documentation
### Official Documentation:

Raylib API Reference: https://www.raylib.com/cheatsheet/cheatsheet.html <br>
Raylib Examples: https://www.raylib.com/examples.html <br>
GitHub Repository: https://github.com/raysan5/raylib <br> 

### Tutorials:

Getting Started Guide: https://github.com/raysan5/raylib/wiki/Working-on-Windows <br>
Raylib Beginner Video Series: https://www.youtube.com/playlist?list=PL5gRzHmN4Dg3ubcneVFkHJnLd-Yj_LPWP <br> 

#### Note: The system improves traffic flow using a single priority lane, but does not fully optimize congestion for all lanes.




