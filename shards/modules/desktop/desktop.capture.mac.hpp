/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2024 Fragcolor Pte. Ltd. */

#ifndef SH_EXTRA_DESKTOP_CAPTURE_MAC
#define SH_EXTRA_DESKTOP_CAPTURE_MAC

#include <shards/shards.h>
#include <shards/core/runtime.hpp>
#import <Foundation/Foundation.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>
#import <Metal/Metal.h>
#import <CoreVideo/CoreVideo.h>
#import <IOSurface/IOSurface.h>
#import <CoreGraphics/CoreGraphics.h>
#include <atomic>
#include <memory>

// Stream delegate to receive captured frames (must be at global scope)
@interface ScreenCaptureStreamDelegate : NSObject<SCStreamOutput>
@property (nonatomic, assign) std::atomic<CVPixelBufferRef> *latestFrame;
@property (nonatomic, assign) std::atomic<bool> *frameReady;
- (instancetype)initWithFramePointer:(std::atomic<CVPixelBufferRef> *)framePtr readyFlag:(std::atomic<bool> *)readyFlag;
@end

@implementation ScreenCaptureStreamDelegate

- (instancetype)initWithFramePointer:(std::atomic<CVPixelBufferRef> *)framePtr readyFlag:(std::atomic<bool> *)readyFlag {
    self = [super init];
    if (self) {
        _latestFrame = framePtr;
        _frameReady = readyFlag;
    }
    return self;
}

- (void)stream:(SCStream *)stream didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer ofType:(SCStreamOutputType)type {
    if (type != SCStreamOutputTypeScreen) {
        return;
    }

    CVPixelBufferRef pixelBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
    if (!pixelBuffer) {
        return;
    }

    // Retain the new buffer
    CVPixelBufferRetain(pixelBuffer);

    // Release the old buffer
    CVPixelBufferRef oldBuffer = _latestFrame->exchange(pixelBuffer);
    if (oldBuffer) {
        CVPixelBufferRelease(oldBuffer);
    }

    _frameReady->store(true, std::memory_order_release);
}

@end

namespace shards {

struct TonemapParams {
    float exposure;
    float gamma;
    float dither;
    float pad;
};

class ScreenCaptureKitGrabber {
    SCStream *_stream;
    SCStreamConfiguration *_config;
    ScreenCaptureStreamDelegate *_delegate;

    // Metal for GPU processing
    id<MTLDevice> _metalDevice;
    id<MTLCommandQueue> _commandQueue;
    id<MTLComputePipelineState> _tonemapPipeline;
    id<MTLBuffer> _paramsBuffer;
    CVMetalTextureCacheRef _textureCache;

    // Frame management
    std::atomic<CVPixelBufferRef> _currentFrame;
    std::atomic<bool> _frameReady;
    uint8_t *_cpuBuffer;

    // Display info
    int _left, _right, _top, _bottom, _width, _height;
    CGDirectDisplayID _displayID;

    // HDR support
    bool _isHDR;
    bool _isEDR; // Extended Dynamic Range
    bool _enableDither;

    // Timeout for frame capture
    static constexpr uint32_t FRAME_TIMEOUT_MS = 1000;

public:
    enum State { Normal, Timeout, Lost, Error };

    int left() const { return _left; }
    int right() const { return _right; }
    int top() const { return _top; }
    int bottom() const { return _bottom; }
    int width() const { return _width; }
    int height() const { return _height; }
    const uint8_t *image() const { return _cpuBuffer; }
    bool isHDR() const { return _isHDR; }
    bool isEDR() const { return _isEDR; }

    static bool FindDisplay(int x, int y, CGDirectDisplayID &outDisplayID, CGRect &outBounds) {
        constexpr uint32_t maxDisplays = 32;
        CGDirectDisplayID displays[maxDisplays];
        uint32_t displayCount;

        if (CGGetActiveDisplayList(maxDisplays, displays, &displayCount) != kCGErrorSuccess) {
            SHLOG_ERROR("Failed to get active display list");
            return false;
        }

        for (uint32_t i = 0; i < displayCount; i++) {
            CGRect bounds = CGDisplayBounds(displays[i]);
            if (x >= bounds.origin.x && x < bounds.origin.x + bounds.size.width &&
                y >= bounds.origin.y && y < bounds.origin.y + bounds.size.height) {
                outDisplayID = displays[i];
                outBounds = bounds;
                return true;
            }
        }

        SHLOG_ERROR("No display found at coordinates ({}, {})", x, y);
        return false;
    }

    ScreenCaptureKitGrabber(int x, int y, int width, int height)
        : _stream(nil), _config(nil), _delegate(nil), _metalDevice(nil), _commandQueue(nil),
          _tonemapPipeline(nil), _paramsBuffer(nil), _textureCache(nullptr),
          _cpuBuffer(nullptr), _isHDR(false), _isEDR(false), _enableDither(true) {

        _currentFrame.store(nullptr);
        _frameReady.store(false);

        // Find display
        CGRect bounds;
        if (!FindDisplay(x, y, _displayID, bounds)) {
            SHLOG_ERROR("Failed to find display for screen capture");
            return;
        }

        _left = bounds.origin.x;
        _top = bounds.origin.y;
        _width = bounds.size.width;
        _height = bounds.size.height;
        _right = _left + _width;
        _bottom = _top + _height;

        // Check for HDR/EDR support
        if (@available(macOS 11.0, *)) {
            NSScreen *screen = [NSScreen mainScreen];
            _isEDR = screen.maximumExtendedDynamicRangeColorComponentValue > 1.0;
            _isHDR = _isEDR;
        }

        // Initialize Metal
        _metalDevice = MTLCreateSystemDefaultDevice();
        if (!_metalDevice) {
            SHLOG_ERROR("Failed to create Metal device");
            return;
        }

        _commandQueue = [_metalDevice newCommandQueue];
        if (!_commandQueue) {
            SHLOG_ERROR("Failed to create Metal command queue");
            return;
        }

        // Create texture cache
        CVReturn cvret = CVMetalTextureCacheCreate(
            kCFAllocatorDefault,
            nil,
            _metalDevice,
            nil,
            &_textureCache
        );

        if (cvret != kCVReturnSuccess) {
            SHLOG_ERROR("Failed to create Metal texture cache");
            return;
        }

        // Allocate CPU buffer
        _cpuBuffer = new uint8_t[4 * _width * _height];

        // Initialize ScreenCaptureKit
        if (@available(macOS 12.3, *)) {
            initializeScreenCapture();
        } else {
            SHLOG_ERROR("ScreenCaptureKit requires macOS 12.3 or later");
        }
    }

    ~ScreenCaptureKitGrabber() {
        if (@available(macOS 12.3, *)) {
            if (_stream) {
                [_stream stopCaptureWithCompletionHandler:^(NSError * _Nullable error) {
                    if (error) {
                        SHLOG_ERROR("Error stopping capture: {}", [[error localizedDescription] UTF8String]);
                    }
                }];
                _stream = nil;
            }
        }

        CVPixelBufferRef buffer = _currentFrame.exchange(nullptr);
        if (buffer) {
            CVPixelBufferRelease(buffer);
        }

        if (_textureCache) {
            CFRelease(_textureCache);
        }

        delete[] _cpuBuffer;

        _delegate = nil;
        _stream = nil;
        _config = nil;
        _metalDevice = nil;
        _commandQueue = nil;
        _tonemapPipeline = nil;
        _paramsBuffer = nil;
    }

    void initializeScreenCapture() API_AVAILABLE(macos(12.3)) {
        // Create stream configuration
        _config = [[SCStreamConfiguration alloc] init];
        _config.width = _width;
        _config.height = _height;
        _config.minimumFrameInterval = CMTimeMake(1, 60); // 60 FPS max
        _config.pixelFormat = kCVPixelFormatType_32BGRA;
        _config.showsCursor = true;

        // Get shareable content - explicitly request all content including desktop windows
        __block SCShareableContent *shareableContent = nil;
        __block NSError *contentError = nil;

        dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
        [SCShareableContent getShareableContentExcludingDesktopWindows:NO
                                                      onScreenWindowsOnly:NO
                                                        completionHandler:^(SCShareableContent * _Nullable content, NSError * _Nullable error) {
            shareableContent = content;
            contentError = error;
            dispatch_semaphore_signal(semaphore);
        }];
        dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);

        // Check for success first (shareableContent should be non-nil)
        if (!shareableContent) {
            if (contentError) {
                NSString *errorDesc = [contentError localizedDescription];
                const char *errorStr = errorDesc ? [errorDesc UTF8String] : nullptr;
                if (errorStr) {
                    SHLOG_ERROR("Failed to get shareable content: {}", errorStr);
                } else {
                    SHLOG_ERROR("Failed to get shareable content (no error description)");
                }
            } else {
                SHLOG_ERROR("Failed to get shareable content (no error returned)");
            }
            return;
        }

        // Check if displays array is empty (permission issue)
        if (!shareableContent.displays || [shareableContent.displays count] == 0) {
            SHLOG_ERROR("No displays available in shareable content. This usually means:");
            SHLOG_ERROR("  1. Screen Recording permission is not granted");
            SHLOG_ERROR("  2. Go to System Settings > Privacy & Security > Screen Recording");
            SHLOG_ERROR("  3. Enable permission for this application");
            return;
        }

        // Find the display
        SCDisplay *targetDisplay = nil;
        for (SCDisplay *display in shareableContent.displays) {
            if (display.displayID == _displayID) {
                targetDisplay = display;
                break;
            }
        }

        if (!targetDisplay) {
            SHLOG_ERROR("Could not find target display {} in shareable content (found {} displays)",
                       _displayID, [shareableContent.displays count]);
            return;
        }

        // Create content filter for the display
        SCContentFilter *filter = [[SCContentFilter alloc] initWithDisplay:targetDisplay excludingWindows:@[]];

        // Create delegate
        _delegate = [[ScreenCaptureStreamDelegate alloc] initWithFramePointer:&_currentFrame readyFlag:&_frameReady];

        // Create stream
        _stream = [[SCStream alloc] initWithFilter:filter configuration:_config delegate:nil];

        if (!_stream) {
            SHLOG_ERROR("Failed to create SCStream");
            return;
        }

        // Add stream output
        NSError *outputError = nil;
        [_stream addStreamOutput:_delegate type:SCStreamOutputTypeScreen sampleHandlerQueue:dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0) error:&outputError];

        if (outputError) {
            SHLOG_ERROR("Failed to add stream output: {}", [[outputError localizedDescription] UTF8String]);
            return;
        }

        // Start capture
        __block NSError *startError = nil;
        dispatch_semaphore_t startSemaphore = dispatch_semaphore_create(0);
        [_stream startCaptureWithCompletionHandler:^(NSError * _Nullable error) {
            startError = error;
            dispatch_semaphore_signal(startSemaphore);
        }];
        dispatch_semaphore_wait(startSemaphore, DISPATCH_TIME_FOREVER);

        if (startError) {
            SHLOG_ERROR("Failed to start capture: {}", [[startError localizedDescription] UTF8String]);
            return;
        }

        SHLOG_INFO("ScreenCaptureKit initialized successfully for display {}", _displayID);
    }

    bool createTonemapPipeline() {
        if (_tonemapPipeline) {
            return true; // Already created
        }

        // Load Metal shader from embedded metallib or compile from source
        // For now, we'll use a simple shader source
        NSString *shaderSource = @R"(
            #include <metal_stdlib>
            using namespace metal;

            struct TonemapParams {
                float exposure;
                float gamma;
                float dither;
                float pad;
            };

            float3 reinhard(float3 hdr) {
                return hdr / (1.0h + hdr);
            }

            float random(float2 p) {
                return fract(sin(dot(p, float2(12.9898, 78.233))) * 43758.5453);
            }

            kernel void tonemap_reinhard(
                texture2d<half, access::read> hdrInput [[texture(0)]],
                texture2d<half, access::write> sdrOutput [[texture(1)]],
                constant TonemapParams &params [[buffer(0)]],
                uint2 gid [[thread_position_in_grid]])
            {
                half4 hdr = hdrInput.read(gid);
                float3 color = float3(hdr.rgb) * params.exposure;
                color = reinhard(color);
                color = pow(color, 1.0 / params.gamma);

                if (params.dither > 0.0) {
                    float noise = random(float2(gid)) * 2.0 - 1.0;
                    color += noise * params.dither / 255.0;
                }

                sdrOutput.write(half4(half3(saturate(color)), hdr.a), gid);
            }
        )";

        NSError *error = nil;
        id<MTLLibrary> library = [_metalDevice newLibraryWithSource:shaderSource options:nil error:&error];
        if (!library) {
            SHLOG_ERROR("Failed to create Metal library: {}", [[error localizedDescription] UTF8String]);
            return false;
        }

        id<MTLFunction> kernelFunction = [library newFunctionWithName:@"tonemap_reinhard"];
        if (!kernelFunction) {
            SHLOG_ERROR("Failed to find tonemap kernel function");
            return false;
        }

        _tonemapPipeline = [_metalDevice newComputePipelineStateWithFunction:kernelFunction error:&error];
        if (!_tonemapPipeline) {
            SHLOG_ERROR("Failed to create compute pipeline: {}", [[error localizedDescription] UTF8String]);
            return false;
        }

        // Create parameter buffer
        _paramsBuffer = [_metalDevice newBufferWithLength:sizeof(TonemapParams) options:MTLResourceStorageModeShared];
        if (!_paramsBuffer) {
            SHLOG_ERROR("Failed to create parameter buffer");
            return false;
        }

        return true;
    }

    void toneMapTexture(CVPixelBufferRef pixelBuffer) {
        if (!_isHDR || !createTonemapPipeline()) {
            return; // Skip tone mapping if not HDR or pipeline creation failed
        }

        // Create Metal texture from pixel buffer
        size_t bufferWidth = CVPixelBufferGetWidth(pixelBuffer);
        size_t bufferHeight = CVPixelBufferGetHeight(pixelBuffer);

        CVMetalTextureRef inputTextureRef = nullptr;
        CVReturn cvret = CVMetalTextureCacheCreateTextureFromImage(
            kCFAllocatorDefault,
            _textureCache,
            pixelBuffer,
            nil,
            MTLPixelFormatBGRA8Unorm,
            bufferWidth,
            bufferHeight,
            0,
            &inputTextureRef
        );

        if (cvret != kCVReturnSuccess || !inputTextureRef) {
            SHLOG_ERROR("Failed to create input Metal texture from pixel buffer");
            return;
        }

        id<MTLTexture> inputTexture = CVMetalTextureGetTexture(inputTextureRef);

        // Create output texture
        MTLTextureDescriptor *outputDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                                                               width:bufferWidth
                                                                                              height:bufferHeight
                                                                                           mipmapped:NO];
        outputDesc.usage = MTLTextureUsageShaderWrite | MTLTextureUsageShaderRead;
        id<MTLTexture> outputTexture = [_metalDevice newTextureWithDescriptor:outputDesc];

        // Update parameters
        TonemapParams *params = (TonemapParams *)[_paramsBuffer contents];
        params->exposure = 0.5f;
        params->gamma = 2.2f;
        params->dither = _enableDither ? 0.5f : 0.0f;

        // Encode compute command
        id<MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];
        id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];

        [encoder setComputePipelineState:_tonemapPipeline];
        [encoder setTexture:inputTexture atIndex:0];
        [encoder setTexture:outputTexture atIndex:1];
        [encoder setBuffer:_paramsBuffer offset:0 atIndex:0];

        MTLSize threadgroupSize = MTLSizeMake(8, 8, 1);
        MTLSize threadgroups = MTLSizeMake(
            (bufferWidth + threadgroupSize.width - 1) / threadgroupSize.width,
            (bufferHeight + threadgroupSize.height - 1) / threadgroupSize.height,
            1
        );

        [encoder dispatchThreadgroups:threadgroups threadsPerThreadgroup:threadgroupSize];
        [encoder endEncoding];

        [commandBuffer commit];
        [commandBuffer waitUntilCompleted];

        CFRelease(inputTextureRef);
    }

    State capture() {
        // Check if frame is ready
        if (!_frameReady.load(std::memory_order_acquire)) {
            return Timeout;
        }

        return Normal;
    }

    void update() {
        CVPixelBufferRef buffer = _currentFrame.load(std::memory_order_acquire);
        if (!buffer) {
            return;
        }

        // Apply HDR tone mapping if needed
        if (_isHDR) {
            toneMapTexture(buffer);
        }

        // Lock pixel buffer for CPU access
        CVPixelBufferLockBaseAddress(buffer, kCVPixelBufferLock_ReadOnly);

        void *baseAddress = CVPixelBufferGetBaseAddress(buffer);
        size_t bytesPerRow = CVPixelBufferGetBytesPerRow(buffer);
        size_t bufferHeight = CVPixelBufferGetHeight(buffer);

        // Copy to CPU buffer
        if (bytesPerRow == (size_t)(_width * 4)) {
            // Direct copy
            memcpy(_cpuBuffer, baseAddress, _width * _height * 4);
        } else {
            // Row-by-row copy
            uint8_t *src = (uint8_t *)baseAddress;
            uint8_t *dst = _cpuBuffer;
            for (size_t y = 0; y < (size_t)_height && y < bufferHeight; y++) {
                memcpy(dst, src, std::min((size_t)_width * 4, bytesPerRow));
                src += bytesPerRow;
                dst += _width * 4;
            }
        }

        CVPixelBufferUnlockBaseAddress(buffer, kCVPixelBufferLock_ReadOnly);

        // Reset frame ready flag
        _frameReady.store(false, std::memory_order_release);
    }

    void setDithering(bool enable) { _enableDither = enable; }
};

}; // namespace shards

#endif // SH_EXTRA_DESKTOP_CAPTURE_MAC