#!/usr/bin/env python
#-*- coding: ascii -*-

# ShaderConductor
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

import multiprocessing, os, platform, subprocess, sys

def LogError(message):
	print("[E] %s" % message)
	sys.stdout.flush()
	if 0 == sys.platform.find("win"):
		pauseCmd = "pause"
	else:
		pauseCmd = "read"
	subprocess.call(pauseCmd, shell = True)
	sys.exit(1)

def LogInfo(message):
	print("[I] %s" % message)
	sys.stdout.flush()

def LogWarning(message):
	print("[W] %s" % message)
	sys.stdout.flush()

def FindProgramFilesFolder():
	env = os.environ
	if "64bit" == platform.architecture()[0]:
		if "ProgramFiles(x86)" in env:
			programFilesFolder = env["ProgramFiles(x86)"]
		else:
			programFilesFolder = "C:\Program Files (x86)"
	else:
		if "ProgramFiles" in env:
			programFilesFolder = env["ProgramFiles"]
		else:
			programFilesFolder = "C:\Program Files"
	return programFilesFolder

def FindVS2017Folder(programFilesFolder):
	tryVswhereLocation = programFilesFolder + "\\Microsoft Visual Studio\\Installer\\vswhere.exe"
	if os.path.exists(tryVswhereLocation):
		vsLocation = subprocess.check_output([tryVswhereLocation,
			"-latest",
			"-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
			"-property", "installationPath",
			"-version", "[15.0,16.0)",
			"-prerelease"]).decode().split("\r\n")[0]
		tryFolder = vsLocation + "\\VC\\Auxiliary\\Build\\"
		tryVcvarsall = "VCVARSALL.BAT"
		if os.path.exists(tryFolder + tryVcvarsall):
			return tryFolder
	else:
		names = ("Preview", "2017")
		skus = ("Community", "Professional", "Enterprise")
		for name in names:
			for sku in skus:
				tryFolder = programFilesFolder + "\\Microsoft Visual Studio\\%s\\%s\\VC\\Auxiliary\\Build\\" % (name, sku)
				tryVcvarsall = "VCVARSALL.BAT"
				if os.path.exists(tryFolder + tryVcvarsall):
					return tryFolder
	return ""

class BatchCommand:
	def __init__(self, hostPlatform):
		self.commands = []
		self.hostPlatform = hostPlatform

	def AddCommand(self, cmd):
		self.commands += [cmd]

	def Execute(self):
		batchFileName = "scBuild."
		if "win" == self.hostPlatform:
			batchFileName += "bat"
		else:
			batchFileName += "sh"
		batchFile = open(batchFileName, "w")
		batchFile.writelines([cmd_line + "\n" for cmd_line in self.commands])
		batchFile.close()
		if "win" == self.hostPlatform:
			retCode = subprocess.call(batchFileName, shell = True)
		else:
			subprocess.call("chmod 777 " + batchFileName, shell = True)
			retCode = subprocess.call("./" + batchFileName, shell = True)
		os.remove(batchFileName)
		return retCode

if __name__ == "__main__":
	originalDir = os.path.abspath(os.curdir)

	if not os.path.exists("Build"):
		os.mkdir("Build")

	hostPlatform = sys.platform
	if 0 == hostPlatform.find("win"):
		hostPlatform = "win"
	elif 0 == hostPlatform.find("linux"):
		hostPlatform = "linux"
	elif 0 == hostPlatform.find("darwin"):
		hostPlatform = "darwin"

	argc = len(sys.argv);
	if (argc > 1):
		buildSys = sys.argv[1]
	else:
		if hostPlatform == "win":
			buildSys = "vs2017"
		else:
			buildSys = "ninja"
	if (argc > 2):
		arch = sys.argv[2]
	else:
		arch = "x64"
	if (argc > 3):
		configuration = sys.argv[3]
	else:
		configuration = "Release"

	multiConfig = (buildSys.find("vs") == 0)

	buildDir = "Build/%s-%s" % (buildSys, arch)
	if not multiConfig:
		buildDir += "-%s" % configuration;
	if not os.path.exists(buildDir):
		os.mkdir(buildDir)
	os.chdir(buildDir)
	buildDir = os.path.abspath(os.curdir)

	parallel = multiprocessing.cpu_count()

	batCmd = BatchCommand(hostPlatform)
	if hostPlatform == "win":
		vs2017Folder = FindVS2017Folder(FindProgramFilesFolder())
		if "x64" == arch:
			vcOption = "amd64"
		elif "x86" == arch:
			vcOption = "x86"
		else:
			LogError("Unsupported architecture.\n")
		batCmd.AddCommand("@call \"%sVCVARSALL.BAT\" %s" % (vs2017Folder, vcOption))
		batCmd.AddCommand("@cd /d \"%s\"" % buildDir)
	if (buildSys == "ninja"):
		if hostPlatform == "win":
			batCmd.AddCommand("set CC=cl.exe")
			batCmd.AddCommand("set CXX=cl.exe")
		batCmd.AddCommand("cmake -G Ninja -DCMAKE_BUILD_TYPE=\"%s\" -DSC_ARCH_NAME=\"%s\" ../../" % (configuration, arch))
		batCmd.AddCommand("ninja -j%d" % parallel)
	else:
		batCmd.AddCommand("cmake -G \"Visual Studio 15\" -T host=x64 -A %s ../../" % arch)
		batCmd.AddCommand("MSBuild ALL_BUILD.vcxproj /nologo /m:%d /v:m /p:Configuration=%s,Platform=%s" % (parallel, configuration, arch))
	if batCmd.Execute() != 0:
		LogError("Build failed.\n")

	os.chdir(originalDir)
