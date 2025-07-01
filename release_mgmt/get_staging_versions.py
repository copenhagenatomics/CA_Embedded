"""
Script to get versions of any releases on the blob that are in staging.
"""
import argparse
import json
import logging as log
import os
from blob_mgmt import get_file_from_blob

NAMING_MAP_LOCAL_FILENAME = "naming_map_local"
PCB_VERSIONS_LOCAL_FILENAME = "pcb_versions_list"

def get_staging_versions(module):
    """ Iterates the blob storage for the relevant modules and prints the staging versions."""

    modules = []
    if module == 'all':
        if get_file_from_blob('naming_map', NAMING_MAP_LOCAL_FILENAME) == 0:
            with open(NAMING_MAP_LOCAL_FILENAME, "r", encoding="utf-8") as naming_map:
                for line in naming_map:
                    module_name, _ = line.partition(":")[::2]
                    modules.append(module_name)
            try:
                os.remove(NAMING_MAP_LOCAL_FILENAME)
            # OS Error occurs if the file has already been deleted
            except OSError:
                pass
            except Exception as e:
                log.error(e)
        else:
            print("Failed to get the naming map")
            return -1

    else:
        modules.append(module)
    
    for module in modules:
        if get_file_from_blob(f"{module}-pcb_versions_list.json", PCB_VERSIONS_LOCAL_FILENAME) == 0:
            data = json.load(open(PCB_VERSIONS_LOCAL_FILENAME))
            if data.get("stagingVersions") and len(data["stagingVersions"]) != 0:
                first = True
                for pcb_version in data["stagingVersions"]:
                    if len(data["stagingVersions"][pcb_version]) != 0:
                        if first:
                            first = False
                            print(f"Staging versions for {module}:")
                        print(f"  PCB v{pcb_version}:")
                        for fw_version in data["stagingVersions"][pcb_version]:
                            print(f"    FW v{fw_version}")
                # If we have printed at least one staging version, we can continue
                if not first:
                    continue

            # Else
            print(f"No staging versions for {module}")

        else:
            print(f"Failed to get the PCB versions for {module}")

    try:
        os.remove(PCB_VERSIONS_LOCAL_FILENAME)
    # OS Error occurs if the file has already been deleted
    except OSError: 
        pass
    except Exception as e:
        log.error(e)

    return 0


if __name__ == '__main__':
    # Set up logging
    logger = log.getLogger(__name__)
    log.basicConfig(filename="staging_versions.log", encoding="utf-8", level=log.DEBUG)

    parser = argparse.ArgumentParser(description='Get staging releases for a single or all modules')

    # If the module argument is not provided, all modules will be processed
    parser.add_argument('-m', '--module', required=False, 
                        help='The name of the module to get the staging versions for')
    args = parser.parse_args()

    MODULE = 'all'
    if args.module:
        MODULE = args.module

    # Basic environmental setup check
    assert os.environ.get('AZURE_BLOB_QUERYSTRING') is not None, "The AZURE_BLOB_QUERYSTRING env param does not exist"

    exit(get_staging_versions(MODULE))
