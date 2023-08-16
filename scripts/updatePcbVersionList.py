import os
import json
import argparse
from pcbVersion import PCBVersion
from simpleConsole import console

# To use this file correctly, the following environment variables must be set:
#   - PCB_VERSION_FILE - path to project pcbversion file (see below)
#   - MODULE_NAME - project name (e.g. AC, DC)
#   - AZURE_BLOB_QUERYSTRING - Query string to access blob storage
#
# Note: It is possible to substitute the PCB_VERSION_FILE and MODULE_NAME for command parameters.
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

# Gets the latest PCB version list from the blob storage and checks whether the latest version is in
# the list. If it is, nothing needs to be done, if it is NOT, then it is added and re-uploaded
def pcbVersionInFile(pcbVersion) -> bool:
    key=os.environ.get('AZURE_BLOB_QUERYSTRING')
    module=os.environ.get('MODULE_NAME')
    CURL_HEAD = 'curl -f -H "x-ms-version: 2020-04-08" -H "Content-Type: application/octet-stream" -H "x-ms-blob-type: BlockBlob"'
    CURL_URL  = f"https://carelease.blob.core.windows.net/temptest/{module}-pcb_versions_list.json{key}"   

    try:
        console(f'{CURL_HEAD} -X GET \"{CURL_URL}\" --output {PCB_VERSIONS_LOCAL_FILENAME}')
        
        data = json.load(open(PCB_VERSIONS_LOCAL_FILENAME))
        
        # The same version doesn't need to be uploaded twice
        if pcbVersion.fullVersion in data["pcbVersions"]:
            print("Skipping, version already exists")
            return True
        
    except Exception as ex:
        # Assume most common exception type is that the file doesn't already exist on the blob 
        # storage so we need to make a new one
        print(ex)
        data = {
            "pcbVersions": {}
        }
        
    data["pcbVersions"][pcbVersion.fullVersion] = {
                "major": pcbVersion.major,
                "minor": pcbVersion.minor,
                "patch": pcbVersion.patch
            }

    data = json.dumps(data, indent=4)
    with open(PCB_VERSIONS_LOCAL_FILENAME, "w") as outfile:
        outfile.write(data)
    
    try:
        console(f'{CURL_HEAD} -X PUT \"{CURL_URL}\" --upload-file {PCB_VERSIONS_LOCAL_FILENAME}')
        return True
    except Exception as ex:
        print(ex)
        return False

def checkPCBVersionFile(file_path) -> bool:
    with open(file_path, 'r') as file:
        # Read the first and second lines from the file
        lines = file.readlines()
        if len(lines) < 2:
            # Ensure that the file has at least two lines
            return False

        print("two lines")
        # Check the format of the first line
        first_line = lines[0].strip()
        if not is_valid_version_format(first_line):
            return None
        pcbVersion = PCBVersion(fullString=first_line)
        print("valid current")

        # Check the format of the second line
        second_line = lines[1].strip()
        if not is_valid_version_format(second_line):
            return None

        print("valid breaking")

        # Both lines have valid version formats
        return pcbVersion


def is_valid_version_format(version) -> bool:
    # Check if the version string follows the format v1.0 or v1.0.0
    try:
        # Remove the leading 'v' character and split the version string
        version_parts = version.lstrip('v').split('.')
        
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

if __name__ == "__main__":
    # Script can be called with args or with environment variables
    parser = argparse.ArgumentParser(description='Verify format of PCB version file, and upload new PCB versions to the pcb_versions_list file on the blob')
    parser.add_argument('-p', '--pcb_file', help='The pcb version file')
    parser.add_argument('-P', '--pcb_versions', nargs="*", help='Additional pcb versions to add into the file')
    parser.add_argument('-m', '--module', help='The name of the module built (1.1.1)')
    args = parser.parse_args()

    # Convert args to environment variables
    if args.pcb_file: os.environ['PCB_VERSION_FILE'] = args.pcb_file
    if args.module: os.environ['MODULE_NAME'] = args.module

    # Basic environmental setup check
    assert os.environ.get('PCB_VERSION_FILE') is not None, "The PCB_VERSION_FILE env param does not exist"
    assert os.environ.get('MODULE_NAME') is not None, "The MODULE_NAME env param does not exist"
    assert os.environ.get('AZURE_BLOB_QUERYSTRING') is not None, "The AZURE_BLOB_QUERYSTRING env param does not exist"
    
    # Verify the format of PCB version file is correct, and if so, extract the latest PCB version
    pcbVersion = checkPCBVersionFile(os.environ.get('PCB_VERSION_FILE'))
    if pcbVersion is None:
        raise AssertionError("The format of the pcbVersion file in the newest release is not correct")
    
    if not pcbVersionInFile(pcbVersion):
        raise AssertionError("Could not set the pcb on the server correctly")
    
    # Add any other versions
    if args.pcb_versions:
        for version in args.pcb_versions:
            pcbVersionInFile(PCBVersion(fullString=version))
    
    try:
        console(f"rm {PCB_VERSIONS_LOCAL_FILENAME}")
    except:
        pass