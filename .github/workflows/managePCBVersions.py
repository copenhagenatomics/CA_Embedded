# To use this code:

# Set the necessary environment variables:
# AZURE_BLOB_QUERYSTRING: The query string for Azure Blob Storage authentication.
# Run the script with the following command-line arguments:
# -c or --current: The PCB version that the current firmware was built for (e.g., "1.1.1").
# -b or --breaking: The earliest PCB version that the current firmware is compatible with (e.g., "1.1.1").
# -fw or --fw_version: The firmware version that was built (e.g., "1.1.1").
# -m or --module: The name of the module that was built (e.g., "module_name").
# The script will perform the necessary tasks based on the provided arguments and the defined functions.
# It will download the PCB version file from Azure Blob Storage, compare the versions, update the PCB versions file, and upload files to Azure Blob Storage.


import json
import os
import subprocess
import logging as log
import argparse
import re

PCB_VERSIONS_LOCAL_FILENAME = ".tmp/pcbVersions.json"


class PCBVersion:
    major: int = 0
    minor: int = 0
    patch: int = 0
    fullVersion: str = None
    fullString: str = None

    def __init__(self, ctx=None, fullString: str = None) -> None:
        if fullString is not None:
            self.fullString = fullString
            splittedStr = fullString.split('.')
            self.major = int(splittedStr[0].replace('v', ''))
            self.minor = int(splittedStr[1]) if len(splittedStr) > 1 else 0
            self.patch = int(splittedStr[2]) if len(splittedStr) > 2 else 0
            self.fullVersion = re.sub('[a-zA-Z]', '', fullString)
        else:
            self.major = ctx["major"]
            self.minor = ctx["minor"]
            self.patch = ctx["patch"]
            self.fullVersion = f"{ctx['major']}.{ctx['minor']}.{ctx['patch']}"
            self.fullString = f"{ctx['major']}.{ctx['minor']}.{ctx['patch']}"


def versionCompare(a: PCBVersion, b: PCBVersion):
    if a.major == b.major:
        if a.minor == b.minor:
            if a.patch == b.patch:
                return 0
            else:
                return a.patch - b.patch
        else:
            return a.minor - b.minor
    else:
        return a.major - b.major


fw_version = None
breaking_pcb_version = None
current_pcb_version = None
module_name = None
handled_pcb_list = []


def console(args):
    return subprocess.run(args, check=True, text=True, shell=True)


def getPcbVersionFile() -> bool:
    try:
        os.mkdir(".tmp")
        output = console(['curl -f -X GET -H "x-ms-version: 2020-04-08" -H "Content-Type: application/octet-stream" -H "x-ms-blob-type: BlockBlob" "https://carelease.blob.core.windows.net/temptest/{module}-pcb_versions_list.json{key}" --output .tmp/pcbVersions.json'.format(
            key=os.environ.get('AZURE_BLOB_QUERYSTRING'), module=module_name)])
        return True
    except Exception as ex:
        print(ex)
        data = {
            "pcbVersions": {
                f"{breaking_pcb_version.fullVersion}": {
                    "major": breaking_pcb_version.major,
                    "minor": breaking_pcb_version.minor,
                    "patch": breaking_pcb_version.patch
                }
            }
        }
        data = json.dumps(data, indent=4)
        with open(PCB_VERSIONS_LOCAL_FILENAME, "w") as outfile:
            outfile.write(data)
        return False


def getNecessaryPCBVersions() -> bool:
    global handled_pcb_list
    try:
        res = getPcbVersionFile()
        data = json.load(open(PCB_VERSIONS_LOCAL_FILENAME))
        data = data["pcbVersions"]
        for pcbVersion in data:
            tempPcbVersion = PCBVersion(fullString=pcbVersion)
            if versionCompare(tempPcbVersion, breaking_pcb_version) >= 0 and versionCompare(tempPcbVersion, current_pcb_version) <= 0:
                # Selected PCB version should be updated with the current FW release
                handled_pcb_list.append(tempPcbVersion)
        return res
    except Exception as e:
        print(e)
        return res


def uploadFileToBlob(remoteFileName, localFileName):
    try:
        output = console(['curl -f -X PUT -H "x-ms-version: 2020-04-08" -H "Content-Type: application/octet-stream" -H "x-ms-blob-type: BlockBlob" "https://carelease.blob.core.windows.net/temptest/{fw_pcb_file_name}{key}" --upload-file {fw_file_name}'.format(
            key=os.environ.get('AZURE_BLOB_QUERYSTRING'),
            fw_pcb_file_name=remoteFileName,
            fw_file_name=localFileName
        )])
    except Exception as e:
        log.error(e)


def updatePcbVersionsFile(newPcbVersion: PCBVersion):
    data = json.load(open(PCB_VERSIONS_LOCAL_FILENAME))
    data["pcbVersions"][newPcbVersion.fullVersion] = {
        "major": newPcbVersion.major,
        "minor": newPcbVersion.minor,
        "patch": newPcbVersion.patch
    }
    data = json.dumps(data, indent=4)

    with open(PCB_VERSIONS_LOCAL_FILENAME, "w") as outfile:
        outfile.write(data)

    uploadFileToBlob(
        remoteFileName=f"{module_name}-pcb_versions_list.json", localFileName=PCB_VERSIONS_LOCAL_FILENAME)


def main():
    global current_pcb_version, breaking_pcb_version
    current_pcb_version = PCBVersion(fullString=current_pcb_version)
    breaking_pcb_version = PCBVersion(fullString=breaking_pcb_version)
    uploadFileToBlob(
        f"{module_name}-{breaking_pcb_version.fullVersion}-{fw_version}", f"{module_name}.zip")
    if versionCompare(current_pcb_version, breaking_pcb_version) == 0:
        # It is a breaking version
        updatePcbVersionsFile(breaking_pcb_version)
        uploadFileToBlob(
            f"{module_name}-{breaking_pcb_version.fullVersion}-latest", f"{module_name}.zip")
    else:
        # Not a breaking version
        if not getNecessaryPCBVersions():
            updatePcbVersionsFile(breaking_pcb_version)
        uploadFileToBlob(
            f"{module_name}-{breaking_pcb_version.fullVersion}-latest", f"{module_name}.zip")
        for pcbVersion in handled_pcb_list:
            uploadFileToBlob(
                f"{module_name}-{pcbVersion.fullVersion}-latest", f"{module_name}.zip")
    try:
        console(["rm -rf .tmp"])
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

    fw_version = args.fw_version
    breaking_pcb_version = args.breaking
    current_pcb_version = args.current
    module_name = args.module

    main()
