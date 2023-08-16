# To use this code:

# Set the necessary environment variables:
# AZURE_BLOB_QUERYSTRING: The query string for Azure Blob Storage authentication.
# Run the script with the following command-line arguments:
# -c or --current: The PCB version that the current firmware was built for (e.g., "1.1.1").
# -b or --breaking: The earliest PCB version that the current firmware is compatible with (e.g., "1.1.1").
# -fw or --fw_version: The firmware version that was built (e.g., "1.1.1").
# -m or --module: The name of the module that was built (e.g., "module_name").
# The script will perform the necessary tasks based on the provided arguments and the defined functions.
# It will download the PCB version file from Azure Blob Storage, compare the versions, and upload files to Azure Blob Storage.

import json
import os
import logging as log
import argparse
from pcbVersion import PCBVersion
from simpleConsole import console
from blobMgmt import getFileFromBlob, uploadFileToBlob
from updatePcbVersionList import pcbVersionInFile

PCB_VERSIONS_LOCAL_FILENAME = "pcbVersions.json"

module_name = None

# Either retrieves or creates a pcb versions list file
def getPcbVersionFile():
    ret_code = getFileFromBlob(f"{module_name}-pcb_versions_list.json", PCB_VERSIONS_LOCAL_FILENAME)
    if ret_code == 0:
        return
    else:
        error_help = f"""Getting file from blob. curl return code: {ret_code}.
Return code 6: Verify you're connected to the internet.
Return code 22: The file \"{module_name}-pcb_versions_list.json\" does not exist on the blob storage. Create it using the updatePcbVersion.py script first"""
        raise Exception(error_help)


# Gets all the PCB versions from the breaking version to the latest version, from the PCB version 
# list file
def getNecessaryPCBVersions(breaking_pcb_version, current_pcb_version) -> bool:
    handled_pcb_list = []
    try:
        getPcbVersionFile()
        data = json.load(open(PCB_VERSIONS_LOCAL_FILENAME))
        data = data["pcbVersions"]
        for pcbVersion in data:
            tempPcbVersion = PCBVersion(fullString=pcbVersion)
            if tempPcbVersion.compare(breaking_pcb_version) >= 0 and tempPcbVersion.compare(current_pcb_version) <= 0:
                # Selected PCB version should be updated with the current FW release
                handled_pcb_list.append(tempPcbVersion)
        return handled_pcb_list
    except Exception as e:
        print(e)
        return None
    

# Adds / updates entries in the latestVersions dictionary to reflect the new version
def changeLatestVersion(pcb_version, fw_version):
    data = json.load(open(PCB_VERSIONS_LOCAL_FILENAME))

    # In case its an older file which hasn't been updated with the latest versions yet...
    if not data.get("latestVersions"):
        data["latestVersions"] = {}
        
    data["latestVersions"][pcb_version] = fw_version
    data = json.dumps(data, indent=4)
    with open(PCB_VERSIONS_LOCAL_FILENAME, "w") as outfile:
        outfile.write(data)


def main(args):
    global module_name
    
    fw_version = args.fw_version
    module_name = args.module
    current_pcb_version = PCBVersion(fullString=args.current)
    breaking_pcb_version = PCBVersion(fullString=args.breaking)

    # It is expected that both the current and breaking pcb versions are in the pcb_versions_list 
    # file. This should be checked. The "updatePcbVersion" script should always be called before 
    # this one, so the current version should already exist. Its only the breaking version that 
    # might not have been entered into the file before
    getPcbVersionFile()
    if not pcbVersionInFile(PCB_VERSIONS_LOCAL_FILENAME, current_pcb_version):
        raise Exception("Current PCB version not found in pcb_versions_list.json")
    if not pcbVersionInFile(PCB_VERSIONS_LOCAL_FILENAME, breaking_pcb_version):
        raise Exception("Breaking PCB version not found in pcb_versions_list.json")
    
    if current_pcb_version.compare(breaking_pcb_version) == 0:
        # It is a breaking version. Therefore no other versions need to be updated. Only the current
        # version
        getPcbVersionFile()
        uploadFileToBlob(f"{module_name}-{breaking_pcb_version.fullVersion}-{fw_version}", f"{module_name}.zip")
        uploadFileToBlob(f"{module_name}-{breaking_pcb_version.fullVersion}-latest", f"{module_name}.zip")
        changeLatestVersion(breaking_pcb_version.fullVersion, fw_version)
    else:
        # It is not a breaking version. Therefore, versions of the PCB that exist between the 
        # breaking version and the current version (inclusive) need to be updated with the firmware
        handled_pcb_list = getNecessaryPCBVersions(breaking_pcb_version, current_pcb_version)
        
        # Overwrite the old "latest" file with a new one (so the "latest" is always the latest) and
        # make a new version numbered file too.
        if handled_pcb_list:
            for pcbVersion in handled_pcb_list:
                changeLatestVersion(pcbVersion.fullVersion, fw_version)
                uploadFileToBlob(f"{module_name}-{pcbVersion.fullVersion}-{fw_version}", f"{module_name}.zip")
                uploadFileToBlob(f"{module_name}-{pcbVersion.fullVersion}-latest", f"{module_name}.zip")
        
    uploadFileToBlob(f"{module_name}-pcb_versions_list.json", PCB_VERSIONS_LOCAL_FILENAME)
    
    try:
        console(f"rm -f {PCB_VERSIONS_LOCAL_FILENAME}")
    except Exception as e:
        log.error(e)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description='Manage the PCB version for this build.')
    parser.add_argument('-c', '--current',
                        required=True, help='The pcb version that the current FW was built for (1.1.1)')
    parser.add_argument('-b', '--breaking',
                        required=True, help='The earliest pcb version that the current FW is compatible with (1.1.1)')
    parser.add_argument('-fw', '--fw_version',
                        required=True, help='The firmware version built (1.1.1)')
    parser.add_argument('-m', '--module',
                        required=True, help='The name of the module built (1.1.1)')

    args = parser.parse_args()

    main(args)
