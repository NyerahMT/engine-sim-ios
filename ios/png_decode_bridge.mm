#import <UIKit/UIKit.h>

#include <climits>
#include <cstdlib>
#include <limits>

extern "C" unsigned char *EngineSimLoadPngRgba(
    const char *path,
    int *widthOut,
    int *heightOut)
{
    if (path == nullptr || widthOut == nullptr || heightOut == nullptr)
        return nullptr;

    @autoreleasepool {
        NSString *filePath =
            [NSString stringWithUTF8String:path];

        if (filePath == nil)
            return nullptr;

        UIImage *image =
            [UIImage imageWithContentsOfFile:filePath];

        CGImageRef cgImage =
            image.CGImage;

        if (cgImage == nullptr)
            return nullptr;

        const size_t width =
            CGImageGetWidth(cgImage);

        const size_t height =
            CGImageGetHeight(cgImage);

        if (
            width == 0
            || height == 0
            || width > static_cast<size_t>(INT_MAX)
            || height > static_cast<size_t>(INT_MAX)
            || width > std::numeric_limits<size_t>::max() / 4
            || height > std::numeric_limits<size_t>::max() / (width * 4))
        {
            return nullptr;
        }

        const size_t bytesPerRow =
            width * 4;

        unsigned char *pixels =
            static_cast<unsigned char *>(
                std::calloc(height, bytesPerRow));

        if (pixels == nullptr)
            return nullptr;

        CGColorSpaceRef colorSpace =
            CGColorSpaceCreateDeviceRGB();

        if (colorSpace == nullptr) {
            std::free(pixels);
            return nullptr;
        }

        CGContextRef context =
            CGBitmapContextCreate(
                pixels,
                width,
                height,
                8,
                bytesPerRow,
                colorSpace,
                kCGImageAlphaPremultipliedLast
                    | kCGBitmapByteOrder32Big);

        CGColorSpaceRelease(colorSpace);

        if (context == nullptr) {
            std::free(pixels);
            return nullptr;
        }

        // Match stb_image's top-left scanline orientation.
        CGContextTranslateCTM(
            context,
            0.0,
            static_cast<CGFloat>(height));

        CGContextScaleCTM(
            context,
            1.0,
            -1.0);

        CGContextSetBlendMode(
            context,
            kCGBlendModeCopy);

        CGContextDrawImage(
            context,
            CGRectMake(
                0.0,
                0.0,
                static_cast<CGFloat>(width),
                static_cast<CGFloat>(height)),
            cgImage);

        CGContextRelease(context);

        *widthOut =
            static_cast<int>(width);

        *heightOut =
            static_cast<int>(height);

        return pixels;
    }
}

extern "C" void EngineSimFreePngRgba(
    unsigned char *pixels)
{
    std::free(pixels);
}
