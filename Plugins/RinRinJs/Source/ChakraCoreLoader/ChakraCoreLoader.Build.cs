// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class ChakraCoreLoader : ModuleRules
{
	public ChakraCoreLoader(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);


		PrivateIncludePaths.AddRange(
			new string[] {
				Path.Combine(ModuleDirectory, "../../ThirdParty/ChakraCore/include")
			}
			);


		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Projects"
				// ... add other public dependencies that you statically link with here ...
			}
			);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				// ... add private dependencies that you statically link with here ...	
			}
			);


		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string ChakraCoreLibPath = Path.Combine(ModuleDirectory, "../../ThirdParty/ChakraCore/lib/Win64/Release", "ChakraCore.lib");
			string ChakraCoreDllPath = Path.Combine(ModuleDirectory, "../../ThirdParty/ChakraCore/bin/Win64/Release", "ChakraCore.dll");
			
			// Link against the import library
			PublicAdditionalLibraries.Add(ChakraCoreLibPath);
			
			// Add as delay load DLL so UE can manage it
			PublicDelayLoadDLLs.Add("ChakraCore.dll");
			
			// Add runtime dependency for packaging
			RuntimeDependencies.Add(ChakraCoreDllPath);
		}
	}
}
