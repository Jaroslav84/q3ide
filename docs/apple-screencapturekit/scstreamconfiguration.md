# SCStreamConfiguration

> **Source:** Apple Developer Documentation
> **URL:** https://developer.apple.com/documentation/screencapturekit/scstreamconfiguration
> **Fetched:** 2026-03-07
> **Description:** Complete documentation for the SCStreamConfiguration class, which provides output configuration for a capture stream including video dimensions, pixel format, color settings, audio, frame rate, and more.

---

## Overview

`SCStreamConfiguration` is a class that provides the output configuration for a stream. Creating an instance of this class provides default configuration, and you only need to configure its properties if you need to customize the output.

**Abstract:** An instance that provides the output configuration for a stream.

## Class Declaration

```swift
class SCStreamConfiguration
```

## Availability

- **macOS**: 12.3+
- **Mac Catalyst**: 18.2+

## Inheritance

- **Inherits From:** `NSObject`
- **Conforms To:** `CVarArg`, `CustomDebugStringConvertible`, `CustomStringConvertible`, `Equatable`, `Hashable`, `NSObjectProtocol`

---

## Properties

### Specifying Dimensions

| Property | Type | Description |
|----------|------|-------------|
| `width` | `Int` | The width of the output. |
| `height` | `Int` | The height of the output. |
| `scalesToFit` | `Bool` | A Boolean value that indicates whether to scale the output to fit the configured width and height. |
| `sourceRect` | `CGRect` | A rectangle that specifies the source area to capture. |
| `destinationRect` | `CGRect` | A rectangle that specifies a destination into which to write the output. |
| `preservesAspectRatio` | `Bool` | A Boolean value that determines if the stream preserves aspect ratio. |

### Configuring Colors

| Property | Type | Description |
|----------|------|-------------|
| `pixelFormat` | `OSType` | A pixel format for sample buffers that a stream outputs. |
| `colorMatrix` | `CFString` | A color matrix to apply to the output surface. |
| `colorSpaceName` | `CFString` | A color space to use for the output buffer. |
| `backgroundColor` | `CGColor` | A background color for the output. |

### Configuring Captured Elements

| Property | Type | Description |
|----------|------|-------------|
| `showsCursor` | `Bool` | A Boolean value that determines whether the cursor is visible in the stream. |
| `shouldBeOpaque` | `Bool` | A Boolean value that indicates if semitransparent content presents as opaque. |
| `capturesShadowsOnly` | `Bool` | A Boolean value that indicates if the stream only captures shadows. |
| `ignoreShadowsDisplay` | `Bool` | A Boolean value that indicates if the stream ignores the capturing of window shadows when streaming in display style. |
| `ignoreShadowsSingleWindow` | `Bool` | A Boolean value that indicates if the stream ignores the capturing of window shadows when streaming in window style. |
| `ignoreGlobalClipDisplay` | `Bool` | A Boolean value that indicates if the stream ignores content clipped past the edge of a display, when streaming in display style. |
| `ignoreGlobalClipSingleWindow` | `Bool` | A Boolean value that indicates if the stream ignores content clipped past the edge of a display, when streaming in window style. |

### Configuring Captured Frames

| Property | Type | Description |
|----------|------|-------------|
| `queueDepth` | `Int` | The maximum number of frames for the queue to store. |
| `minimumFrameInterval` | `CMTime` | The desired minimum time between frame updates, in seconds. |
| `captureResolution` | `SCCaptureResolutionType` | The resolution at which to capture source content. |

### Configuring Audio

| Property | Type | Description |
|----------|------|-------------|
| `capturesAudio` | `Bool` | A Boolean value that indicates whether to capture audio. |
| `sampleRate` | `Int` | The sample rate for audio capture. |
| `channelCount` | `Int` | The number of audio channels to capture. |
| `excludesCurrentProcessAudio` | `Bool` | A Boolean value that indicates whether to exclude audio from your app during capture. |

### Identifying a Stream

| Property | Type | Description |
|----------|------|-------------|
| `streamName` | `String?` | A name that you provide for identifying the stream. |

### Notifying Presenters

| Property | Type | Description |
|----------|------|-------------|
| `presenterOverlayPrivacyAlertSetting` | `SCPresenterOverlayAlertSetting` | A value indicating if alerts appear to presenters while using Presenter Overlay. |

### Additional Instance Properties

| Property | Type | Description |
|----------|------|-------------|
| `captureDynamicRange` | `SCCaptureDynamicRange` | Specifies whether the captured screen output is standard or high dynamic range. |
| `captureMicrophone` | `Bool` | Whether to capture microphone audio. |
| `includeChildWindows` | `Bool` | Whether to include child windows in the capture. |
| `microphoneCaptureDeviceID` | `String?` | Identifier for a specific microphone capture device. |
| `showMouseClicks` | `Bool` | Whether to show mouse click indicators. |

---

## Initializers

### `init(preset:)`

```swift
convenience init(preset: SCStreamConfiguration.Preset)
```

Creates a stream configuration with a preset configuration.

---

## Related Types

### Enumerations

- **`SCStreamConfiguration.Preset`** -- Preset configurations for stream setup
- **`SCCaptureDynamicRange`** -- Specifies whether the captured screen output is standard or high dynamic range
- **`SCCaptureResolutionType`** -- Available resolutions for content capture
- **`SCPresenterOverlayAlertSetting`** -- Configures how to present streaming notifications to a streamer of Presenter Overlay

---

## Common Configuration Examples

### 4K 60FPS Capture for Streaming

```swift
let streamConfiguration = SCStreamConfiguration()

// 4K output size
streamConfiguration.width  = 3840
streamConfiguration.height = 2160

// 60 FPS
streamConfiguration.minimumFrameInterval = CMTime(value: 1, timescale: CMTimeScale(60))

// 420v output pixel format for encoding
streamConfiguration.pixelFormat = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange

// Source rect (optional)
streamConfiguration.sourceRect = CGRectMake(100, 200, 3940, 2360)

// Set background fill color to black
streamConfiguration.backgroundColor = CGColor.black

// Include cursor in capture
streamConfiguration.showsCursor = true

// Valid queue depth is between 3 to 8
streamConfiguration.queueDepth = 5

// Include audio in capture
streamConfiguration.capturesAudio = true
```

### Thumbnail Preview (Low Overhead)

```swift
let streamConfiguration = SCStreamConfiguration()

// Small output for thumbnails
streamConfiguration.width  = 284
streamConfiguration.height = 182

// 5 FPS (sufficient for previews)
streamConfiguration.minimumFrameInterval = CMTime(value: 1, timescale: CMTimeScale(5))

// BGRA pixel format for on-screen display
streamConfiguration.pixelFormat = kCVPixelFormatType_32BGRA

// No audio needed for previews
streamConfiguration.capturesAudio = false

// No cursor in thumbnails
streamConfiguration.showsCursor = false
```

### Dynamic Update (Resolution Change)

```swift
// Update output dimension down to 720p
streamConfiguration.width  = 1280
streamConfiguration.height = 720

// 15FPS
streamConfiguration.minimumFrameInterval = CMTime(value: 1, timescale: CMTimeScale(15))

// Update the configuration on a running stream
try await stream.updateConfiguration(streamConfiguration)
```

---

## Queue Depth and Performance Notes

- Surface pool contains N surfaces available for rendering
- ScreenCaptureKit renders to active surface
- Completed surface sent to app for processing
- App holds surface while processing, blocking ScreenCaptureKit from reuse
- Valid range: 3-8 surfaces (default: 3)

**Performance Rules:**
1. Process each frame within `minimumFrameInterval` to avoid delayed frames
2. Release surfaces within `(minimumFrameInterval * (queueDepth - 1))` to avoid frame loss

**Trade-offs:**
- More surfaces = Better frame rate potential, but higher memory usage + potential latency increase
- Fewer surfaces = Lower latency, but may cause frame drops if processing is slow

---

## Related Symbols

- **`SCStream`** -- An instance that represents a stream of shareable content
- **`SCContentFilter`** -- An instance that filters the content a stream captures
- **`SCStreamDelegate`** -- A delegate protocol your app implements to respond to stream events
- **`SCScreenshotManager`** -- An instance for the capture of single frames from a stream
- **`SCScreenshotConfiguration`** -- An object that contains screenshot properties
- **`SCScreenshotOutput`** -- An object that contains all images requested by the client
