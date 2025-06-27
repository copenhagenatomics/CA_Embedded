"""
Manages uploading and downloading files from the blob storage.
"""
import os
import logging as log

# 3rd party imports
import requests

# Functions to simplify reading/writing to the blob
URL_BASE = "https://carelease.blob.core.windows.net/boards/"

def get_file_from_blob(remote_file_name, local_file_name):
    """
    Downloads a file from the blob storage to the local machine.

    Params:
        remote_file_name: The name of the file on the blobStorage
        local_file_name: The name of the local file to be uploaded.

    Returns:
        0 on success, -1 on failure.
    """
    try:
        key=os.environ.get('AZURE_BLOB_QUERYSTRING')
        r = requests.get(f"{URL_BASE}{remote_file_name}{key}", timeout=5)
        r.raise_for_status()
        with open(local_file_name, 'wb') as f:
            f.write(r.content)
        return 0
    except Exception as ex:
        log.error(ex)
        return -1


def upload_file_to_blob(remote_file_name, local_file_name):
    """
    Uploads a file from the local machine to the blob storage.

    Params:
        remote_file_name: The name of the file on the blobStorage
        local_file_name: The name of the local file to be uploaded.

    Returns:
        0 on success, -1 on failure.
    """
    try:
        key=os.environ.get('AZURE_BLOB_QUERYSTRING')
        r = requests.put(f"{URL_BASE}{remote_file_name}{key}",
                         data=open(local_file_name, 'rb'), timeout=5,
                         headers={
                                "x-ms-version": "2020-04-08",
                                "Content-Type": "application/octet-stream",
                                "x-ms-blob-type": "BlockBlob"
                            })
        r.raise_for_status()
        return 0
    except Exception as e:
        log.error(e)
        return -1
