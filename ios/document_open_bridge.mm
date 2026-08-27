#import <UIKit/UIKit.h>
#import <Foundation/Foundation.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
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
    const std::string &message)
{
    /*
     * Keep stderr logging for Xcode / console output.
     */
    std::fprintf(
        stderr,
        "[DocumentBridge] %s\n",
        message.c_str());

    std::fflush(
        stderr);

    /*
     * Also write into the same persistent log the engine loader uses so
     * these messages are visible directly from the phone.
     */
    const char *home =
        std::getenv("HOME");

    if (
        home == nullptr
        || home[0] == '\0')
    {
        return;
    }

    const std::string logPath =
        std::string(home)
        + "/Documents/engine-sim.log";

    std::ofstream log(
        logPath,
        std::ios::out
            | std::ios::app);

    if (!log.is_open()) {
        return;
    }

    log
        << "[DocumentBridge] "
        << message
        << '\n';

    log.flush();
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

    const char *urlPath =
        url.path.UTF8String;

    documentBridgeLog(
        std::string("Received: ")
        + (
            urlPath != nullptr
                ? urlPath
                : "(null)"
        ));

    /*
     * Files, Safari, Discord, etc. may give us a security-scoped URL.
     * Consume it while access is valid and copy the contents somewhere
     * Engine Simulator owns.
     */
    const BOOL securityScoped =
        [url startAccessingSecurityScopedResource];

    documentBridgeLog(
        std::string("Security-scoped access: ")
        + (
            securityScoped
                ? "granted"
                : "not required / unavailable"
        ));

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
        NSString *description =
            readError != nil
                ? readError.localizedDescription
                : @"unknown error";

        documentBridgeLog(
            std::string("Read FAILED: ")
            + description.UTF8String);

        return;
    }

    documentBridgeLog(
        std::string("Read SUCCESS bytes=")
        + std::to_string(
            static_cast<unsigned long long>(
                data.length)));

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
        NSString *description =
            directoryError != nil
                ? directoryError.localizedDescription
                : @"unknown error";

        documentBridgeLog(
            std::string("Directory creation FAILED: ")
            + description.UTF8String);

        return;
    }

    documentBridgeLog(
        std::string("Staging directory created: ")
        + uniqueDirectory.UTF8String);

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
        NSString *description =
            writeError != nil
                ? writeError.localizedDescription
                : @"unknown error";

        documentBridgeLog(
            std::string("Staging FAILED: ")
            + description.UTF8String);

        return;
    }

    {
        std::lock_guard<std::mutex>
            lock(
                g_pendingDocumentMutex);

        g_pendingDocumentPaths.push_back(
            stagedPath.UTF8String);
    }

    documentBridgeLog(
        std::string("STAGED: ")
        + stagedPath.UTF8String
        + " bytes="
        + std::to_string(
            static_cast<unsigned long long>(
                data.length)));

    documentBridgeLog(
        std::string("Pending queue size=")
        + std::to_string(
            g_pendingDocumentPaths.size()));
}

}

/*
 * Called from main.cpp after Engine Simulator has initialized.
 *
 * Swapping clears the queue atomically so one document-open event cannot
 * trigger multiple engine loads.
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

    if (!result.empty()) {
        documentBridgeLog(
            std::string("Handing ")
            + std::to_string(
                result.size())
            + " queued document(s) to EngineSim");
    }

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
 * iOS places document URLs in connectionOptions.URLContexts.
 */
- (void)scene:(UIScene *)scene
    willConnectToSession:(UISceneSession *)session
    options:(UISceneConnectionOptions *)connectionOptions
{
    documentBridgeLog(
        std::string("Scene cold-start URL count=")
        + std::to_string(
            static_cast<unsigned long long>(
                connectionOptions.URLContexts.count)));

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
 * Warm open:
 *
 * Engine Simulator is already running/backgrounded and another .mr file is
 * opened with it.
 */
- (void)scene:(UIScene *)scene
    openURLContexts:(NSSet<UIOpenURLContext *> *)URLContexts
{
    (void)scene;

    documentBridgeLog(
        std::string("Scene warm-open URL count=")
        + std::to_string(
            static_cast<unsigned long long>(
                URLContexts.count)));

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
 * SDL asks this class method which UIScene delegate it should instantiate.
 * Return ours so document URLs are captured before SDL's callback app needs
 * them.
 */
@implementation SDLUIKitSceneDelegate (EngineSimDocumentBridge)

+ (NSString *)getSceneDelegateClassName
{
    documentBridgeLog(
        "SDL requested scene delegate; returning EngineSimSceneDelegate");

    return
        @"EngineSimSceneDelegate";
}

@end
