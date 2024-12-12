# OVRLipSync Plugin for Unreal Engine

## Overview
This repository contains an updated and enhanced version of the OVRLipSync plugin originally provided by Oculus for use with Unreal Engine. This version has been adapted for Unreal Engine 5.x and includes significant improvements to support smoother animations and compatibility with modern engine features.

## Features
### 1. Dynamic Interpolation of Phoneme Transitions
- Introduced a mechanism for weighted interpolation of phoneme values, smoothing transitions between frames.
- Interpolation strength is dynamically adjusted based on the calculated speech tempo.

### 2. Compatibility Enhancements
- Adapted API calls to align with Unreal Engine 5.x updates, including `FPaths` and asynchronous threading changes.
- Improved support for various audio formats with robust error handling.

### 3. Adjustable Parameters
- Added configurable interpolation frames (`MaxInterpolationFrames`) as an input parameter for better control over animation smoothness.
- Added a flag (`bEnableInterpolation`) to toggle interpolation on or off.

## Modifications
The following changes have been made to the original plugin:

### `CookFrameSequenceAsync.cpp`
- **Function `CookFrameSequence`:**
  - Added parameters `bEnableInterpolation` and `MaxInterpolationFrames`.
  - Adjusted logic to compute smoothed phoneme values using weighted averages of previous frames.
- **Dynamic Frame Adjustment:**
  - Introduced logic to analyze speech tempo and dynamically adjust interpolation strength.
  - Added statistical tracking for average phoneme duration (`AverageVisemeDuration`).

### `AudioConverterLibrary.cpp`
- **New Feature:** Added `ParseWavHeader` to dynamically extract WAV file metadata.
- Improved error handling for malformed or unconventional audio files.
- Defaults to standard audio settings (44.1kHz, stereo, 16-bit) when WAV header parsing fails.

## Instructions for Using the Enhanced OVRLipSync Plugin

### Step 1: Download the Official Plugin
1. Visit the official [Oculus LipSync Plugin for Unreal](https://developers.meta.com/horizon/downloads/package/oculus-lipsync-unreal/) page.
2. Download the latest version of the plugin.
3. Extract the downloaded archive.

### Step 2: Add the Plugin to Your Project
1. Locate the plugin folder in the extracted archive:
   ```
   OVRLipSyncUnrealDemo\Plugins\OVRLipSync
   ```
2. Copy the `OVRLipSync` folder to the `Plugins` directory of your Unreal Engine project:
   ```
   YourProject/Plugins/OVRLipSync
   ```

### Step 3: Replace the `Source` Folder
1. Clone this repository to a temporary location:
   ```bash
   git clone https://github.com/droganaida/OVRLipSync-Enhanced-UE5
   ```
2. Replace the `Source` folder inside your project's `Plugins/OVRLipSync` directory with the `Source` folder from this repository.

### Step 4: Rebuild the Plugin
1. Open your Unreal Engine project.
2. If prompted, click "Yes" to rebuild the plugin.
3. Once the rebuild is complete, the updated plugin will be ready to use.


## Usage
1. Add the `CookFrameSequenceAsync` node in your Blueprints to generate lip-sync sequences.
2. Adjust the `MaxInterpolationFrames` parameter for desired animation smoothness.
3. Use the `bEnableInterpolation` flag to toggle interpolation as needed.

## License
This plugin is licensed under the Oculus Audio SDK License Version 3.3. For more details, visit:
[https://developer.oculus.com/licenses/audio-3.3/](https://developer.oculus.com/licenses/audio-3.3/).

## Authors
**Modified by:** Aida Drogan, [https://silvercord-vr.com/](SilverCord-VR)

### Acknowledgments
- Original plugin by Facebook Technologies, LLC and its affiliates.
- Additional contributions by the Unreal Engine developer community.
