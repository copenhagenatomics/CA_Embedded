import os
import json
import argparse
from pcbVersion import PCBVersion
import subprocess
from blob_mgmt import get_file_from_blob, upload_file_to_blob

# To use this file correctly, the following environment variables must be set:
#   - MODULE_NAME - project name (e.g. AC, DC)
#   - AZURE_BLOB_QUERYSTRING - Query string to access blob storage
#
# Note: It is possible to substitute the MODULE_NAME for command parameters.
#       call python updatePcbVersionList.py --help for more info.
#
# Running this file as a script will:
#   - Retrieve the pcbversion file from the project directory (this contains the version of the PCB
#     that this firmware was designed for, and the breaking version, e.g. the oldest version of the
#     PCB the firmware supports).
#   - Verify the file is formatted correctly and read the PCB version out
#   - Retrieve the <project>-pcb_versions_list.json from the blob storage
#   - If the PCB version number isn't already in the file, save it into the file
#   - Upload the new .json file to blob storage
#

PCB_VERSIONS_LOCAL_FILENAME = "pcbVersions.json"

# Makes a new PCB version lists file
def makeNewPcbVersionFile():
    data = {
        "pcbVersions": {},
        "latestVersions": {},
        "latestStagingVersions": {},
        "stagingVersions": {}
    }
    data = json.dumps(data, indent=4)
    with open(PCB_VERSIONS_LOCAL_FILENAME, "w") as outfile:
        outfile.write(data)


# Checks if the PCB version is already in the file
def pcbVersionInFile(filename, version):
    data = json.load(open(filename))
    return version.fullVersion in data["pcbVersions"]


# Checks whether the version is in the list. If it is, nothing needs to be done, if it is NOT, then 
# it is added and re-uploaded. Will raise an error if PCB_VERSIONS_LOCAL_FILENAME does not exist
def addPcbVersionIntoFile(pcbVersion):
    data = json.load(open(PCB_VERSIONS_LOCAL_FILENAME))
        
    # The same version doesn't need to be uploaded twice
    if pcbVersionInFile(PCB_VERSIONS_LOCAL_FILENAME, pcbVersion):
        print("Skipping, version already exists")
        return 
              
    data["pcbVersions"][pcbVersion.fullVersion] = {
                "major": pcbVersion.major,
                "minor": pcbVersion.minor,
                "patch": pcbVersion.patch
            }

    data = json.dumps(data, indent=4)
    with open(PCB_VERSIONS_LOCAL_FILENAME, "w") as outfile:
        outfile.write(data)
    

def checkPCBVersionFile(file_path) -> bool:
    try:
        h = subprocess.run(f"./util/pcbversion.py {file_path} latest both", shell=True, encoding='UTF-8', capture_output=True)
        latest_line = h.stdout.strip()
        h = subprocess.run(f"./util/pcbversion.py {file_path} breaking both", shell=True, encoding='UTF-8', capture_output=True)
        breaking_line = h.stdout.strip()

        if isValidVersionFormat(latest_line) and isValidVersionFormat(breaking_line):
            return PCBVersion(fullString=latest_line)
        else:
            return None
    except:
        return None

def isValidVersionFormat(version) -> bool:
    # Check if the version string follows the format v1.0 or v1.0.0
    try:
        # Remove the leading 'v' character and split the version string
        version_parts = version.lstrip("vV").split('.')
        
        if len(version_parts) < 2 or len(version_parts) > 3:
            # Invalid version format if it doesn't have 2 or 3 parts
            return False

        # Check if each part is a valid integer
        for part in version_parts:
            int(part)
        
        return True
    except ValueError:
        # Version string contains non-integer characters
        return False


def getOrMakePcbVersionsList(module):
    res = get_file_from_blob(f"{module}-pcb_versions_list.json", PCB_VERSIONS_LOCAL_FILENAME)
    if res == 22:
        makeNewPcbVersionFile()
    elif res == 0:
        pass
    else:
        raise AssertionError(f"Could not get {module}-pcb_versions_list.json from blob storage")
    

if __name__ == "__main__":
    # Script can be called with args or with environment variables
    parser = argparse.ArgumentParser(description='Verify format of PCB version file, and upload new PCB versions to the pcb_versions_list file on the blob')
    parser.add_argument('pcb_file', help='The pcb version file')
    parser.add_argument('-P', '--pcb_versions', nargs="*", help='Additional pcb versions to add into the file')
    parser.add_argument('-m', '--module', help='The name of the module built (1.1.1)')
    args = parser.parse_args()

    # Convert args to environment variables
    if args.module: os.environ['MODULE_NAME'] = args.module

    # Basic environmental setup check
    assert os.environ.get('MODULE_NAME') is not None, "The MODULE_NAME env param does not exist"
    assert os.environ.get('AZURE_BLOB_QUERYSTRING') is not None, "The AZURE_BLOB_QUERYSTRING env param does not exist"
    
    # Verify the format of PCB version file is correct, and if so, extract the latest PCB version
    pcbVersion = checkPCBVersionFile(args.pcb_file)
    if pcbVersion is None:
        raise AssertionError("The format of the pcbVersion file in the newest release is not correct")

    module = os.environ.get('MODULE_NAME')
    getOrMakePcbVersionsList(module)
    addPcbVersionIntoFile(pcbVersion)

    # Add any other versions
    if args.pcb_versions:
        for version in args.pcb_versions:
            addPcbVersionIntoFile(PCBVersion(fullString=version))

    if upload_file_to_blob(f"{module}-pcb_versions_list.json", PCB_VERSIONS_LOCAL_FILENAME) < 0:
        raise AssertionError("Could not set the pcb on the server correctly")
    
    try:
        subprocess.run(f"rm {PCB_VERSIONS_LOCAL_FILENAME}")
    except:
        pass