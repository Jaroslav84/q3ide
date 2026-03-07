# ScreenCaptureKit Framework Overview

> **Source:** Apple Developer Documentation
> **URL:** https://developer.apple.com/documentation/screencapturekit
> **Fetched:** 2026-03-07
> **Description:** Complete overview of Apple's ScreenCaptureKit framework, including all classes, protocols, structures, and enumerations.

---

## Overview

ScreenCaptureKit is a macOS framework that enables high-performance frame capture of screen and audio content. It provides fine-grained control to select and stream only the content your app needs to capture.

**Abstract:** Filter and select screen content and stream it to your app.

**Platforms:**
- macOS 12.3+
- Mac Catalyst 18.2+

### Key Capabilities

- Capture video frames and audio samples as `CMSampleBuffer` objects
- Access detailed media metadata alongside captured content
- macOS-integrated content sharing picker (`SCContentSharingPicker`) for user-friendly stream management
- High-performance, efficient capture operations

---

## Framework Structure

### Essentials

1. **ScreenCaptureKit Updates** - Learn about important changes to the framework
2. **Persistent Content Capture Entitlement** (`com.apple.developer.persistent-content-capture`) - Boolean entitlement for VNC apps requiring persistent screen capture access
3. **Capturing screen content in macOS** - Sample code demonstrating how to stream desktop content like displays, apps, and windows

---

## Core Components

### Shareable Content

Classes representing capturable content:

| Class | Purpose |
|-------|---------|
| **SCShareableContent** | Represents a set of displays, apps, and windows available for capture |
| **SCShareableContentInfo** | Provides information about content in a given stream |
| **SCShareableContentStyle** | Enum defining the style of content presented in a stream |
| **SCDisplay** | Represents a display device |
| **SCRunningApplication** | Represents an app running on a device |
| **SCWindow** | Represents an onscreen window |

### Content Capture

Classes and protocols for capturing and configuring streams:

| Item | Type | Purpose |
|------|------|---------|
| **SCStream** | Class | Represents a stream of shareable content |
| **SCStreamConfiguration** | Class | Provides output configuration for a stream |
| **SCContentFilter** | Class | Filters the content a stream captures |
| **SCStreamDelegate** | Protocol | Responds to stream events |
| **SCScreenshotManager** | Class | Captures single frames from a stream |
| **SCScreenshotConfiguration** | Class | Contains screenshot properties (width, height, quality) |
| **SCScreenshotOutput** | Class | Contains all images requested by the client |

#### Screenshot Configuration - Display Intent

```swift
enum DisplayIntent {
    // Specifies the type of display a screenshot rendering optimizes for
}
```

#### Screenshot Configuration - Dynamic Range

```swift
enum DynamicRange {
    // Specifies image types: standard dynamic range (SDR),
    // high dynamic range (HDR), or both
}
```

### Output Processing

Classes and protocols for handling captured output:

| Item | Type | Purpose |
|------|------|---------|
| **SCStreamOutput** | Protocol | Delegate protocol for receiving capture stream output events |
| **SCStreamOutputType** | Enum | Constants representing output types for stream frames |
| **SCStreamFrameInfo** | Struct | Defines metadata keys for a stream frame |
| **SCFrameStatus** | Enum | Status values for frames from a stream |

---

## System Content-Sharing Picker

Classes for the macOS-integrated content picker:

| Item | Type | Purpose |
|------|------|---------|
| **SCContentSharingPicker** | Class | Picker instance presented by the OS for managing frame-capture streams |
| **SCContentSharingPickerConfiguration** | Struct | Configuration options for the system content-sharing picker |
| **SCContentSharingPickerMode** | Struct | Available modes for selecting streaming content from the picker |
| **SCContentSharingPickerObserver** | Protocol | Observer protocol for receiving picker messages from the OS |

---

## Error Handling

| Item | Type | Purpose |
|------|------|---------|
| **SCStreamErrorDomain** | String constant | String representation of the error domain |
| **SCStreamError** | Struct | Instance representing a ScreenCaptureKit framework error |
| **SCStreamError.Code** | Enum | Codes for user cancellation events and errors in ScreenCaptureKit |

---

## WWDC Resources

The framework documentation references the following Apple Developer sessions:

- **WWDC 2022, Session 10156:** [Meet ScreenCaptureKit](https://developer.apple.com/wwdc22/10156)
- **WWDC 2022, Session 10155:** [Take ScreenCaptureKit to the next level](https://developer.apple.com/wwdc22/10155)
- **WWDC 2023, Session 10136:** [What's new in ScreenCaptureKit](https://developer.apple.com/videos/play/wwdc2023/10136/)

---

## Typical Workflow

1. **Get Shareable Content** - Use `SCShareableContent` to discover available displays, apps, and windows
2. **Create Content Filter** - Use `SCContentFilter` to specify what content to capture
3. **Configure Stream** - Set up `SCStreamConfiguration` with output parameters
4. **Create Stream** - Initialize `SCStream` with the filter and configuration
5. **Implement Output Handler** - Conform to `SCStreamOutput` protocol to receive `CMSampleBuffer` frames
6. **Optional: User Selection** - Use `SCContentSharingPicker` for integrated OS-level content selection

---

## Key Interface Languages

Documentation is available for:
- **Swift** (primary interface)
- **Objective-C** (variant support)
