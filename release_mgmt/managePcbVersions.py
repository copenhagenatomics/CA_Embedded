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
from blob_mgmt import get_file_from_blob, upload_file_to_blob
from updatePcbVersionList import pcbVersionInFile

PCB_VERSIONS_LOCAL_FILENAME = "pcbVersions.json"

# Either retrieves or creates a pcb versions list file
def getPcbVersionFile(module_name):
    ret_code = get_file_from_blob(f"{module_name}-pcb_versions_list.json", PCB_VERSIONS_LOCAL_FILENAME)
    if ret_code == 0:
        return
    else:
        error_help = f"""Getting file from blob. curl return code: {ret_code}.
Return code 6: Verify you're connected to the internet.
Return code 22: The file \"{module_name}-pcb_versions_list.json\" does not exist on the blob storage. Create it using the updatePcbVersion.py script first"""
        raise Exception(error_help)


# Gets all the PCB versions from the breaking version to the latest version, from the PCB version 
# list file
def getNecessaryPCBVersions(breaking_pcb_version, current_pcb_version, module_name) -> bool:
    handled_pcb_list = []
    try:
        getPcbVersionFile(module_name)
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
    

# Adds / updates entries in the stagingVersions dictionary to reflect the new staging version. 
# The entry defines the latest release not yet deemed as stable. The staging version is only
# accessible from the dfu-scripts by adding the -staging flag. The move from staging to stable 
# must be done manually at later stage after testing.
# NOTE: If the force_latest flag is set, the release overwrites the stable version directly.
#       Should only be used in admin cases.
def changeLatestStagingVersion(pcb_version, fw_version, release_type="latestStagingVersion"):
    data = json.load(open(PCB_VERSIONS_LOCAL_FILENAME))

    # In case it is an older file which hasn't been updated with the release_type yet...
    if not data.get(release_type):
        data[release_type] = {}
        
    data[release_type][pcb_version] = fw_version
    data = json.dumps(data, indent=4)
    with open(PCB_VERSIONS_LOCAL_FILENAME, "w") as outfile:
        outfile.write(data)


def checkLatestVersion(pcb_version, fw_version, release_type="latestStagingVersion"):
    """ Checks if the firmware version is the latest for the PCB version. """
    data = json.load(open(PCB_VERSIONS_LOCAL_FILENAME))
    ret = True

    # In case it is an older file which hasn't been updated with the release_type yet...
    if data.get(release_type):
        if data[release_type].get(pcb_version):
            curr_latest = PCBVersion(fullString=data[release_type][pcb_version])
            next_latest = PCBVersion(fullString=fw_version)
            if next_latest.compare(curr_latest) >= 0:
                return True
            else:
                return False

    return ret

def updateStagingVersionsList(pcb_version, fw_version):
    """ Updates the stagingVersions list with the new firmware version for the PCB version."""
    data = json.load(open(PCB_VERSIONS_LOCAL_FILENAME))
    
    if not data.get("stagingVersions"):
        data["stagingVersions"] = {}

    if not data["stagingVersions"].get(pcb_version):
        data["stagingVersions"][pcb_version] = []

    data["stagingVersions"][pcb_version].append(fw_version)
    data = json.dumps(data, indent=4)
    with open(PCB_VERSIONS_LOCAL_FILENAME, "w") as outfile:
        outfile.write(data)


def main(args):
    fw_version = args.fw_version.lstrip("vV")
    module_name = args.module
    current_pcb_version = PCBVersion(fullString=args.current)
    breaking_pcb_version = PCBVersion(fullString=args.breaking)

    # It is expected that both the current and breaking pcb versions are in the pcb_versions_list 
    # file. This should be checked. The "updatePcbVersion" script should always be called before 
    # this one, so the current version should already exist. Its only the breaking version that 
    # might not have been entered into the file before
    getPcbVersionFile(module_name)
    if not pcbVersionInFile(PCB_VERSIONS_LOCAL_FILENAME, current_pcb_version):
        raise Exception("Current PCB version not found in pcb_versions_list.json")
    if not pcbVersionInFile(PCB_VERSIONS_LOCAL_FILENAME, breaking_pcb_version):
        raise Exception("Breaking PCB version not found in pcb_versions_list.json")
    
    # If the force flag is on change standard parameters such that the "latest" version is updated
    release_type = "staging"
    release_entry = "latestStagingVersion"
    if args.force_latest:
        release_type = "latest"
        release_entry = "latestVersions"
    
    # Versions of the PCB that exist between the breaking version and the current version 
    # (inclusive) need to be updated with the firmware
    handled_pcb_list = getNecessaryPCBVersions(breaking_pcb_version, current_pcb_version, module_name)
    
    # Overwrite the old "latest" file with a new one (so the "latest" is always the latest) and
    # make a new version numbered file too.
    if handled_pcb_list:
        for pcbVersion in handled_pcb_list:
            upload_file_to_blob(f"{module_name}-{pcbVersion.fullVersion}-{fw_version}", f"{module_name}.zip")
            if checkLatestVersion(current_pcb_version.fullString, fw_version, release_entry) or args.force_latest:
                changeLatestStagingVersion(pcbVersion.fullVersion, fw_version, release_entry)
                upload_file_to_blob(f"{module_name}-{pcbVersion.fullVersion}-{release_type}", f"{module_name}.zip")
                if release_entry == "latestStagingVersion":
                    updateStagingVersionsList(pcbVersion.fullVersion, fw_version)
        
    upload_file_to_blob(f"{module_name}-pcb_versions_list.json", PCB_VERSIONS_LOCAL_FILENAME)
    
    try:
        os.remove(PCB_VERSIONS_LOCAL_FILENAME)
    except OSError:
        pass
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
    parser.add_argument('-f', '--force_latest', action="store_true",
                        help='Forces updating the "latest" on the blob to this version')

    args = parser.parse_args()
    assert os.environ.get('AZURE_BLOB_QUERYSTRING') is not None, "The AZURE_BLOB_QUERYSTRING env param does not exist"

    main(args)
