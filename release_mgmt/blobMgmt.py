from simpleConsole import console
import os
import subprocess
import logging as log

# Functions to simplify reading/writing to the blob
CURL_HEAD = 'curl -f -H "x-ms-version: 2020-04-08" -H "Content-Type: application/octet-stream" -H "x-ms-blob-type: BlockBlob"'
CURL_URL_BASE = "https://carelease.blob.core.windows.net/temptest/"
def getFileFromBlob(remoteFileName, localFileName):
    try:
        key=os.environ.get('AZURE_BLOB_QUERYSTRING')
        console(f'{CURL_HEAD} -X GET \"{CURL_URL_BASE}{remoteFileName}{key}\" --output {localFileName}')
    except subprocess.CalledProcessError as ex:
        log.error(ex)
        return ex.returncode
    except Exception as ex:
        log.error(ex)
        return -1
    
    return 0

def uploadFileToBlob(remoteFileName, localFileName):
    try:
        key=os.environ.get('AZURE_BLOB_QUERYSTRING')
        console(f'{CURL_HEAD} -X PUT \"{CURL_URL_BASE}{remoteFileName}{key}\" --upload-file {localFileName}')
        return True
    except Exception as e:
        log.error(e)
        return False