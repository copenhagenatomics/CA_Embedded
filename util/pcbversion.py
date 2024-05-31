#!/usr/bin/env python3

import argparse
import re
from os.path import exists

def get_line(file_path, line_num):
    with open(file_path, 'r') as file:
        lines = file.readlines()
    return lines[line_num - 1].strip()

def main():
    # Set up argument parsing
    parser = argparse.ArgumentParser(description='Process PCB version information.')
    parser.add_argument('file_path', type=str, help='Path to the file containing PCB versions')
    parser.add_argument('option', choices=['latest', 'breaking'], help='Option to select latest or breaking PCB version')
    parser.add_argument('part', choices=['major', 'minor', 'both'], help='Part of the version to display (major or minor)')
    args = parser.parse_args()

    # Get line 2 and 1 of file and trim the first character
    if exists(args.file_path):
        
        # Different parsing methods in case pcbversion is old pcbversion file, or new pcbversion.h
        with open(args.file_path, 'r') as file1:
            strings = re.findall(r'#define\s+(\w+)[^\S\r\n]+(.*)', file1.read())

        if not strings:
            oldest_pcb = get_line(args.file_path, 2)[1:]
            newest_pcb = get_line(args.file_path, 1)[1:]
        else:
            for define in strings:
                if define[0] == 'PCB_VERSION_H_':
                    continue
                elif define[0] == 'LATEST_MAJOR':
                    newest_pcb_major = define[1]
                elif define[0] == 'LATEST_MINOR':
                    newest_pcb_minor = define[1]
                elif define[0] == 'BREAKING_MAJOR':
                    oldest_pcb_major = define[1]
                elif define[0] == 'BREAKING_MINOR':
                    oldest_pcb_minor = define[1]

            oldest_pcb = f"{oldest_pcb_major}.{oldest_pcb_minor}"
            newest_pcb = f"{newest_pcb_major}.{newest_pcb_minor}"
    else:
        oldest_pcb = "v0.0"
        newest_pcb = "v0.0"

    # Split by '.'
    breaking = oldest_pcb.lstrip('vV').split('.')
    latest   = newest_pcb.lstrip('vV').split('.')

    # Print output based on option and part
    if args.option == "latest":
        if args.part == "major":
            print(latest[0])
        elif args.part == "minor":
            print(latest[1])
        else:
            print(f"{latest[0]}.{latest[1]}")
    else:
        if args.part == "major":
            print(breaking[0])
        elif args.part == "minor":
            print(breaking[1])
        else:
            print(f"{breaking[0]}.{breaking[1]}")

if __name__ == "__main__":
    main()
