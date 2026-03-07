# SCStreamOutput

> **Source:** Apple Developer Documentation
> **URL:** https://developer.apple.com/documentation/screencapturekit/scstreamoutput
> **Fetched:** 2026-03-07
> **Description:** Complete documentation for the SCStreamOutput protocol, the delegate protocol for receiving captured video and audio frame data from a ScreenCaptureKit stream.

---

## Overview

**SCStreamOutput** is a delegate protocol that your app implements to receive capture stream output events from the ScreenCaptureKit framework.

### Abstract
A delegate protocol your app implements to receive capture stream output events.

---

## Declaration

### Swift
```swift
protocol SCStreamOutput : NSObjectProtocol
```

### Objective-C
```objc
@protocol SCStreamOutput <NSObject>
```

---

## Availability

| Platform | Version |
|----------|---------|
| **macOS** | 12.3+ |
| **Mac Catalyst** | 18.2+ |

---

## Protocol Details

### Inherits From
- `NSObjectProtocol` -- The group of methods that are fundamental to all Objective-C objects.

---

## Required Methods

### `stream(_:didOutputSampleBuffer:of:)`

Tells the delegate that a capture stream produced a frame.

#### Swift Declaration
```swift
func stream(
    _ stream: SCStream,
    didOutputSampleBuffer sampleBuffer: CMSampleBuffer,
    of outputType: SCStreamOutputType
)
```

#### Objective-C Declaration
```objc
- (void)stream:(SCStream *)stream
didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
        ofType:(SCStreamOutputType)outputType;
```

#### Parameters
- **stream** (`SCStream`) -- The stream that produced the frame
- **sampleBuffer** (`CMSampleBuffer`/`CMSampleBufferRef`) -- A buffer of media data containing the captured frame
- **outputType** (`SCStreamOutputType`) -- The type of output (audio or video)

#### Description
After you call `SCStream.startCapture(completionHandler:)`, the system provides frame data through this method. You can inspect the `CMSampleBuffer` to retrieve image data and examine metadata about the frame.

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

### Extract IOSurface and Frame Data

Convert the sample buffer's pixel buffer to an IOSurface:

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
```

### Handle Different Output Types

```swift
func stream(_ stream: SCStream, didOutputSampleBuffer sampleBuffer: CMSampleBuffer, of outputType: SCStreamOutputType) {

    // Return early if the sample buffer is invalid.
    guard sampleBuffer.isValid else { return }

    // Determine which type of data the sample buffer contains.
    switch outputType {
    case .screen:
        // Process the screen content (video frames).
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

## Per-Frame Metadata Keys

Metadata is accessed from the `CMSampleBuffer` attachments array:

| Key | Type | Description |
|-----|------|-------------|
| `.status` | `SCFrameStatus` | Frame completion status |
| `.dirtyRects` | `[CGRect]` | Regions with new content since previous frame |
| `.contentRect` | `CGRect` | Region of interest of captured content in the output frame |
| `.contentScale` | `CGFloat` | Scaling factor applied to fit content into the frame output |
| `.scaleFactor` | `CGFloat` | Display pixel density ratio (logical points to backing surface pixels) |

### Dirty Rects

```swift
func streamUpdateHandler(_ stream: SCStream, sampleBuffer: CMSampleBuffer) {
    guard let attachmentsArray = CMSampleBufferGetSampleAttachmentsArray(sampleBuffer,
                                                          createIfNecessary: false) as?
                                                         [[SCStreamFrameInfo: Any]],
        let attachments = attachmentsArray.first else { return }

        let dirtyRects = attachments[.dirtyRects]
    }
}

// Only encode and transmit the content within dirty rects
```

**Purpose**: Indicate regions where new content exists since the previous frame. Enables efficient encoding by only transmitting changed regions.

### Scale Factor Details

- **Scale factor 2 (Retina):** 1 logical point = 4 backing surface pixels
- **Scale factor 1 (non-Retina):** 1 logical point = 1 backing surface pixel

**Display Mismatch Example:**
- Source: Retina display (scale factor 2)
- Target: Non-Retina display (scale factor 1)
- Without adjustment: Captured content appears 4x larger
- Solution: Scale captured content by the scale factor before display

---

## Processing Audio Sample Buffers

### Create Audio Buffer

Convert audio sample buffers to `AVAudioPCMBuffer`:

```swift
private func handleAudio(for buffer: CMSampleBuffer) -> Void? {
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

## Related Topics

### Output Processing
- `SCStreamOutputType` -- Constants that represent output types for a stream frame
- `SCStreamFrameInfo` -- An instance that defines metadata keys for a stream frame
- `SCFrameStatus` -- Status values for a frame from a stream

### Core Types
- `SCStream` -- An instance that represents a stream of shareable content
- `CMSampleBuffer` -- A reference to a buffer of media data

---

## Usage Pattern

1. Create a class that conforms to `SCStreamOutput`
2. Implement the `stream(_:didOutputSampleBuffer:of:)` method
3. Add your object as the output on an `SCStream` instance via `addStreamOutput(_:type:sampleHandlerQueue:)`
4. Call `startCapture(completionHandler:)` to begin receiving frames
5. Process the sample buffers in the delegate method
