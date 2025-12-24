// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class RinRinJs : ModuleRules
{
    public RinRinJs(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.AddRange(new string[] { });
        PrivateIncludePaths.AddRange(new string[] { });

        PublicDependencyModuleNames.AddRange(new string[]
        {
                "Core",
                "CoreUObject",
                "Projects",
        });

        // For civetweb
        PublicDefinitions.AddRange(new string[]
        {
            "USE_WEBSOCKET",
            "NO_FILES",
            "NO_CGI",
            "NO_SSL",
        });
        PrivateDependencyModuleNames.AddRange(new string[] { });
        DynamicallyLoadedModuleNames.AddRange(new string[] { });

        AddV8();
        //AddChakraCore();
    }
    private void AddV8()
    {
        PublicDefinitions.Add("RinRinJs_USE_V8=1");
        PrivateIncludePaths.AddRange(new string[] {
            Path.Combine(ModuleDirectory, "Private/ThirdParty/civetweb"),
            Path.Combine(ModuleDirectory, "Private/ThirdParty/civetweb/include"),
            Path.Combine(ModuleDirectory, "../../ThirdParty/v8/include")
        });

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PublicDefinitions.AddRange(new string[]
            {
                "V8_COMPRESS_POINTERS",
                "V8_31BIT_SMIS_ON_64BIT_ARCH",
                "V8_ENABLE_SANDBOX",
                "_ITERATOR_DEBUG_LEVEL=0",
                "V8_TARGET_ARCH_X64=1",
                "V8_TARGET_ARCH_64_BIT=1",
                "V8_HOST_ARCH_X64=1",
                "V8_TARGET_ARCH_IA32=0",
                "V8_TARGET_ARCH_32_BIT=0",
                "V8_TARGET_ARCH_ARM=0",
                "V8_TARGET_ARCH_ARM64=0",
                "V8_TARGET_ARCH_MIPS64=0",
                "V8_TARGET_ARCH_PPC=0",
                "V8_TARGET_ARCH_PPC64=0",
                "V8_TARGET_ARCH_S390=0",
                "V8_TARGET_ARCH_RISCV64=0",
                "V8_TARGET_ARCH_RISCV32=0",
                "V8_TARGET_ARCH_LOONG64=0",
                "V8_HOST_ARCH_IA32=0",
                "V8_HOST_ARCH_ARM=0",
                "V8_HOST_ARCH_ARM64=0",
                "V8_HOST_ARCH_MIPS64=0",
                "V8_HOST_ARCH_RISCV64=0",
                "V8_HOST_ARCH_RISCV32=0",
                "V8_HOST_ARCH_LOONG64=0",
                "V8_OS_WIN=1",
                "V8_OS_DARWIN=0",
                "V8_OS_FUCHSIA=0",
                "V8_HAS_ATTRIBUTE_ALWAYS_INLINE=0",
                "V8_HAS_BUILTIN_ASSUME=0",
                "V8_HAS_BUILTIN_UNREACHABLE=0",
                "V8_HAS_ATTRIBUTE_CONST=0",
                "V8_HAS_ATTRIBUTE_CONSTINIT=0",
                "V8_HAS_ATTRIBUTE_NONNULL=0",
                "V8_HAS_ATTRIBUTE_NOINLINE=0",
                "V8_HAS_ATTRIBUTE_PRESERVE_MOST=0",
                "V8_CC_INTEL=0",
                "V8_HAS_BUILTIN_EXPECT=0",
                "V8_HAS_ATTRIBUTE_WARN_UNUSED_RESULT=0",
                "V8_HAS_ATTRIBUTE_WEAK=0",
                "V8_HAS_CPP_ATTRIBUTE_NODISCARD=0",
                "V8_HAS_CPP_ATTRIBUTE_NO_UNIQUE_ADDRESS=0",
                "USING_V8_SHARED=0",
                "USING_V8_PLATFORM_SHARED=0"
            });

            string V8LibPath = Path.GetFullPath(Path.Combine(ModuleDirectory, "../../ThirdParty/v8/lib/Win64/Release"));
            string V8LibFile = Path.Combine(V8LibPath, "v8_monolith.lib");

            PublicAdditionalLibraries.Add(V8LibFile);
            //PublicSystemLibraryPaths.Add(V8LibPath);
        }
    }

    private void AddChakraCore()
    {
        PublicDefinitions.Add("RinRinJs_USE_V8=0");
        PrivateIncludePaths.AddRange(new string[] {
                Path.Combine(ModuleDirectory, "../../ThirdParty/ChakraCore/include")
        });

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            string ChakraCoreLibPath = Path.Combine(ModuleDirectory, "../../ThirdParty/ChakraCore/lib/Win64/Release", "ChakraCore.lib");
            string ChakraCoreDllPath = Path.Combine(ModuleDirectory, "../../ThirdParty/ChakraCore/bin/Win64/Release", "ChakraCore.dll");

            PublicAdditionalLibraries.Add(ChakraCoreLibPath);
            PublicDelayLoadDLLs.Add("ChakraCore.dll");
            RuntimeDependencies.Add(ChakraCoreDllPath);
        }
    }
}
