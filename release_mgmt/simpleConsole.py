import subprocess

def console(args):
    return subprocess.run(args, check=True, text=True, shell=True)