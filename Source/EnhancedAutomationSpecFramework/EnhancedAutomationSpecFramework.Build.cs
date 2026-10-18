// Enhanced Automation Specs for UE, Copyright 2024-2025, Guy Elsmore-Paddock. All Rights Reserved.
//
// This Source Code Form is subject to the terms of the MIT License. If a copy of the license was not distributed with
// this file, you can obtain one at https://github.com/OpenPF2/EnhancedUnrealSpecs/blob/main/LICENSE.txt.
using UnrealBuildTool;

// ReSharper disable once InconsistentNaming
public class EnhancedAutomationSpecFramework : ModuleRules
{
	public EnhancedAutomationSpecFramework(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new[]
			{
				"Core",
				"AutomationController",
			}
		);
	}
}
