#
# Description: This script is used to move a staging release to stable in the blob storage.
#
# To use this code:
#
# Set the necessary environment variables:
# AZURE_BLOB_QUERYSTRING: The query string for Azure Blob Storage authentication.
# Run the script with the following command-line arguments:
# -c or --current: The target PCB version for which a move from staging to stable is meant for (e.g., "1.1.1").
# -b or --breaking: The earliest PCB version that the current firmware is compatible with (e.g., "1.1.1").
# -m or --module: The name of the module that was built (e.g., "module_name").
# -t or --target_staging_release: The target staging release to update to stable (e.g., "1.10.0").

import os
import json
import argparse
from pcbVersion import PCBVersion
from blob_mgmt import get_file_from_blob, upload_file_to_blob
from managePcbVersions import getNecessaryPCBVersions, getPcbVersionFile

PCB_VERSIONS_LOCAL_FILENAME = "pcbVersions.json"
FW_LOCAL_FILENAME = "fwVersions.zip"

def getReleaseVersion(pcb_version, release_type="latestVersions", target_release=None):
    """ Get the release version for the PCB version from the PCB versions list file 
        depending on the release type."""
    
    data = json.load(open(PCB_VERSIONS_LOCAL_FILENAME))
    if release_type == "stagingVersions":
        if target_release in data[release_type][pcb_version.fullVersion]:
            return target_release
        else:
            raise AssertionError(f"Target staging release: {target_release} does not exist for PCB version {pcb_version.fullVersion}")
    else:
        return data[release_type][pcb_version.fullVersion]
    
    
def compareFWVersions(fw_release_1, fw_release_2):
    """ Compare two firmware versions by converting them to tuples
        This method means we do not need to import any libraries, but
        is only suitable for versions that are in the format x.y.z"""
    
    fw_release_1_tuple = tuple(map(int, fw_release_1.split(".")))
    fw_release_2_tuple = tuple(map(int, fw_release_2.split(".")))
    
    if fw_release_1_tuple > fw_release_2_tuple:
        return 1
    elif fw_release_1_tuple < fw_release_2_tuple:
        return -1
    else:
        return 0

def isStagingAheadOfStable(pcb_version, target_release):
    """ Check if the target release of the staging version is ahead 
        of the stable version for the PCB version."""
    
    staging_version = getReleaseVersion(pcb_version, "stagingVersions", target_release)

    try:
        stable_version = getReleaseVersion(pcb_version, "latestVersions")
    except KeyError:
        # If the stable pcb version does not exist, we assume the staging version is ahead
        return True

    if compareFWVersions(staging_version, stable_version) == 1:
        return True
    else:
        return False

def removeTargetVersionFromStaging(pcbversion, target_staging_release):
    """ Ensures that a release is removed from staging when it has been 
        marked as stable for all of the relevant PCB versions."""
    
    data = json.load(open(PCB_VERSIONS_LOCAL_FILENAME))
    mark_for_delete_versions = []
    for version in data["stagingVersions"][pcbversion]:
        if compareFWVersions(target_staging_release, version) >= 0:
            mark_for_delete_versions.append(version)

    for version in mark_for_delete_versions:
        data["stagingVersions"][pcbversion].remove(version)

    data = json.dumps(data, indent=4)
    with open(PCB_VERSIONS_LOCAL_FILENAME, 'w') as f:
        f.write(data)

def updateLatestBuildFiles(module_name, pcbversion, target_staging_release):
    """ Updates the build files for the latest versions that contain the target staging release."""

    get_file_from_blob(f"{module_name}-{pcbversion}-{target_staging_release}", FW_LOCAL_FILENAME)
    upload_file_to_blob(f"{module_name}-{pcbversion}-latest", FW_LOCAL_FILENAME)

def updateStableVersion(pcbversion, target_staging_release):
    """ Updates the stable version for the PCB version in the PCB versions list file."""

    data = json.load(open(PCB_VERSIONS_LOCAL_FILENAME))
    data["latestVersions"][pcbversion] = target_staging_release
    data = json.dumps(data, indent=4)
    with open(PCB_VERSIONS_LOCAL_FILENAME, 'w') as f:
        f.write(data)

def updateVersionsListAndReleases(module_name, current_pcb, breaking_pcb, target_staging_release):
    """ Updates meta data as well as build files in the blob storage for the target release
        of the target module for PCB versions between the current and breaking PCB version."""
    pcb_update_list = getNecessaryPCBVersions(breaking_pcb, current_pcb, module_name)
    if pcb_update_list:
        for pcb_version in pcb_update_list:
            updateStableVersion(pcb_version.fullVersion, target_staging_release)
            updateLatestBuildFiles(module_name, pcb_version.fullVersion, target_staging_release)
            removeTargetVersionFromStaging(pcb_version.fullVersion, target_staging_release)
            print(f"Moved Beta {target_staging_release} to stable for PCB {pcb_version.fullVersion}")

    upload_file_to_blob(f"{module_name}-pcb_versions_list.json", PCB_VERSIONS_LOCAL_FILENAME)

    try:
        try:
            os.remove(PCB_VERSIONS_LOCAL_FILENAME)
        except OSError:
            pass
        
        try:
            os.remove(FW_LOCAL_FILENAME)
        except OSError:
            pass
    except Exception as e:
        print(e)



def main(args):
    # Get PCB Versions list file 
    getPcbVersionFile(args.module)

    current_pcb_version = PCBVersion(fullString=args.current)
    breaking_pcb_version = PCBVersion(fullString=args.breaking)

    # Do not update releases if staging is not ahead of stable
    if not isStagingAheadOfStable(current_pcb_version, args.target_staging_release):
        raise AssertionError(f"Staging version is not ahead of stable version - Update aborted.")

    updateVersionsListAndReleases(args.module, current_pcb_version, breaking_pcb_version, args.target_staging_release)

    

if __name__ == "__main__":
    # Script must be called with the following args
    parser = argparse.ArgumentParser(description='Mark staging release as stable in blob storage')
    parser.add_argument('-c', '--current',
                        required=True, help='The pcb version that the current FW was built for (1.1.1)')
    parser.add_argument('-b', '--breaking',
                        required=True, help='The earliest pcb version that the current FW is compatible with (1.1.1)')
    parser.add_argument('-m', '--module', 
                        required=True, help='Target module for which to update release info')
    parser.add_argument('-t', '--target_staging_release', 
                        required=True, help='Target release to update to stable')
    args = parser.parse_args()

    # Basic environmental setup check
    assert os.environ.get('AZURE_BLOB_QUERYSTRING') is not None, "The AZURE_BLOB_QUERYSTRING env param does not exist"

    main(args)

