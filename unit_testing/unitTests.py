#!/usr/bin/env python

import argparse
import subprocess
import os

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Run unit tests')
    # parser.add_argument('-b', '--board', help='Only run the unit tests for this board')
    args = parser.parse_args()

    subprocess.run("cmake -S . -B build", shell=True)
    subprocess.run("cmake --build build", shell=True)
    subprocess.run("cd build && ctest", shell=True)
