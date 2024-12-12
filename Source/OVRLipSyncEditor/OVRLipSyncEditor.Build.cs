/*******************************************************************************
 * Filename    :   OVRLipSyncEditor.Build.cs
 * Content     :   Unreal build script for OVRLipSync
 * Created     :   Sep 14th, 2018
 * Copyright   :   Copyright Facebook Technologies, LLC and its affiliates.
 *                 All rights reserved.
 *
 * Licensed under the Oculus Audio SDK License Version 3.3 (the "License");
 * you may not use the Oculus Audio SDK except in compliance with the License,
 * which is provided at the time of installation or download, or which
 * otherwise accompanies this software in either electronic or hard copy form.

 * You may obtain a copy of the License at
 *
 * https://developer.oculus.com/licenses/audio-3.3/
 *
 * Unless required by applicable law or agreed to in writing, the Oculus Audio SDK
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.

 * Modified by :   Aida Drogan, SilverCord-VR
 *
 * Modifications:
 * - Adapted the original Oculus OVRLipSync plugin for use with Unreal Engine 5.x.
 * - Introduced dynamic interpolation based on phoneme durations.
 * - Added a mechanism to adjust interpolation frames dynamically based on speech tempo.
 * - Enhanced compatibility with modern Unreal Engine APIs (e.g., FPaths and Async threading).
 ******************************************************************************/

using System.IO;
using UnrealBuildTool;

public class OVRLipSyncEditor : ModuleRules
{
    public OVRLipSyncEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange( new string[] {
          "Core",
          "CoreUObject",
          "Engine",
          "OVRLipSync",
          "Slate",
          "SlateCore",
          "Voice"
        });
    }
}

