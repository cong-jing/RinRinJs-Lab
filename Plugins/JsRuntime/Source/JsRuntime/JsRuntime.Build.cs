// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class JsRuntime : ModuleRules
{
	public JsRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		bool useV8 = true;  // ¸ÄÎª true ÆôÓÃ V8

		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);


		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);


		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Projects",
                useV8 ? "V8Loader" : "ChakraCoreLoader"
				// ... add other public dependencies that you statically link with here ...
			}
			);
		if (useV8)
		{
            PublicDefinitions.Add("JS_RUNTIME_V8=1");
		}
		else
		{
			PublicDefinitions.Add("JS_RUNTIME_V8=0");
        }

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
	}
}
