# Image generator and saver using threads
This project explores the use of threading on C++ to generate and save images to disk at a constant rate. The main goal is to generate 50 images per second, for 5 minutes, resulting in a total of 15,000 images.

## Problem Statement
Generate and save images at a rate of 50 images per second for 5 minutes, is not as straightforward as it seems. The challenge lies in keeping a constant rate of image generation while ensuring that the system can handle the load without dropping frames or slowing down. This cannot be done with a single thread, as the image generation and saving process can be time-consuming. A multi-threaded approach is necessary to achieve the desired performance. But still the system can drop frames if the image generation and saving process takes too long.

## Requirements
To run this project, you need to have the following dependencies installed:
- C++ compiler (g++, clang++, etc.)
- CMake (for building the project)
- OpenCV (for image generation and saving)

## Installation
1. Clone the repository:
   ```bash
   git clone https://github.com/sharedUSSRepo/50fps-challenge.git
   ```
2. Create a build and out directory:
   ```bash
   mkdir build && mkdir out && cd build
   ```

3. Build the project using CMake:
   ```bash
    cmake ..
    make -j
    ```
4. Run the executable:
    ```bash
    ./random_image_generator -f 50 -m 1
    ```

## Usage
The program can be run with the following command-line arguments:
- `-f`: The number of images to generate per second (default is 50).
- `-m`: The total time in seconds to run the image generation (default is 300 seconds, which is 5 minutes).
- `-t`: The number of threads to use for image generation (default is 8).

## Authors
- @[AlanSilvaaa](https://github.com/AlanSilvaaa)
- @[Vinbu](https://github.com/Vinbu)


