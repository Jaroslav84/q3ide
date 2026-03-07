# SCShareableContent

> **Source:** Apple Developer Documentation
> **URL:** https://developer.apple.com/documentation/screencapturekit/scshareablecontent
> **Fetched:** 2026-03-07
> **Description:** Complete documentation for the SCShareableContent class, which represents the set of displays, apps, and windows available for capture.

---

## Class Declaration

```swift
class SCShareableContent : NSObject
```

**Objective-C:**
```objc
@interface SCShareableContent : NSObject
```

## Overview

An instance that represents a set of displays, apps, and windows that your app can capture.

**Abstract:** Use the `displays`, `windows`, and `applications` properties to create an `SCContentFilter` object that specifies what display content to capture. You apply the filter to an instance of `SCStream` to limit its output to only the content matching your filter.

## Availability

| Platform | Introduced |
|----------|------------|
| macOS | 12.3 |
| Mac Catalyst | 18.2 |

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

### Retrieving Shareable Content

#### `getWithCompletionHandler(_:)`
Retrieves the displays, apps, and windows that your app can capture.

```swift
class func getWithCompletionHandler((SCShareableContent?, (any Error)?) -> Void)
```

**Objective-C:**
```objc
+ (void)getShareableContentWithCompletionHandler:(void (^)(SCShareableContent * _Nullable, NSError * _Nullable))completionHandler;
```

#### `getExcludingDesktopWindows(_:onScreenWindowsOnly:completionHandler:)`
Retrieves the displays, apps, and windows that match your criteria.

```swift
class func getExcludingDesktopWindows(Bool, onScreenWindowsOnly: Bool, completionHandler: (SCShareableContent?, (any Error)?) -> Void)
```

**Objective-C:**
```objc
+ (void)getShareableContentExcludingDesktopWindows:(BOOL)excludeDesktopWindows
                              onScreenWindowsOnly:(BOOL)onScreenWindowsOnly
                                  completionHandler:(void (^)(SCShareableContent * _Nullable, NSError * _Nullable))completionHandler;
```

#### `getExcludingDesktopWindows(_:onScreenWindowsOnlyAbove:completionHandler:)`
Retrieves the displays, apps, and windows that are in front of the specified window.

```swift
class func getExcludingDesktopWindows(Bool, onScreenWindowsOnlyAbove: SCWindow, completionHandler: (SCShareableContent?, (any Error)?) -> Void)
```

**Objective-C:**
```objc
+ (void)getShareableContentExcludingDesktopWindows:(BOOL)excludeDesktopWindows
                        onScreenWindowsOnlyAboveWindow:(SCWindow *)window
                                      completionHandler:(void (^)(SCShareableContent * _Nullable, NSError * _Nullable))completionHandler;
```

#### `getExcludingDesktopWindows(_:onScreenWindowsOnlyBelow:completionHandler:)`
Retrieves the displays, apps, and windows that are behind the specified window.

```swift
class func getExcludingDesktopWindows(Bool, onScreenWindowsOnlyBelow: SCWindow, completionHandler: (SCShareableContent?, (any Error)?) -> Void)
```

**Objective-C:**
```objc
+ (void)getShareableContentExcludingDesktopWindows:(BOOL)excludeDesktopWindows
                        onScreenWindowsOnlyBelowWindow:(SCWindow *)window
                                      completionHandler:(void (^)(SCShareableContent * _Nullable, NSError * _Nullable))completionHandler;
```

#### `info(for:)`
Retrieves any available sharable content information that matches the provided filter.

```swift
class func info(for: SCContentFilter) -> SCShareableContentInfo
```

**Objective-C:**
```objc
+ (SCShareableContentInfo *)infoForFilter:(SCContentFilter *)filter;
```

### Inspecting Shareable Content

#### `windows`
The windows available for capture.

```swift
var windows: [SCWindow]
```

**Objective-C:**
```objc
@property (readonly) NSArray<SCWindow *> *windows;
```

#### `displays`
The displays available for capture.

```swift
var displays: [SCDisplay]
```

**Objective-C:**
```objc
@property (readonly) NSArray<SCDisplay *> *displays;
```

#### `applications`
The apps available for capture.

```swift
var applications: [SCRunningApplication]
```

**Objective-C:**
```objc
@property (readonly) NSArray<SCRunningApplication *> *applications;
```

### Type Methods

#### `getCurrentProcessShareableContent(completionHandler:)`
Retrieves shareable content for the current process.

```swift
class func getCurrentProcessShareableContent(completionHandler: (SCShareableContent?, (any Error)?) -> Void)
```

**Objective-C:**
```objc
+ (void)getCurrentProcessShareableContentWithCompletionHandler:(void (^)(SCShareableContent * _Nullable, NSError * _Nullable))completionHandler;
```

---

## Related Types

- **SCShareableContentInfo** -- Provides information for the content in a given stream
- **SCShareableContentStyle** -- The style of content presented in a stream
- **SCDisplay** -- Represents a display device
- **SCRunningApplication** -- Represents an app running on a device
- **SCWindow** -- Represents an onscreen window
- **SCContentFilter** -- Filters the content a stream captures
- **SCStream** -- Represents a stream of shareable content
