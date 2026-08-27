#import <UIKit/UIKit.h>
#import <Foundation/Foundation.h>

#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

/*
 * SDL provides this class at runtime.
 *
 * We redeclare the interface here so Engine Simulator can subclass SDL's
 * scene delegate and intercept iOS document-open URLs before SDL has a
 * chance to lose them during a cold launch.
 */
@interface SDLUIKitSceneDelegate
    : NSObject <UIApplicationDelegate, UIWindowSceneDelegate>

+ (NSString *)getSceneDelegateClassName;

- (void)scene:(UIScene *)scene
    willConnectToSession:(UISceneSession *)session
    options:(UISceneConnectionOptions *)connectionOptions;

@end

namespace {

std::mutex g_pendingDocumentMutex;
std::vector<std::string> g_pendingDocumentPaths;

void documentBridgeLog(
    const char *message)
{
    std::fprintf(
        stderr,
        "[DocumentBridge] %s\n",
        message);

    std::fflush(
        stderr);
}

void queueDocumentURL(
    NSURL *url)
{
    if (
        url == nil
        || !url.isFileURL)
    {
        documentBridgeLog(
            "Ignoring non-file URL");

        return;
    }

    std::fprintf(
        stderr,
        "[DocumentBridge] Received: %s\n",
        url.path.UTF8String);

    std::fflush(
        stderr);

    /*
     * Files, Safari, Discord, etc. may give us a security-scoped URL.
     * We need to consume it while access is valid and copy the contents
     * somewhere owned by Engine Simulator.
     */
    const BOOL securityScoped =
        [url startAccessingSecurityScopedResource];

    NSError *readError =
        nil;

    NSData *data =
        [
            NSData
            dataWithContentsOfURL:
                url
            options:
                NSDataReadingMappedIfSafe
            error:
                &readError
        ];

    if (securityScoped) {
        [url stopAccessingSecurityScopedResource];
    }

    if (
        data == nil
        || readError != nil)
    {
        std::fprintf(
            stderr,
            "[DocumentBridge] Read FAILED: %s\n",
            readError != nil
                ? readError
                    .localizedDescription
                    .UTF8String
                : "unknown error");

        std::fflush(
            stderr);

        return;
    }

    NSString *temporaryRoot =
        [
            NSTemporaryDirectory()
            stringByAppendingPathComponent:
                @"EngineSimDocumentImports"
        ];

    NSString *uniqueDirectory =
        [
            temporaryRoot
            stringByAppendingPathComponent:
                [NSUUID UUID].UUIDString
        ];

    NSError *directoryError =
        nil;

    const BOOL directoryCreated =
        [
            [NSFileManager defaultManager]
            createDirectoryAtPath:
                uniqueDirectory
            withIntermediateDirectories:
                YES
            attributes:
                nil
            error:
                &directoryError
        ];

    if (!directoryCreated) {
        std::fprintf(
            stderr,
            "[DocumentBridge] Directory creation FAILED: %s\n",
            directoryError != nil
                ? directoryError
                    .localizedDescription
                    .UTF8String
                : "unknown error");

        std::fflush(
            stderr);

        return;
    }

    NSString *filename =
        url.lastPathComponent;

    if (
        filename == nil
        || filename.length == 0)
    {
        filename =
            @"ImportedEngine.mr";
    }

    NSString *stagedPath =
        [
            uniqueDirectory
            stringByAppendingPathComponent:
                filename
        ];

    NSError *writeError =
        nil;

    const BOOL written =
        [
            data
            writeToFile:
                stagedPath
            options:
                NSDataWritingAtomic
            error:
                &writeError
        ];

    if (!written) {
        std::fprintf(
            stderr,
            "[DocumentBridge] Staging FAILED: %s\n",
            writeError != nil
                ? writeError
                    .localizedDescription
                    .UTF8String
                : "unknown error");

        std::fflush(
            stderr);

        return;
    }

    {
        std::lock_guard<std::mutex>
            lock(
                g_pendingDocumentMutex);

        g_pendingDocumentPaths.push_back(
            stagedPath.UTF8String);
    }

    std::fprintf(
        stderr,
        "[DocumentBridge] STAGED: %s (%lu bytes)\n",
        stagedPath.UTF8String,
        static_cast<unsigned long>(
            data.length));

    std::fflush(
        stderr);
}

}

/*
 * Called from main.cpp after Engine Simulator has initialized.
 *
 * Swapping rather than copying also clears the queue atomically, preventing
 * the same iOS document-open event from loading the engine more than once.
 */
std::vector<std::string>
engineSimTakePendingDocumentPaths()
{
    std::lock_guard<std::mutex>
        lock(
            g_pendingDocumentMutex);

    std::vector<std::string>
        result;

    result.swap(
        g_pendingDocumentPaths);

    return result;
}

/*
 * Custom scene delegate used by Engine Simulator.
 */
@interface EngineSimSceneDelegate
    : SDLUIKitSceneDelegate
@end

@implementation EngineSimSceneDelegate

/*
 * Cold launch:
 *
 * Engine Simulator wasn't running when the user selected an .mr file.
 * iOS puts the document URL in connectionOptions.
 */
- (void)scene:(UIScene *)scene
    willConnectToSession:(UISceneSession *)session
    options:(UISceneConnectionOptions *)connectionOptions
{
    std::fprintf(
        stderr,
        "[DocumentBridge] Scene cold-start URL count: %lu\n",
        static_cast<unsigned long>(
            connectionOptions.URLContexts.count));

    std::fflush(
        stderr);

    for (
        UIOpenURLContext *context
        in connectionOptions.URLContexts)
    {
        queueDocumentURL(
            context.URL);
    }

    [
        super
        scene:
            scene
        willConnectToSession:
            session
        options:
            connectionOptions
    ];
}

/*
 * Warm launch:
 *
 * Engine Simulator is already alive/backgrounded and another .mr file is
 * opened with it.
 */
- (void)scene:(UIScene *)scene
    openURLContexts:(NSSet<UIOpenURLContext *> *)URLContexts
{
    (void)scene;

    std::fprintf(
        stderr,
        "[DocumentBridge] Scene warm-open URL count: %lu\n",
        static_cast<unsigned long>(
            URLContexts.count));

    std::fflush(
        stderr);

    for (
        UIOpenURLContext *context
        in URLContexts)
    {
        queueDocumentURL(
            context.URL);
    }
}

@end

/*
 * SDL asks this method which scene-delegate class it should instantiate.
 * Return ours so document URLs are captured before the SDL callback
 * application needs them.
 */
@implementation SDLUIKitSceneDelegate (EngineSimDocumentBridge)

+ (NSString *)getSceneDelegateClassName
{
    return
        @"EngineSimSceneDelegate";
}

@end
