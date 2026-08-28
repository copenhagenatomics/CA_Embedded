#!/usr/bin/env python3

import argparse
import subprocess
import os
import shutil
import sys

def build_tests_in_subdirectory(dir, build_type="Release", clean=False):
    """
    Builds the tests in the specified subdirectory using CMake.
    Args:
        dir (str): The directory containing the CMakeLists.txt file.
        build_type: Builds in the specified mode. Default is Release.
        clean: If True, cleans the build directory before building.
    """
    if clean:
        shutil.rmtree(f"{dir}/build", ignore_errors=True)
    subprocess.run(f"cmake -S . -B build -DCMAKE_BUILD_TYPE={build_type}", shell=True, cwd=dir)
    subprocess.run("cmake --build build", shell=True, cwd=dir)

def run_tests_in_subdirectory(dir, regex, verbose, clean):
    build_tests_in_subdirectory(dir, clean=clean)

    run_str = "cd build && ctest"
    run_str += regex
    run_str += verbose

    ret = subprocess.run(run_str, shell=True, cwd=dir)
    return ret.returncode
        

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Run unit tests')
    parser.add_argument('-D', '--dir', help='Run the CMakeLists.txt in the specified directory')
    parser.add_argument('-R', '--regex', help='Only run the unit tests matching this regex')
    parser.add_argument('-d', '--debug', action="store_true", help='Debug config: builds in debug mode, doesn\'t run')
    parser.add_argument('-v', '--verbose', action="store_true", help='Maximum output')
    parser.add_argument('-c', '--clean', action="store_true", help='Clean build directory before building')
    args = parser.parse_args()

    regex=""
    if(args.regex):
        regex += f" -R {args.regex}"

    verbose=""
    if(args.verbose):
        verbose += f" --output-on-failure"

    # Run the tests in the specified directory
    if args.dir:
        if args.debug:
            build_tests_in_subdirectory(args.dir, "Debug", args.clean)
            sys.exit(0)
        else:
            sys.exit(run_tests_in_subdirectory(f"{args.dir}", regex, verbose, args.clean))

    #If the directory is not specified then run all tests from all sub-directories
    returncodes = []
    for f in os.scandir(os.getcwd()):
        if f.is_dir():
            if(args.debug):
                build_tests_in_subdirectory(f.name, "Debug", args.clean)
                returncodes.append(0)
            else:
                returncodes.append(run_tests_in_subdirectory(f.name, regex, verbose, args.clean))
    sys.exit(any(r != 0 for r in returncodes))