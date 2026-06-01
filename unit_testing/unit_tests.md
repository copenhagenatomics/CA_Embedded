# Introduction

The unit tests for CA_Embedded are performed using GoogleTest suite. The tests are written in CPP, compiled using cmake, and run using a python script. As far as possible, the tests are written in a black box fashion. In certain cases, some elements of the source code are accessed to simplify the tests, when true black-box testing would be too cumbersome.

The user guide for google test can be found [here](https://google.github.io/googletest/).

# Setup unit-test environment for Windows
Tested on Windows 11 (should work on Windows 10 but WSL might not be installed by default)

## Installing Windows Subsystem for Linux (WSL)
* Open a terminal
```
wsl --install -d Ubuntu
```
* Choose username and password for Linux system (can be different from Windows)

**Note:**
You might get an error telling you that Virtualization is not enabled, in that case, you need to activate it in the BIOS of the computer.
You can also be asked to restart your computer.

## Installing required packages in WSL
* Once inside the Linux subsystem (you can reopen it from the Start Menu, look for Debian), type the following commands, to update and install the required packages:
```
sudo apt update && sudo apt upgrade
```
```
sudo apt install python3
```
```
sudo apt install cmake
```
```
sudo apt install build-essential checkinstall zlib1g-dev libssl-dev -y
```
```
sudo apt install wget
```
* You can check that the packages are installed with the following commands:

```
python3 --version
```
```
cmake --version
```
```
cc --version
```
```
cpp --version
```

## Preparing VS code
* Go to the project folder (can be located on Windows):
```
cd /mnt/c/Users/...
```
* Install VS code server in Linux
```
code .
```
* Install C/C++ Extension pack and Cmake Tools in VS code
* You should see that you're remotely connecting to WSL on the bottom left

# Linux pre-requisites

* cmake 3.25 or greater
* python 3 or greater

The GoogleTest libraries are automatically downloaded by cmake

# Running Tests

The tests can be run by entering the `unit_testing` sub-folder and running `./unitTests.py>`.

A variety of options exist:

    usage: unitTests.py [-h] [-D DIR] [-R REGEX] [-v]
    
    Run unit tests
    
    options:
      -h, --help            show this help message and exit
      -D DIR, --dir DIR     Run the CMakeLists.txt in the specified directory
      -R REGEX, --regex REGEX
                            Only run the unit tests matching this regex
      -v, --verbose         Maximum output (e.g. outputs reason for failure, in case of failure)

If no arguments are given then all of the unit test files are compiled and run. 

# Writing Tests

At the time of writing this page, there is no procedure for writing unit tests, but there are some general suggestions:

1. Try to create fakes at the HAL level. This maximises coverage for code written at CA.
2. Include the ".c" file of the UUT, and include it last. This has two benefits:
    1. The internals of the module are available to perform grey/white box testing, where appropriate
    2. It is possible to add "fake" headers in front of the UUT include, which prevents "real" headers from being included. This allows unit testing code to be included in the source module replacing real code (where appropriate) without modifying the source code at all. An example is the fake HAL header file. By using the same header guards as the real HAL, the fake HAL prevents it from being included, and therefore allows fake HAL functions that can be manipulated for testing.
