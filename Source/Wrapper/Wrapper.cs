using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace CSharpConsole
{
    public class Wrapper
    {     
        [DllImport("ShaderConductorWrapper.dll", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        public static extern void Compile([In] ref SourceDesc source, [In] ref TargetDesc target, out ResultDesc result);      

        public enum ShaderStage
        {
            VertexShader,
            PixelShader,
            GeometryShader,
            HullShader,
            DomainShader,
            ComputeShader,

            NumShaderStages,
        };

        public enum ShadingLanguage
        {
            Dxil = 0,
            SpirV,

            Hlsl,
            Glsl,
            Essl,
            Msl,

            NumShadingLanguages,
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct MacroDefine
        {
            public string name;
            public string value;
        };

        [StructLayout(LayoutKind.Sequential)]
        public struct SourceDesc
        {
            public string source;            
            public string entryPoint;
            public ShaderStage stage;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct TargetDesc
        {
            public ShadingLanguage language;
            public string version;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct ResultDesc
        {
            public bool hasError;
            public IntPtr target;
            public IntPtr errorWarningMsg;            
        }
    }
}
