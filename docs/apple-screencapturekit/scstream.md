# SCStream

> **Source:** Apple Developer Documentation
> **URL:** https://developer.apple.com/documentation/screencapturekit/scstream
> **Fetched:** 2026-03-07
> **Description:** Complete documentation for the SCStream class, which represents a stream of shareable screen content.

---

## Overview

**SCStream** is a class that represents a stream of shareable content. It enables applications to capture video of screen content like apps and windows using the ScreenCaptureKit framework.

**Abstract:** An instance that represents a stream of shareable content.

---

## Class Declaration

### Swift
```swift
class SCStream
```

### Objective-C
```objc
@interface SCStream : NSObject
```

---

## Availability

| Platform | Introduced | Status |
|----------|-----------|--------|
| **macOS** | 12.3 | Available |
| **Mac Catalyst** | 18.2 | Available |

---

## Inheritance & Conformance

**Inherits From:**
- `NSObject`

**Conforms To:**
- `CVarArg`
- `CustomDebugStringConvertible`
- `CustomStringConvertible`
- `Equatable`
- `Hashable`
- `NSObjectProtocol`

---

## Topics

### Creating a Stream

#### `init(filter:configuration:delegate:)`
Creates a stream with a content filter and configuration.

```swift
init(
    filter: SCContentFilter,
    configuration: SCStreamConfiguration,
    delegate: (any SCStreamDelegate)?
)
```

**Parameters:**
- `filter` (`SCContentFilter`): An instance that filters the content the stream captures
- `configuration` (`SCStreamConfiguration`): An instance that provides the output configuration for the stream
- `delegate` (`SCStreamDelegate?`): A delegate protocol your app implements to respond to stream events

---

### Updating Stream Configuration

#### `updateConfiguration(_:completionHandler:)`
Updates the stream with a new configuration.

```swift
func updateConfiguration(
    _ configuration: SCStreamConfiguration,
    completionHandler: (((any Error)?) -> Void)?
)
```

**Parameters:**
- `configuration`: New stream configuration
- `completionHandler`: Callback indicating success or error

#### `updateContentFilter(_:completionHandler:)`
Updates the stream by applying a new content filter.

```swift
func updateContentFilter(
    _ contentFilter: SCContentFilter,
    completionHandler: (((any Error)?) -> Void)?
)
```

**Parameters:**
- `contentFilter`: New content filter to apply
- `completionHandler`: Callback indicating success or error

---

### Adding and Removing Stream Output

#### `addStreamOutput(_:type:sampleHandlerQueue:)`
Adds a destination that receives the stream output.

```swift
func addStreamOutput(
    _ streamOutput: any SCStreamOutput,
    type: SCStreamOutputType,
    sampleHandlerQueue: dispatch_queue_t?
) throws
```

**Parameters:**
- `streamOutput`: The output destination
- `type`: Type of stream output
- `sampleHandlerQueue`: Queue for handling samples (optional)

**Throws:** Error if the output cannot be added

#### `removeStreamOutput(_:type:)`
Removes a destination from receiving stream output.

```swift
func removeStreamOutput(
    _ streamOutput: any SCStreamOutput,
    type: SCStreamOutputType
) throws
```

**Parameters:**
- `streamOutput`: The output destination to remove
- `type`: Type of stream output

**Throws:** Error if the output cannot be removed

---

### Adding and Removing Recording Output

#### `addRecordingOutput(_:)`
Adds a recording output to the stream.

```swift
func addRecordingOutput(_ recordingOutput: SCRecordingOutput) throws
```

**Parameters:**
- `recordingOutput`: Recording output to add

**Throws:** Error if the output cannot be added

#### `removeRecordingOutput(_:)`
Removes a recording output from the stream.

```swift
func removeRecordingOutput(_ recordingOutput: SCRecordingOutput) throws
```

**Parameters:**
- `recordingOutput`: Recording output to remove

**Throws:** Error if the output cannot be removed

---

### Starting and Stopping a Stream

#### `startCapture(completionHandler:)`
Starts the stream with a callback to indicate whether it successfully starts.

```swift
func startCapture(completionHandler: (((any Error)?) -> Void)?)
```

**Parameters:**
- `completionHandler`: Callback with optional error indicating start status

#### `stopCapture(completionHandler:)`
Stops the stream.

```swift
func stopCapture(completionHandler: (((any Error)?) -> Void)?)
```

**Parameters:**
- `completionHandler`: Callback with optional error indicating stop status

---

### Stream Synchronization

#### `synchronizationClock`
A clock to use for output synchronization.

```swift
var synchronizationClock: CMClock?
```

**Type:** `CMClock?`

**Description:** Provides timing reference for synchronized output of stream data.

---

## Related Types

### SCContentFilter
An instance that filters the content a stream captures. Used in stream initialization to determine which screen content to capture.

### SCStreamConfiguration
An instance that provides the output configuration for a stream. Defines output parameters like resolution and quality.

### SCStreamDelegate
A delegate protocol your app implements to respond to stream events.

### SCRecordingOutput
Output object for recording stream content.

### SCScreenshotManager
An instance for the capture of single frames from a stream.

### SCScreenshotConfiguration
An object that contains screenshot properties such as output width, height, and image quality specifications.

### SCScreenshotOutput
An object that contains all images requested by the client.

---

## Usage Pattern

```swift
// 1. Create a content filter
let contentFilter = SCContentFilter(...)

// 2. Create stream configuration
let streamConfig = SCStreamConfiguration()

// 3. Create the stream
let stream = SCStream(
    filter: contentFilter,
    configuration: streamConfig,
    delegate: self
)

// 4. Add stream output
try stream.addStreamOutput(output, type: .screen, sampleHandlerQueue: nil)

// 5. Start capturing
stream.startCapture { error in
    if let error = error {
        print("Failed to start capture: \(error)")
    }
}

// 6. Update configuration if needed
stream.updateConfiguration(newConfig) { error in
    // Handle completion
}

// 7. Stop capturing when done
stream.stopCapture { error in
    // Handle completion
}
```

---

## See Also

- **SCStreamConfiguration** - Output configuration for streams
- **SCContentFilter** - Content filtering for streams
- **SCStreamDelegate** - Stream event delegation
- **SCScreenshotManager** - Single frame capture
- **SCScreenshotConfiguration** - Screenshot configuration
- **SCScreenshotOutput** - Screenshot output container
