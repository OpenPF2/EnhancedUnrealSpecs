// Enhanced Automation Specs for UE, Copyright 2026, Guy Elsmore-Paddock. All Rights Reserved.
// Licensed under the MIT License. See LICENSE.txt.

#pragma once

#include <Misc/AutomationTest.h>
#include <Misc/EngineVersionComparison.h>

// UE 5.5 changed automation flags from an integer bitfield to a scoped enum.
#if UE_VERSION_OLDER_THAN(5, 5, 0)
using FEnhancedAutomationTestFlags = uint32;
inline constexpr FEnhancedAutomationTestFlags EnhancedAutomationApplicationContextMask = EAutomationTestFlags::ApplicationContextMask;
inline constexpr FEnhancedAutomationTestFlags EnhancedAutomationFilterMask = EAutomationTestFlags::FilterMask;
#else
using FEnhancedAutomationTestFlags = EAutomationTestFlags;
inline constexpr FEnhancedAutomationTestFlags EnhancedAutomationApplicationContextMask = EAutomationTestFlags_ApplicationContextMask;
inline constexpr FEnhancedAutomationTestFlags EnhancedAutomationFilterMask = EAutomationTestFlags_FilterMask;
#endif
