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
                entryPoint = "VS",
                stage = Wrapper.ShaderStage.VertexShader,
                source = source,
            };

            Wrapper.TargetDesc targetDesc = new Wrapper.TargetDesc()
            {
                language = Wrapper.ShadingLanguage.Glsl,
                version = "450",
            };

            Wrapper.Compile(ref sourceDesc, ref targetDesc, out Wrapper.ResultDesc result);

            if (result.isText)
            {
                string translate = Marshal.PtrToStringAnsi(result.target);

                Console.WriteLine("*************************\n" +
                                  "**  GLSL translation   **\n" +
                                  "*************************\n");
                Console.WriteLine(translate);
            }            

            if (result.hasError)
            {
                string warning = Marshal.PtrToStringAnsi(result.errorWarningMsg);
                Console.WriteLine("*************************\n" +
                                  "**  Error output       **\n" +
                                  "*************************\n");
                Console.WriteLine(warning);
            }

            Console.Read();
        }
    }
}
