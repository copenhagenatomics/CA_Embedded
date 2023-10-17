#!/usr/bin/env python3

import argparse
import subprocess
import os
import serial as s
from time import sleep

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Download firmware to board.')
    parser.add_argument('-b', '--board', help='The board to download to')
    parser.add_argument('-p', '--port',  help='The port the board is on, if not already in DFU')
    parser.add_argument('-r', '--reset', action="store_true", help='Reset a board in DFU mode back to normal mode')
    parser.add_argument('-m', '--make',  action="store_true", help='Only make, don\'t download')
    args = parser.parse_args()

    if args.reset:
        # Make a dummy upload to force the device to reset
        subprocess.run(f"dfu-util -a 0 -s 0x08000000:leave -U temp.bin", shell=True)
        subprocess.run(f"rm -rf temp.bin", shell=True)

    elif args.board:
        # Change to the correct directory to run make
        owd = os.getcwd()

        try:
            os.chdir(f"STM32/{args.board}")
            
            # If make has an error, raise an exception
            subprocess.run(f"make", check=True)

            if args.make:
                exit(0)

            # Open the port and put the device in DFU mode
            if args.port:
                brd = s.Serial(port=args.port)
                try:
                    while(brd.is_open):
                        brd.write("DFU\n".encode('UTF-8'))
                        sleep(0.1)
                except s.SerialException:
                    pass
                finally:
                    brd.close()

                sleep(0.5)

            subprocess.run(f"dfu-util -a 0 -D build/{args.board}.bin -s 0x08000000:leave", shell=True)

        finally:
            os.chdir(owd)

    else:
        # Invalid combination of options, print help text
        parser.print_help()
