#!/usr/bin/env python3

import argparse
import subprocess
import os
from time import sleep
import paramiko

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Compile/download firmware to board.')
    parser.add_argument('-b', '--board', help='The board to compile for')
    parser.add_argument('-p', '--port',  help='The port the board is on')
    parser.add_argument('-d', '--dfu', action="store_true", help='Use instead of -p if the board is in DFU mode')
    parser.add_argument('-R', '--remote', help='The hostname for the port (e.g. loop name)')
    parser.add_argument('-P', '--password', help='The password for the host')
    parser.add_argument('-j', '--threads', type=int, help='Number of threads to use for make')
    parser.add_argument('-r', '--reset', action="store_true", help='Reset a board in DFU mode back to normal mode')
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
            command = "make"
            if args.threads:
                command += f" -j{args.threads}"

            subprocess.run(command, shell=True, check=True)

            if args.port or args.dfu:
                if args.remote:
                    # Connect over SSH
                    ssh = paramiko.SSHClient()
                    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
                    ssh.connect(f"{args.remote}.local", username="pi", password=f"{args.password}")
                    ssh.exec_command("tmux kill-server")

                    # Copy file over
                    sftp = ssh.open_sftp()
                    sftp.put(f"build/{args.board}.bin", f"/home/pi/{args.board}.bin")
                    sftp.close()
                    
                    # Reset port
                    if args.port:
                        ssh.exec_command(f"echo $'DFU\n' > {args.port}")
                        sleep(1.0)
                    
                    # Download file
                    stdin, stdout, stderr = ssh.exec_command(f"sudo dfu-util -a 0 -D /home/pi/{args.board}.bin -s 0x08000000:leave")

                    sleep(0.5)

                    # Print status while downloading
                    while not stdout.channel.exit_status_ready() or stdout.channel.recv_ready():
                        if stdout.channel.recv_ready():
                            print(stdout.readline(1), end="")

                    if stderr.channel.recv_ready():
                        for line in iter(stderr.readline, ""):
                            print(line, end = "")

                    stdin.close()
                    stdout.close()
                    stderr.close()

                    ssh.exec_command("systemctl --user restart tmux.service")

                    ssh.close()
                
                else:
                    # Open the port and put the device in DFU mode
                    if args.port:
                        subprocess.run(f"echo 'DFU\n' > {args.port}", shell=True)
                        sleep(1.0)

                    subprocess.run(f"dfu-util -a 0 -D build/{args.board}.bin -s 0x08000000:leave", shell=True)

        except Exception as e:
            print(e)

        finally:
            os.chdir(owd)

    else:
        # Invalid combination of options, print help text
        parser.print_help()
