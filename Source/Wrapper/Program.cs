/*
 * ShaderConductor
 *
 * Copyright (c) Microsoft Corporation. All rights reserved.
 * Licensed under the MIT License.
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons
 * to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED *AS IS*, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
 * FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

using System;
using System.IO;
using System.Runtime.InteropServices;

namespace CSharpConsole
{
    class Program
    {
        static void Main(string[] args)
        {
            string source = File.ReadAllText("shader.hlsl");

            Wrapper.SourceDesc sourceDesc = new Wrapper.SourceDesc()
            {
                entryPoint = "PS",

                stage = Wrapper.ShaderStage.PixelShader,
                source = source,
            };

            Wrapper.OptionsDesc optionsDesc = Wrapper.OptionsDesc.Default;
            //optionsDesc.packMatricesInRowMajor = true;
            //optionsDesc.enable16bitTypes = true;
            //optionsDesc.enableDebugInfo = true;
            //optionsDesc.disableOptimizations = true;
            //optionsDesc.optimizationLevel = 3;
            //optionsDesc.shaderModel = new Wrapper.ShaderModel(6, 2);
            //optionsDesc.shiftAllCBuffersBindings = 10;
            //optionsDesc.shiftAllTexturesBindings = 20;
            //optionsDesc.shiftAllSamplersBindings = 30;
            //optionsDesc.shiftAllUABuffersBindings = 40;

            Wrapper.TargetDesc targetDesc = new Wrapper.TargetDesc()
            {
                language = Wrapper.ShadingLanguage.Glsl,
                version = "460",
            };

            Wrapper.Compile(ref sourceDesc, ref optionsDesc, ref targetDesc, out Wrapper.ResultDesc result);

            if (result.hasError)
            {
                string warning = Marshal.PtrToStringAnsi(Wrapper.GetShaderConductorBlobData(result.errorWarningMsg),
                    Wrapper.GetShaderConductorBlobSize(result.errorWarningMsg));
                Console.WriteLine("*************************\n" +
                                  "**  Error output       **\n" +
                                  "*************************\n");
                Console.WriteLine(warning);
            }
            else
            {
                if (!result.isText)
                {
                    Wrapper.DisassembleDesc disassembleDesc = new Wrapper.DisassembleDesc()
                    {
                        language = targetDesc.language,
                        binarySize = Wrapper.GetShaderConductorBlobSize(result.target),
                        binary = Wrapper.GetShaderConductorBlobData(result.target),
                    };

                    Wrapper.Disassemble(ref disassembleDesc, out Wrapper.ResultDesc disassembleResult);
                    Wrapper.DestroyShaderConductorBlob(result.target);
                    Wrapper.DestroyShaderConductorBlob(result.errorWarningMsg);
                    result = disassembleResult;
                }

                if (result.isText)
                {
                    string translation = Marshal.PtrToStringAnsi(Wrapper.GetShaderConductorBlobData(result.target),
                        Wrapper.GetShaderConductorBlobSize(result.target));
                    Console.WriteLine("*************************\n" +
                                      "**  Translation        **\n" +
                                      "*************************\n");
                    Console.WriteLine(translation);
                }
            }            

            Wrapper.DestroyShaderConductorBlob(result.target);
            Wrapper.DestroyShaderConductorBlob(result.errorWarningMsg);

            Console.Read();
        }
    }
}
