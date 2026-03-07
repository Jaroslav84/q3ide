# SCContentFilter

> **Source:** Apple Developer Documentation
> **URL:** https://developer.apple.com/documentation/screencapturekit/sccontentfilter
> **Fetched:** 2026-03-07
> **Description:** Complete documentation for the SCContentFilter class, which filters the content a stream captures based on displays, applications, and windows.

---

## Class Overview

**SCContentFilter** is a class in Apple's ScreenCaptureKit framework that filters the content a stream captures.

### Abstract
> An instance that filters the content a stream captures.

### Declaration
```swift
class SCContentFilter
```

### Availability
- **macOS**: 12.3+
- **Mac Catalyst**: 18.2+

---

## Inheritance & Conformance

### Inherits From
- `NSObject`

### Conforms To
- `CVarArg`
- `CustomDebugStringConvertible`
- `CustomStringConvertible`
- `Equatable`
- `Hashable`
- `NSObjectProtocol`

---

## Overview

Use a content filter to limit an `SCStream` object's output to only that matching your filter criteria. Retrieve the displays, apps, and windows that your app can capture from an instance of `SCShareableContent`.

---

## Creating a Filter

### Initializers

#### `init(desktopIndependentWindow:)`
Creates a filter that captures only the specified window.
```swift
init(desktopIndependentWindow: SCWindow)
```

**Behavior:**
- Video output includes only the specified window
- Audio output includes all audio from the containing application (even if from non-captured windows)
- Window is captured even when occluded or off-screen
- Capture pauses when window is minimized, resumes when restored
- Child/popup windows are NOT included
- Output is always offset at top-left corner

#### `init(display:including:)`
Creates a filter that captures only specific windows from a display.
```swift
init(display: SCDisplay, including: [SCWindow])
```

**Behavior:**
- By default, no windows are captured -- must explicitly add windows
- New windows created after filter setup are NOT automatically included
- Child/popup windows are NOT included
- Windows moved off-screen are removed from output

#### `init(display:excludingWindows:)`
Creates a filter that captures the contents of a display, excluding the specified windows.
```swift
init(display: SCDisplay, excludingWindows: [SCWindow])
```

#### `init(display:including:exceptingWindows:)`
Creates a filter that captures a display, including only windows of the specified apps.
```swift
init(display: SCDisplay, including: [SCRunningApplication], exceptingWindows: [SCWindow])
```

**Behavior:**
- All windows and audio from specified apps automatically included
- New windows from included apps are automatically captured
- Child/popup windows are automatically captured
- Useful for tutorials showing full app interaction
- Can exclude specific windows via `exceptingWindows`

#### `init(display:excludingApplications:exceptingWindows:)`
Creates a filter that captures a display, excluding windows of the specified apps.
```swift
init(display: SCDisplay, excludingApplications: [SCRunningApplication], exceptingWindows: [SCWindow])
```

**Use Cases:**
- Remove screen capture app's own windows (prevent mirror hall effect)
- Exclude notification windows
- Remove participant camera previews
- System UI exclusion

**Audio Behavior:** Audio from excluded apps is removed from output entirely (app-level audio exclusion).

---

## Filter Properties

### Core Properties

#### `contentRect`
The size and location of the content to filter, in screen points.
```swift
var contentRect: CGRect
```

#### `pointPixelScale`
The scaling factor used to translate screen points into pixels.
```swift
var pointPixelScale: Float
```

#### `streamType` (Deprecated)
The type of the streaming content.
```swift
var streamType: SCStreamType
```

#### `style`
The display style of the sharable content.
```swift
var style: SCShareableContentStyle
```

---

## Instance Properties

#### `includeMenuBar`
Controls whether the menu bar is included in the capture.
```swift
var includeMenuBar: Bool
```

#### `includedApplications`
Array of running applications included in the filter.
```swift
var includedApplications: [SCRunningApplication]
```

#### `includedDisplays`
Array of displays included in the filter.
```swift
var includedDisplays: [SCDisplay]
```

#### `includedWindows`
Array of windows included in the filter.
```swift
var includedWindows: [SCWindow]
```

---

## Usage Examples

### Capture a Single Window

```swift
// Create a content filter that includes a single window.
let filter = SCContentFilter(desktopIndependentWindow: window)
```

### Capture Display with Excluded Apps

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
let filter = SCContentFilter(display: display,
                             excludingApplications: excludedApps,
                             exceptingWindows: [])
```

### Capture Display Including Specific Apps

```swift
let includingApplications = shareableContent.applications.filter {
    appBundleIDs.contains($0.bundleIdentifier)
}

let exceptingWindows = shareableContent.windows.filter {
    windowIDs.contains($0.windowID)
}

let contentFilter = SCContentFilter(display: display,
                                    including: includingApplications,
                                    exceptingWindows: exceptingWindows)
```

---

## Related Content

### See Also: Content Capture
- `SCStream` -- An instance that represents a stream of shareable content
- `SCStreamConfiguration` -- An instance that provides the output configuration for a stream
- `SCStreamDelegate` -- A delegate protocol your app implements to respond to stream events
- `SCScreenshotManager` -- An instance for the capture of single frames from a stream
- `SCScreenshotConfiguration` -- An object that contains screenshot properties
- `SCScreenshotOutput` -- An object that contains all images requested by the client
