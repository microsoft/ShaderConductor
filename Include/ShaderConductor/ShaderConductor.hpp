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

#ifndef SHADER_CONDUCTOR_HPP
#define SHADER_CONDUCTOR_HPP

#pragma once

#include <functional>

#if defined(__clang__)
#define SC_SYMBOL_EXPORT __attribute__((__visibility__("default")))
#define SC_SYMBOL_IMPORT
#elif defined(__GNUC__)
#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
#define SC_SYMBOL_EXPORT __attribute__((__dllexport__))
#define SC_SYMBOL_IMPORT __attribute__((__dllimport__))
#else
#define SC_SYMBOL_EXPORT __attribute__((__visibility__("default")))
#define SC_SYMBOL_IMPORT
#endif
#elif defined(_MSC_VER)
#define SC_SYMBOL_EXPORT __declspec(dllexport)
#define SC_SYMBOL_IMPORT __declspec(dllimport)
#endif

#ifdef SHADER_CONDUCTOR_SOURCE
#define SC_API SC_SYMBOL_EXPORT
#else
#define SC_API SC_SYMBOL_IMPORT
#endif

namespace ShaderConductor
{
    enum class ShaderStage : uint32_t
    {
        VertexShader,
        PixelShader,
        GeometryShader,
        HullShader,
        DomainShader,
        ComputeShader,

        NumShaderStages,
    };

    enum class ShadingLanguage : uint32_t
    {
        Dxil = 0,
        SpirV,

        Hlsl,
        Glsl,
        Essl,
        Msl_macOS,
        Msl_iOS,

        NumShadingLanguages,
    };

    enum class ShaderResourceType : uint32_t
    {
        ConstantBuffer,
        Parameter,
        Texture,
        Sampler,
        ShaderResourceView,
        UnorderedAccessView,

        NumShaderResourceType,
    };

    struct MacroDefine
    {
        const char* name;
        const char* value;
    };

    class SC_API Blob
    {
    public:
        Blob() noexcept;
        Blob(const void* data, uint32_t size);
        Blob(const Blob& other);
        Blob(Blob&& other) noexcept;
        ~Blob() noexcept;

        Blob& operator=(const Blob& other);
        Blob& operator=(Blob&& other) noexcept;

        void Reset();
        void Reset(const void* data, uint32_t size);

        const void* Data() const noexcept;
        uint32_t Size() const noexcept;

    private:
        class BlobImpl;
        BlobImpl* m_impl = nullptr;
    };

    class SC_API Reflection
    {
    public:
        class ReflectionImpl;
        struct VariableDesc;

        class SC_API ConstantBuffer
        {
            friend class ReflectionImpl;

        public:
            ConstantBuffer() noexcept;
            ConstantBuffer(const ConstantBuffer& other);
            ConstantBuffer(ConstantBuffer&& other) noexcept;
            ~ConstantBuffer() noexcept;

            ConstantBuffer& operator=(const ConstantBuffer& other);
            ConstantBuffer& operator=(ConstantBuffer&& other) noexcept;

            bool Valid() const noexcept;

            const char* Name() const noexcept;
            uint32_t Size() const noexcept;

            uint32_t NumVariables() const noexcept;
            const VariableDesc* VariableByIndex(uint32_t index) const noexcept;
            const VariableDesc* VariableByName(const char* name) const noexcept;

        private:
            class ConstantBufferImpl;
            ConstantBufferImpl* m_impl = nullptr;
        };

        class SC_API VariableType
        {
            friend class ConstantBuffer;

        public:
            enum class DataType
            {
                Void,

                Bool,
                Int,
                Uint,
                Float,

                Half,
                Int16,
                Uint16,

                Struct,
            };

        public:
            VariableType() noexcept;
            VariableType(const VariableType& other);
            VariableType(VariableType&& other) noexcept;
            ~VariableType() noexcept;

            VariableType& operator=(const VariableType& other);
            VariableType& operator=(VariableType&& other) noexcept;

            bool Valid() const noexcept;

            const char* Name() const noexcept;
            DataType Type() const noexcept;
            uint32_t Rows() const noexcept;     // Number of rows (for matrices, 1 for other numeric, 0 if not applicable)
            uint32_t Columns() const noexcept;  // Number of columns (for vectors & matrices, 1 for other numeric, 0 if not applicable)
            uint32_t Elements() const noexcept; // Number of elements (0 if not an array)
            uint32_t ElementStride() const noexcept;

            uint32_t NumMembers() const noexcept;
            const VariableDesc* MemberByIndex(uint32_t index) const noexcept;
            const VariableDesc* MemberByName(const char* name) const noexcept;

        private:
            class VariableTypeImpl;
            VariableTypeImpl* m_impl = nullptr;
        };

        struct ResourceDesc
        {
            char name[256];          // Name of the resource
            ShaderResourceType type; // Type of resource (e.g. texture, cbuffer, etc.)
            uint32_t space;
            uint32_t bindPoint; // Starting bind point
            uint32_t bindCount; // Number of contiguous bind points (for arrays)
        };

        struct VariableDesc
        {
            char name[256];
            VariableType type;
            uint32_t offset; // Offset in cbuffer or stuct
            uint32_t size;   // Size of the variable
        };

        enum class ComponentMask : uint8_t
        {
            X = 0x1U,
            Y = 0x2U,
            Z = 0x4U,
            W = 0x8U,
        };

        struct SignatureParameterDesc
        {
            char semantic[256];
            uint32_t semanticIndex;
            uint32_t location;
            VariableType::DataType componentType;
            ComponentMask mask;
        };

        enum class PrimitiveTopology
        {
            Undefined,
            Points,
            Lines,
            LineStrip,
            Triangles,
            TriangleStrip,

            LinesAdj,
            LineStripAdj,
            TrianglesAdj,
            TriangleStripAdj,

            Patches_1_CtrlPoint,
            Patches_2_CtrlPoint,
            Patches_3_CtrlPoint,
            Patches_4_CtrlPoint,
            Patches_5_CtrlPoint,
            Patches_6_CtrlPoint,
            Patches_7_CtrlPoint,
            Patches_8_CtrlPoint,
            Patches_9_CtrlPoint,
            Patches_10_CtrlPoint,
            Patches_11_CtrlPoint,
            Patches_12_CtrlPoint,
            Patches_13_CtrlPoint,
            Patches_14_CtrlPoint,
            Patches_15_CtrlPoint,
            Patches_16_CtrlPoint,
            Patches_17_CtrlPoint,
            Patches_18_CtrlPoint,
            Patches_19_CtrlPoint,
            Patches_20_CtrlPoint,
            Patches_21_CtrlPoint,
            Patches_22_CtrlPoint,
            Patches_23_CtrlPoint,
            Patches_24_CtrlPoint,
            Patches_25_CtrlPoint,
            Patches_26_CtrlPoint,
            Patches_27_CtrlPoint,
            Patches_28_CtrlPoint,
            Patches_29_CtrlPoint,
            Patches_30_CtrlPoint,
            Patches_31_CtrlPoint,
            Patches_32_CtrlPoint,
        };

        enum class TessellatorOutputPrimitive
        {
            Undefined,
            Point,
            Line,
            TriangleCW,
            TriangleCCW,
        };

        enum class TessellatorPartitioning
        {
            Undefined,
            Integer,
            Pow2,
            FractionalOdd,
            FractionalEven,
        };

        enum class TessellatorDomain
        {
            Undefined,
            Line,
            Triangle,
            Quad,
        };

    public:
        Reflection() noexcept;
        Reflection(const Reflection& other);
        Reflection(Reflection&& other) noexcept;
        ~Reflection() noexcept;

        Reflection& operator=(const Reflection& other);
        Reflection& operator=(Reflection&& other) noexcept;

        bool Valid() const noexcept;

        uint32_t NumResources() const noexcept;
        const ResourceDesc* ResourceByIndex(uint32_t index) const noexcept;
        const ResourceDesc* ResourceByName(const char* name) const noexcept;

        uint32_t NumConstantBuffers() const noexcept;
        const ConstantBuffer* ConstantBufferByIndex(uint32_t index) const noexcept;
        const ConstantBuffer* ConstantBufferByName(const char* name) const noexcept;

        uint32_t NumInputParameters() const noexcept;
        const SignatureParameterDesc* InputParameter(uint32_t index) const noexcept;
        uint32_t NumOutputParameters() const noexcept;
        const SignatureParameterDesc* OutputParameter(uint32_t index) const noexcept;

        PrimitiveTopology GSHSInputPrimitive() const noexcept;
        PrimitiveTopology GSOutputTopology() const noexcept;
        uint32_t GSMaxNumOutputVertices() const noexcept;
        uint32_t GSNumInstances() const noexcept;

        TessellatorOutputPrimitive HSOutputPrimitive() const noexcept;
        TessellatorPartitioning HSPartitioning() const noexcept;

        TessellatorDomain HSDSTessellatorDomain() const noexcept;
        uint32_t HSDSNumPatchConstantParameters() const noexcept;
        const SignatureParameterDesc* HSDSPatchConstantParameter(uint32_t index) const noexcept;
        uint32_t HSDSNumConrolPoints() const noexcept;

        uint32_t CSBlockSizeX() const noexcept;
        uint32_t CSBlockSizeY() const noexcept;
        uint32_t CSBlockSizeZ() const noexcept;

    private:
        ReflectionImpl* m_impl = nullptr;
    };

    class SC_API Compiler
    {
    public:
        struct ShaderModel
        {
            uint8_t major_ver : 6;
            uint8_t minor_ver : 2;

            uint32_t FullVersion() const noexcept
            {
                return (major_ver << 2) | minor_ver;
            }

            bool operator<(const ShaderModel& other) const noexcept
            {
                return this->FullVersion() < other.FullVersion();
            }
            bool operator==(const ShaderModel& other) const noexcept
            {
                return this->FullVersion() == other.FullVersion();
            }
            bool operator>(const ShaderModel& other) const noexcept
            {
                return other < *this;
            }
            bool operator<=(const ShaderModel& other) const noexcept
            {
                return (*this < other) || (*this == other);
            }
            bool operator>=(const ShaderModel& other) const noexcept
            {
                return (*this > other) || (*this == other);
            }
        };

        struct SourceDesc
        {
            const char* source;
            const char* fileName;
            const char* entryPoint;
            ShaderStage stage;
            const MacroDefine* defines;
            uint32_t numDefines;
            std::function<Blob(const char* includeName)> loadIncludeCallback;
        };

        struct Options
        {
            bool packMatricesInRowMajor = true;          // Experimental: Decide how a matrix get packed
            bool enable16bitTypes = false;               // Enable 16-bit types, such as half, uint16_t. Requires shader model 6.2+
            bool enableDebugInfo = false;                // Embed debug info into the binary
            bool disableOptimizations = false;           // Force to turn off optimizations. Ignore optimizationLevel below.
            bool inheritCombinedSamplerBindings = false; // If textures and samplers are combined, inherit the binding of the texture

            int optimizationLevel = 3; // 0 to 3, no optimization to most optimization
            ShaderModel shaderModel = {6, 0};

            int shiftAllTexturesBindings = 0;
            int shiftAllSamplersBindings = 0;
            int shiftAllCBuffersBindings = 0;
            int shiftAllUABuffersBindings = 0;

            bool needReflection = false;
        };

        struct TargetDesc
        {
            ShadingLanguage language;
            const char* version;
            bool asModule;
        };

        struct ResultDesc
        {
            Blob target;
            bool isText;

            Blob errorWarningMsg;
            bool hasError;

            Reflection reflection;
        };

        struct DisassembleDesc
        {
            ShadingLanguage language;
            const uint8_t* binary;
            uint32_t binarySize;
        };

        struct ModuleDesc
        {
            const char* name;
            Blob target;
        };

        struct LinkDesc
        {
            const char* entryPoint;
            ShaderStage stage;

            const ModuleDesc** modules;
            uint32_t numModules;
        };

    public:
        static ResultDesc Compile(const SourceDesc& source, const Options& options, const TargetDesc& target);
        static void Compile(const SourceDesc& source, const Options& options, const TargetDesc* targets, uint32_t numTargets,
                            ResultDesc* results);
        static ResultDesc Disassemble(const DisassembleDesc& source);

        // Currently only Dxil on Windows supports linking
        static bool LinkSupport();
        static ResultDesc Link(const LinkDesc& modules, const Options& options, const TargetDesc& target);
    };

    inline Reflection::ComponentMask& operator|=(Reflection::ComponentMask& lhs, Reflection::ComponentMask rhs)
    {
        lhs = static_cast<Reflection::ComponentMask>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
        return lhs;
    }
    inline constexpr Reflection::ComponentMask operator|(Reflection::ComponentMask lhs, Reflection::ComponentMask rhs)
    {
        return static_cast<Reflection::ComponentMask>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
    }

    inline Reflection::ComponentMask& operator&=(Reflection::ComponentMask& lhs, Reflection::ComponentMask rhs)
    {
        lhs = static_cast<Reflection::ComponentMask>(static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs));
        return lhs;
    }
    inline constexpr Reflection::ComponentMask operator&(Reflection::ComponentMask lhs, Reflection::ComponentMask rhs)
    {
        return static_cast<Reflection::ComponentMask>(static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs));
    }

    inline bool HasAllFlags(Reflection::ComponentMask flags, Reflection::ComponentMask contains)
    {
        return (static_cast<uint8_t>(flags) & static_cast<uint8_t>(contains)) == static_cast<uint8_t>(contains);
    }
    inline bool HasAnyFlags(Reflection::ComponentMask flags, Reflection::ComponentMask contains)
    {
        return (static_cast<uint8_t>(flags) & static_cast<uint8_t>(contains)) != 0;
    }
} // namespace ShaderConductor

#endif // SHADER_CONDUCTOR_HPP
