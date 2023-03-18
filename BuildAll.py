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

def FindVS2017OrUpFolder(programFilesFolder, vsVersion, vsName):
	tryVswhereLocation = programFilesFolder + "\\Microsoft Visual Studio\\Installer\\vswhere.exe"
	if os.path.exists(tryVswhereLocation):
		vsLocation = subprocess.check_output([tryVswhereLocation,
			"-latest",
			"-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
			"-property", "installationPath",
			"-version", "[%d.0,%d.0)" % (vsVersion, vsVersion + 1),
			"-prerelease"]).decode().split("\r\n")[0]
		tryFolder = vsLocation + "\\VC\\Auxiliary\\Build\\"
		tryVcvarsall = "VCVARSALL.BAT"
		if os.path.exists(tryFolder + tryVcvarsall):
			return tryFolder
	else:
		names = ("Preview", vsName)
		skus = ("Community", "Professional", "Enterprise")
		for name in names:
			for sku in skus:
				tryFolder = programFilesFolder + "\\Microsoft Visual Studio\\%s\\%s\\VC\\Auxiliary\\Build\\" % (name, sku)
				tryVcvarsall = "VCVARSALL.BAT"
				if os.path.exists(tryFolder + tryVcvarsall):
					return tryFolder
	LogError("Could NOT find VS%s.\n" % vsName)
	return ""

def FindVS2022Folder(programFilesFolder):
	return FindVS2017OrUpFolder(programFilesFolder, 17, "2022")

def FindVS2019Folder(programFilesFolder):
	return FindVS2017OrUpFolder(programFilesFolder, 16, "2019")

def FindVS2017Folder(programFilesFolder):
	return FindVS2017OrUpFolder(programFilesFolder, 15, "2017")

def FindVS2015Folder(programFilesFolder):
	env = os.environ
	if "VS140COMNTOOLS" in env:
		return env["VS140COMNTOOLS"] + "..\\..\\VC\\"
	else:
		tryFolder = programFilesFolder + "\\Microsoft Visual Studio 14.0\\VC\\"
		tryVcvarsall = "VCVARSALL.BAT"
		if os.path.exists(tryFolder + tryVcvarsall):
			return tryFolder
		else:
			LogError("Could NOT find VS2015.\n")

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

def Build(hostPlatform, hostArch, buildSys, compiler, arch, configuration, tblgenMode, tblgenPath):
	originalDir = os.path.abspath(os.curdir)

	if not os.path.exists("Build"):
		os.mkdir("Build")

	multiConfig = (buildSys.find("vs") == 0)

	buildDir = "Build/%s-%s-%s-%s" % (buildSys, hostPlatform, compiler, arch)
	if (not multiConfig) or (configuration == "clangformat"):
		buildDir += "-%s" % configuration;
	if not os.path.exists(buildDir):
		os.mkdir(buildDir)
	os.chdir(buildDir)
	buildDir = os.path.abspath(os.curdir)

	tblgenOptions = ""
	if (tblgenPath != None):
		tblgenOptions = " -DCLANG_TABLEGEN=\"%s\" -DLLVM_TABLEGEN=\"%s\"" % tblgenPath

	parallel = multiprocessing.cpu_count()

	batCmd = BatchCommand(hostPlatform)
	if hostPlatform == "win":
		programFilesFolder = FindProgramFilesFolder()
		if (buildSys == "vs2022") or ((buildSys == "ninja") and (compiler == "vc143")):
			vsFolder = FindVS2022Folder(programFilesFolder)
		elif (buildSys == "vs2019") or ((buildSys == "ninja") and (compiler == "vc142")):
			vsFolder = FindVS2019Folder(programFilesFolder)
		elif (buildSys == "vs2017") or ((buildSys == "ninja") and (compiler == "vc141")):
			vsFolder = FindVS2017Folder(programFilesFolder)
		elif (buildSys == "vs2015") or ((buildSys == "ninja") and (compiler == "vc140")):
			vsFolder = FindVS2015Folder(programFilesFolder)
		if "x64" == arch:
			vcOption = "amd64"
			vcArch = "x64"
		elif "x86" == arch:
			vcOption = "x86"
			vcArch = "Win32"
		elif "arm64" == arch:
			vcOption = "amd64_arm64"
			vcArch = "ARM64"
		elif "arm" == arch:
			vcOption = "amd64_arm"
			vcArch = "ARM"
		else:
			LogError("Unsupported architecture.\n")
		vcToolset = ""
		if (buildSys == "vs2022") and (compiler == "vc142"):
			vcOption += " -vcvars_ver=14.2"
			vcToolset = "v142,"
		elif ((buildSys == "vs2022") or (buildSys == "vs2019")) and (compiler == "vc141"):
			vcOption += " -vcvars_ver=14.1"
			vcToolset = "v141,"
		elif ((buildSys == "vs2022") or (buildSys == "vs2019") or (buildSys == "vs2017")) and (compiler == "vc140"):
			vcOption += " -vcvars_ver=14.0"
			vcToolset = "v140,"
		batCmd.AddCommand("@call \"%sVCVARSALL.BAT\" %s" % (vsFolder, vcOption))
		batCmd.AddCommand("@cd /d \"%s\"" % buildDir)
	if (buildSys == "ninja"):
		if hostPlatform == "win":
			batCmd.AddCommand("set CC=cl.exe")
			batCmd.AddCommand("set CXX=cl.exe")
		if (configuration == "clangformat"):
			options = "-DSC_CLANGFORMAT=\"ON\""
		else:
			options = "-DCMAKE_BUILD_TYPE=\"%s\" -DSC_ARCH_NAME=\"%s\" %s" % (configuration, arch, tblgenOptions)
		batCmd.AddCommand("cmake -G Ninja %s ../../" % options)
		if tblgenMode:
			batCmd.AddCommand("ninja clang-tblgen -j%d" % parallel)
			batCmd.AddCommand("ninja llvm-tblgen -j%d" % parallel)
		else:
			batCmd.AddCommand("ninja -j%d" % parallel)
	else:
		if buildSys == "vs2022":
			generator = "\"Visual Studio 17\""
		elif buildSys == "vs2019":
			generator = "\"Visual Studio 16\""
		elif buildSys == "vs2017":
			generator = "\"Visual Studio 15\""
		elif buildSys == "vs2015":
			generator = "\"Visual Studio 14\""
		if (configuration == "clangformat"):
			cmake_options = "-DSC_CLANGFORMAT=\"ON\""
			msbuild_options = ""
		else:
			cmake_options = "-T %shost=x64 -A %s %s" % (vcToolset, vcArch, tblgenOptions)
			msbuild_options = "/m:%d /v:m /p:Configuration=%s,Platform=%s" % (parallel, configuration, vcArch)
		batCmd.AddCommand("cmake -G %s %s ../../" % (generator, cmake_options))
		if tblgenMode:
			batCmd.AddCommand("MSBuild External\\DirectXShaderCompiler\\tools\\clang\\utils\\TableGen\\clang-tblgen.vcxproj /nologo %s" % msbuild_options)
			batCmd.AddCommand("MSBuild External\\DirectXShaderCompiler\\utils\\TableGen\\llvm-tblgen.vcxproj /nologo %s" % msbuild_options)
		else:
			batCmd.AddCommand("MSBuild ALL_BUILD.vcxproj /nologo %s" % msbuild_options)
	if batCmd.Execute() != 0:
		LogError("Build failed.\n")

	os.chdir(originalDir)

	tblGenPath = buildDir + "/External/DirectXShaderCompiler"
	if multiConfig:
		tblGenPath += "/" + configuration
	tblGenPath += "/bin/"
	clangTblgenPath = tblGenPath + "clang-tblgen"
	llvmTblGenPath = tblGenPath + "llvm-tblgen"
	if (hostPlatform == "win"):
		clangTblgenPath += ".exe"
		llvmTblGenPath += ".exe"
	return (clangTblgenPath, llvmTblGenPath)

if __name__ == "__main__":
	hostPlatform = sys.platform
	if 0 == hostPlatform.find("win"):
		hostPlatform = "win"
	elif 0 == hostPlatform.find("linux"):
		hostPlatform = "linux"
	elif 0 == hostPlatform.find("darwin"):
		hostPlatform = "osx"

	hostArch = platform.machine()
	if (hostArch == "AMD64") or (hostArch == "x86_64"):
		hostArch = "x64"
	elif (hostArch == "i386"):
		hostArch = "x86"
	elif (hostArch == "ARM64"):
		hostArch = "arm64"
	else:
		LogError("Unknown host architecture %s.\n" % hostArch)

	argc = len(sys.argv);
	if (argc > 1):
		buildSys = sys.argv[1]
	else:
		if hostPlatform == "win":
			buildSys = "vs2019"
		else:
			buildSys = "ninja"
	if (argc > 2):
		compiler = sys.argv[2]
	else:
		if buildSys == "vs2022":
			compiler = "vc143"
		elif buildSys == "vs2019":
			compiler = "vc142"
		elif buildSys == "vs2017":
			compiler = "vc141"
		elif buildSys == "vs2015":
			compiler = "vc140"
		else:
			compiler = "gcc"
	if (argc > 3):
		arch = sys.argv[3]
	else:
		arch = "x64"
	if (argc > 4):
		configuration = sys.argv[4]
	else:
		configuration = "Release"

	tblgenPath = None
	if (configuration != "clangformat") and (hostArch != arch) and (not ((hostArch == "x64") and (arch == "x86"))):
		# Cross compiling:
		# Generate a project with host architecture, build clang-tblgen and llvm-tblgen, and keep the path of clang-tblgen and llvm-tblgen
		tblgenPath = Build(hostPlatform, hostArch, buildSys, compiler, hostArch, configuration, True, None)

	Build(hostPlatform, hostArch, buildSys, compiler, arch, configuration, False, tblgenPath)
