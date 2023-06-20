import os
import subprocess
import json
import re

PCB_VERSIONS_LOCAL_FILENAME = "pcbVersions.json"
pcbVersion = None
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



def console(args):
    return subprocess.run(args, check=True, text=True, shell=True)

def getPcbVersionFile() -> bool:
    try:
        output = console(['curl -f -X GET -H "x-ms-version: 2020-04-08" -H "Content-Type: application/octet-stream" -H "x-ms-blob-type: BlockBlob" "https://carelease.blob.core.windows.net/temptest/{module}-pcb_versions_list.json{key}" --output pcbVersions.json'.format(
            key=os.environ.get('AZURE_BLOB_QUERYSTRING'), module=os.environ.get('MODULE_NAME'))])
        
        data = json.load(open(PCB_VERSIONS_LOCAL_FILENAME))
        
        if pcbVersion.fullVersion in data["pcbVersions"]:
            print("Skipping, version already exists")
            return True
        
        data["pcbVersions"][pcbVersion.fullVersion] = {
                    "major": pcbVersion.major,
                    "minor": pcbVersion.minor,
                    "patch": pcbVersion.patch
                }

        data = json.dumps(data, indent=4)
        with open(PCB_VERSIONS_LOCAL_FILENAME, "w") as outfile:
            outfile.write(data)
    
    except Exception as ex:
        print(ex)
        data = {
            "pcbVersions": {
                f"{pcbVersion.fullVersion}": {
                    "major": pcbVersion.major,
                    "minor": pcbVersion.minor,
                    "patch": pcbVersion.patch
                }
            }
        }
        data = json.dumps(data, indent=4)
        with open(PCB_VERSIONS_LOCAL_FILENAME, "w") as outfile:
            outfile.write(data)
    
    try:
        output = console(['curl -f -X PUT -H "x-ms-version: 2020-04-08" -H "Content-Type: application/octet-stream" -H "x-ms-blob-type: BlockBlob" "https://carelease.blob.core.windows.net/temptest/{remoteFileName}{key}" --upload-file {localFileName}'.format(
            key=os.environ.get('AZURE_BLOB_QUERYSTRING'),
            remoteFileName=f"{os.environ.get('MODULE_NAME')}-pcb_versions_list.json", localFileName=PCB_VERSIONS_LOCAL_FILENAME
        )])
        return True
    except Exception as ex:
        print(ex)
        return False

def check_file_lines(file_path) -> bool:
    global pcbVersion
    with open(file_path, 'r') as file:
        # Read the first and second lines from the file
        lines = file.readlines()
        if len(lines) < 2:
            # Ensure that the file has at least two lines
            return False

        # Check the format of the first line
        first_line = lines[0].strip()
        if not is_valid_version_format(first_line):
            return False
        pcbVersion = PCBVersion(fullString=first_line)

        # Check the format of the second line
        second_line = lines[1].strip()
        if not is_valid_version_format(second_line):
            return False

        # Both lines have valid version formats
        return True

def is_valid_version_format(version) -> bool:
    # Check if the version string follows the format v1.0 or v1.0.0
    try:
        # Remove the leading 'v' character and split the version string
        version_parts = version[1:].split('.')
        
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


def checkPCBVersionFile(filename : str) -> bool:
    return check_file_lines(filename)

def pcbVersionInFile() -> bool:
    return getPcbVersionFile()

def test_pcb_version():
    assert os.environ.get('PCB_VERSION_FILE') is not None, "The pcb version env param does not exist"
    assert os.environ.get('MODULE_NAME') is not None, "The module name env param does not exist"
    assert checkPCBVersionFile(os.environ.get('PCB_VERSION_FILE')) == True , "The format of the pcbVersion file in the newest release is not correct"
    assert pcbVersionInFile(),"Could not set the pcb on the server correctly"
    
    try:
        console([f"rm {PCB_VERSIONS_LOCAL_FILENAME}"])
    except:
        pass
    
if __name__ == "__main__":
    test_pcb_version()