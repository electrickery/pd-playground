#!/bin/python

# Usage: python getObjectsDescriptions.py (pathToHelpPatches) > libraryName-libraryVersion-objects.txt
#
# This script generates an object list based on the 
# help-patches it finds in the directory. It also harvests
# the object description from the META sub-patch and adds it
# to the object line.
# version: 0.1.0
# license: Standard Improved BSD
# fjkraan@xs4all.nl, 2016-07-02

import sys

from os import listdir
from os.path import isfile, join

helpPatchPath = sys.argv[1]


def getObjectsAndDescriptions(helpPatchPath):
    objectDescriptionLines = ""
    minimumColumDistance = 10

    if not helpPatchPath[-1:] == "/":
        helpPatchPath += "/"
    onlyFiles = sorted([f for f in listdir(helpPatchPath) if isfile(join(helpPatchPath, f))])
    for file in onlyFiles:
        if not "-help.pd" in file:
            continue
        objectName = file.split("-help", 1)[0]
        helpPatchFile = helpPatchPath + file
        objectDescription = getDescription(helpPatchFile)
        fillerSpaces = " "*(minimumColumDistance - len(objectName)) + "  "
        objectDescriptionLines += objectName + fillerSpaces + str(objectDescription) + "\n"
    return objectDescriptionLines

def getDescription(helpPatchFile):
    descriptionMatcher = "DESCRIPTION "

    with open(helpPatchFile) as helpPatch:
        for line in helpPatch:
            if not descriptionMatcher in line:
                continue
            description = line.split(descriptionMatcher, 1)[1].rstrip("\r\n;")
            return description


theWholeList = getObjectsAndDescriptions(helpPatchPath)
print theWholeList