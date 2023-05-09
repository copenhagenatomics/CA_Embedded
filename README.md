# CA_Embedded

 This repository contains the open-source embedded software for microcontrollers in Copenhagen Atomics. More info on setting up your workspace correctly and working with the MCU boards can be found in the [wiki](https://github.com/copenhagenatomics/CA_Embedded/wiki/home).

# Cloning this repository
This repository links to a common embedded library repository `CA_Embedded_Libraries`. This link is there because the library functions are identical for the various repositories containing source-code for Copenhagen Atomics embedded platforms.

The link is done via a git submodule. This makes cloning the project to a local repository slightly different than cloning a standard git repo. The steps are:

1. `git clone https://github.com/copenhagenatomics/CA_AMB.git`
2. Enter your local directory (named 'CA_AMB' if not specified otherwise)
3. `git submodule init`
4. `git submodule update`

The first step clones the main project and will also include the submodule folder with the CA_Embedded_Libraries name, but will be empty. The second command initializes your local configuration file with information about the submodule. The final command fetches all the files found in the submodule. 

# Updating submodules
In the remote repository in GitHub you can see a commit tag after the submodule name e.g. 'CA_Embedded_Libraries @ 4d66627'. The tag refers to a specific commit on the CA_Embedded_Libraries repository. The submodule is always pointing to a specific commit of the submodule. This allows higher control since it is possible to always point to a stable commit. However, this also means the submodule is not automatically updated even if changes were made to it. 

To fetch the latest version of the submodule run

* `git submodule update --remote`

Any changed files in the submodule will be visible as when running 'git status'. Pushing these changes will change the commit number (@ xxxxxx) in GitHub. 

