#!/usr/bin/env python3

import argparse
import subprocess
import os

def run_tests_in_subdirectory(dir, regex, verbose):
    subprocess.run("cmake -S . -B build", shell=True, cwd=dir)
    subprocess.run("cmake --build build", shell=True, cwd=dir)

    run_str = "cd build && ctest"

    subprocess.run(run_str, shell=True, cwd=dir)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Run unit tests')
    parser.add_argument('-D', '--dir', help='Run the CMakeLists.txt in the specified directory')
    parser.add_argument('-R', '--regex', help='Only run the unit tests matching this regex')
    parser.add_argument('-v', '--verbose', action="store_true", help='Maximum output')
    args = parser.parse_args()

    regex=""
    if(args.regex):
        regex += f" -R {args.regex}"

    verbose=""
    if(args.verbose):
        verbose += f" --output-on-failure"

    # Run the tests in the specified directory
    if (args.dir):
        run_tests_in_subdirectory(f"{args.dir}", regex, verbose)
        sys.exit()

    #If the directory is not specified then run all tests from all sub-directories
    [run_tests_in_subdirectory(f.name, regex, verbose) for f in os.scandir(os.getcwd()) if f.is_dir()]