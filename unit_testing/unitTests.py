#!/usr/bin/env python3

import argparse
import subprocess
import os

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Run unit tests')
    parser.add_argument('-R', '--regex', help='Only run the unit tests matching this regex')
    parser.add_argument('-v', '--verbose', action="store_true", help='Maximum output')
    args = parser.parse_args()

    dir=f"{args.regex}"
    subprocess.run("cmake -S . -B build", shell=True, cwd=dir)
    subprocess.run("cmake --build build", shell=True, cwd=dir)

    run_str = "cd build && ctest"

    if(args.regex):
        run_str += f" -R {args.regex}"

    if(args.verbose):
        run_str += f" --output-on-failure"

    subprocess.run(run_str, shell=True, cwd=dir)
