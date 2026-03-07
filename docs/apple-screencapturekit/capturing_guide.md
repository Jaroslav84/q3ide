# Capturing Screen Content in macOS

> **Source:** Apple Developer Documentation (Sample Code / Guide)
> **URL:** https://developer.apple.com/documentation/screencapturekit/capturing-screen-content-in-macos
> **Fetched:** 2026-03-07
> **Description:** Apple's official guide and sample code for adding high-performance screen capture to a Mac app using ScreenCaptureKit. Includes step-by-step instructions, code examples, and best practices.
> **Additional Source:** WWDC22 Session 10155 "Take ScreenCaptureKit to the next level" (https://developer.apple.com/videos/play/wwdc2022/10155/)

---

## Overview

This sample demonstrates how to stream desktop content like displays, apps, and windows by adopting screen capture in your Mac application. The sample explores:

- Creating content filters to capture specific displays, apps, and windows
- Configuring stream output for video and audio
- Retrieving video frames and audio samples
- Updating running streams dynamically

**Associated WWDC Session:** [WWDC24 Session 10088 - Capture HDR content with ScreenCaptureKit](https://developer.apple.com/wwdc24/10088/)

**Download Sample Code:** [CapturingScreenContentInMacOS.zip](https://docs-assets.developer.apple.com/published/9db8b3fae777/CapturingScreenContentInMacOS.zip)

---

## Configuration Requirements

### System Requirements

- **macOS 15** or later (for sample code)
- **Xcode 16** or later

### Setup Steps

1. **Grant Permissions:** On first run, the system prompts you to grant the app Screen Recording permission
2. **Restart Required:** After granting permission, restart the app to enable capture

---

## Step 1: Create a Content Filter

### Retrieve Available Content

Use the `SCShareableContent` class to retrieve shareable content on the device:

```swift
// Retrieve the available screen content to capture.
let availableContent = try await SCShareableContent.excludingDesktopWindows(false,
                                                                            onScreenWindowsOnly: true)
```

Available content types:
- **`SCDisplay`** -- Display devices
- **`SCRunningApplication`** -- Running applications
- **`SCWindow`** -- Onscreen windows

### Filter for a Single Window

Create a content filter to capture only a specific window:

```swift
// Create a content filter that includes a single window.
filter = SCContentFilter(desktopIndependentWindow: window)
```

### Filter for Display with Excluded Apps

Create a filter to capture an entire display while optionally excluding specific applications:

```swift
var excludedApps = [SCRunningApplication]()

// If a user chooses to exclude the app from the stream,
// exclude it by matching its bundle identifier.
if isAppExcluded {
    excludedApps = availableApps.filter { app in
        Bundle.main.bundleIdentifier == app.bundleIdentifier
    }
}

// Create a content filter with excluded apps.
filter = SCContentFilter(display: display,
                         excludingApplications: excludedApps,
                         exceptingWindows: [])
```

---

## Step 2: Create a Stream Configuration

Use `SCStreamConfiguration` to configure the stream's output properties:

```swift
var streamConfig = SCStreamConfiguration()

if let dynamicRangePreset = selectedDynamicRangePreset?.scDynamicRangePreset {
    streamConfig = SCStreamConfiguration(preset: dynamicRangePreset)
}

// Configure audio capture.
streamConfig.capturesAudio = isAudioCaptureEnabled
streamConfig.excludesCurrentProcessAudio = isAppAudioExcluded
streamConfig.captureMicrophone = isMicCaptureEnabled

// Configure the display content width and height.
if captureType == .display, let display = selectedDisplay {
    streamConfig.width = display.width * scaleFactor
    streamConfig.height = display.height * scaleFactor
}

// Configure the window content width and height.
if captureType == .window, let window = selectedWindow {
    streamConfig.width = Int(window.frame.width) * 2
    streamConfig.height = Int(window.frame.height) * 2
}

// Set the capture interval at 60 fps.
streamConfig.minimumFrameInterval = CMTime(value: 1, timescale: 60)

// Increase the depth of the frame queue to ensure high fps at the expense of increasing
// the memory footprint of WindowServer.
streamConfig.queueDepth = 5
```

### Configuration Notes

- **Frame Rate:** Throttled to 60 fps via `minimumFrameInterval`
- **Queue Depth:** Set to 5 frames (default is 3, max is 8)
  - Higher values use more memory but allow processing without stalling the stream
  - Default value (3) should not exceed 8

---

## Step 3: Start the Capture Session

Initialize an `SCStream` with the content filter and configuration, then add stream outputs:

```swift
stream = SCStream(filter: filter, configuration: configuration, delegate: streamOutput)

// Add stream outputs to capture screen content.
try stream?.addStreamOutput(streamOutput, type: .screen, sampleHandlerQueue: videoSampleBufferQueue)
try stream?.addStreamOutput(streamOutput, type: .audio, sampleHandlerQueue: audioSampleBufferQueue)
try stream?.addStreamOutput(streamOutput, type: .microphone, sampleHandlerQueue: micSampleBufferQueue)

stream?.startCapture()
```

### Update Running Streams

After the stream starts, update its configuration and content filter without restarting:

```swift
try await stream?.updateConfiguration(configuration)
try await stream?.updateContentFilter(filter)
```

---

## Step 4: Process the Output

### Handle Sample Buffers

When the stream captures new audio or video sample buffers, it calls the stream output's `stream(_:didOutputSampleBuffer:of:)` method:

```swift
func stream(_ stream: SCStream, didOutputSampleBuffer sampleBuffer: CMSampleBuffer, of outputType: SCStreamOutputType) {

    // Return early if the sample buffer is invalid.
    guard sampleBuffer.isValid else { return }

    // Determine which type of data the sample buffer contains.
    switch outputType {
    case .screen:
        // Process the screen content.
    case .audio:
        // Process the audio content.
    case .microphone:
        // Process the microphone content.
    @unknown default:
        break
    }
}
```

---

## Processing Video Sample Buffers

### Retrieve Frame Metadata

Extract metadata attachments that describe the output video frame:

```swift
// Retrieve the array of metadata attachments from the sample buffer.
guard let attachmentsArray = CMSampleBufferGetSampleAttachmentsArray(sampleBuffer,
                                                                     createIfNecessary: false) as? [[SCStreamFrameInfo: Any]],
      let attachments = attachmentsArray.first else { return nil }
```

### Validate Frame Status

Check the frame status using `SCStreamFrameInfo` dictionary keys:

```swift
// Validate the status of the frame. If it isn't `.complete`, return nil.
guard let statusRawValue = attachments[SCStreamFrameInfo.status] as? Int,
      let status = SCFrameStatus(rawValue: statusRawValue),
      status == .complete else { return nil }
```

### Extract Frame Data

Convert the sample buffer's pixel buffer to an IOSurface and extract frame information:

```swift
// Get the pixel buffer that contains the image data.
guard let pixelBuffer = sampleBuffer.imageBuffer else { return nil }

// Get the backing IOSurface.
guard let surfaceRef = CVPixelBufferGetIOSurface(pixelBuffer)?.takeUnretainedValue() else { return nil }
let surface = unsafeBitCast(surfaceRef, to: IOSurface.self)

// Retrieve the content rectangle, scale, and scale factor.
guard let contentRectDict = attachments[.contentRect],
      let contentRect = CGRect(dictionaryRepresentation: contentRectDict as! CFDictionary),
      let contentScale = attachments[.contentScale] as? CGFloat,
      let scaleFactor = attachments[.scaleFactor] as? CGFloat else { return nil }

// Create a new frame with the relevant data.
let frame = CapturedFrame(surface: surface,
                          contentRect: contentRect,
                          contentScale: contentScale,
                          scaleFactor: scaleFactor)
```

---

## Processing Audio Sample Buffers

### Create Audio Buffer

Convert audio sample buffers to `AVAudioPCMBuffer`:

```swift
private func handleAudio(for buffer: CMSampleBuffer) -> Void? {
    // Create an AVAudioPCMBuffer from an audio sample buffer.
    try? buffer.withAudioBufferList { audioBufferList, blockBuffer in
        guard let description = buffer.formatDescription?.audioStreamBasicDescription,
              let format = AVAudioFormat(standardFormatWithSampleRate: description.mSampleRate,
                                         channels: description.mChannelsPerFrame),
              let samples = AVAudioPCMBuffer(pcmFormat: format,
                                             bufferListNoCopy: audioBufferList.unsafePointer)
        else { return }
        pcmBufferHandler?(samples)
    }
}
```

---

## Advanced Topics (from WWDC22 Session 10155)

### Single Window Capture Behavior

- **Resized:** Output dimension remains fixed; content is hardware-scaled
- **Occluded:** Full window content still captured
- **Off-screen/Moved:** Full content still captured
- **Minimized:** Stream output paused, resumes when window is restored

### Display-Based Content Filters

#### Including Windows
- By default, no windows are captured -- must explicitly add
- New windows created after filter setup are NOT automatically included
- Child/popup windows are NOT included
- Windows moved off-screen are removed from output

#### Including Apps
- All windows and audio from specified apps automatically included
- New windows from included apps are automatically captured
- Child/popup windows are automatically captured

#### Excluding Apps
- Remove screen capture app's own windows (prevent mirror hall effect)
- Audio from excluded apps is removed from output entirely

### Content Display Workflow

1. Crop content from frame using `contentRect`
2. Scale content up by dividing by `contentScale`
3. Check scale factor mismatch between source and target display
4. Apply additional scaling if necessary to match target display density

### Queue Depth Mechanics

- Surface pool contains N surfaces available for rendering
- ScreenCaptureKit renders to the active surface
- Completed surface sent to app for processing
- App holds surface while processing, blocking ScreenCaptureKit from reuse
- Valid range: 3-8 surfaces (default: 3)

**Avoid Delayed Frames:** Process each frame within `minimumFrameInterval`
**Avoid Frame Loss:** Release surfaces within `(minimumFrameInterval * (queueDepth - 1))`

---

## Key APIs Reference

| Class/Structure | Purpose |
|---|---|
| **SCShareableContent** | Represents displayable content available for capture |
| **SCDisplay** | Represents a display device |
| **SCRunningApplication** | Represents a running application |
| **SCWindow** | Represents an onscreen window |
| **SCContentFilter** | Filters the content a stream captures |
| **SCStreamConfiguration** | Provides output configuration for a stream |
| **SCStream** | Represents a stream of shareable content |
| **SCStreamFrameInfo** | Defines metadata keys for stream frames |
| **SCFrameStatus** | Indicates frame status (`.complete` means ready) |

---

## Performance Results (OBS Studio Case Study, WWDC22)

- **Frame Rate:** Improved from ~7 fps to 60 fps (replacing CGWindowListCreateImage)
- **Memory:** Up to 15% less RAM usage vs. Window Capture
- **CPU:** Up to 50% CPU reduction compared to Window Capture

---

## Related Resources

- [ScreenCaptureKit Documentation](https://developer.apple.com/documentation/screencapturekit)
- [ScreenCaptureKit Updates](https://developer.apple.com/documentation/Updates/ScreenCaptureKit)
- [Persistent Content Capture Entitlement](https://developer.apple.com/documentation/BundleResources/Entitlements/com.apple.developer.persistent-content-capture)
- [Meet ScreenCaptureKit - WWDC22](https://developer.apple.com/videos/play/wwdc2022/10156)
- [Take ScreenCaptureKit to the next level - WWDC22](https://developer.apple.com/videos/play/wwdc2022/10155)
- [What's new in ScreenCaptureKit - WWDC23](https://developer.apple.com/videos/play/wwdc2023/10136)
- [Capture HDR content with ScreenCaptureKit - WWDC24](https://developer.apple.com/videos/play/wwdc2024/10088)
