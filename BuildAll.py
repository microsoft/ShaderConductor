#!/usr/bin/env python
#-*- coding: ascii -*-

# ShaderConductor
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

import os, platform, subprocess, sys

def FindProgramFilesFolder():
	env = os.environ
	if "64bit" == platform.architecture()[0]:
		if "ProgramFiles(x86)" in env:
			program_files_folder = env["ProgramFiles(x86)"]
		else:
			program_files_folder = "C:\Program Files (x86)"
	else:
		if "ProgramFiles" in env:
			program_files_folder = env["ProgramFiles"]
		else:
			program_files_folder = "C:\Program Files"
	return program_files_folder

def FindVS2017Folder(program_files_folder):
	try_vswhere_location = program_files_folder + "\\Microsoft Visual Studio\\Installer\\vswhere.exe"
	if os.path.exists(try_vswhere_location):
		vs_location = subprocess.check_output([try_vswhere_location,
			"-latest",
			"-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
			"-property", "installationPath",
			"-version", "[15.0,16.0)",
			"-prerelease"]).decode().split("\r\n")[0]
		try_folder = vs_location + "\\VC\\Auxiliary\\Build\\"
		try_vcvarsall = "VCVARSALL.BAT"
		if os.path.exists(try_folder + try_vcvarsall):
			return try_folder
	else:
		names = ("Preview", "2017")
		skus = ("Community", "Professional", "Enterprise")
		for name in names:
			for sku in skus:
				try_folder = program_files_folder + "\\Microsoft Visual Studio\\%s\\%s\\VC\\Auxiliary\\Build\\" % (name, sku)
				try_vcvarsall = "VCVARSALL.BAT"
				if os.path.exists(try_folder + try_vcvarsall):
					return try_folder
	return ""

if __name__ == "__main__":
	if not os.path.exists("Build"):
		os.mkdir("Build")
	if not os.path.exists("Build/Vs2017X64"):
		os.mkdir("Build/Vs2017X64")
	os.chdir("Build/Vs2017X64")
	curdir = os.path.abspath(os.curdir)
	subprocess.call("cmake -G \"Visual Studio 15\" -T v141,host=x64 -A x64 ../../")
	subprocess.call("%sVCVARSALL.BAT amd64 & cd /d \"%s\" & MSBuild ALL_BUILD.vcxproj /nologo /m:2 /v:m /p:Configuration=Debug,Platform=x64" % (FindVS2017Folder(FindProgramFilesFolder()), curdir))
	
	os.chdir("../..")
