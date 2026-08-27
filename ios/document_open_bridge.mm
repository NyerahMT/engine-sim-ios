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
 * Engine Simulator subclasses it so we can intercept document URLs before
 * they disappear into SDL's normal event path.
 */
@interface SDLUIKitSceneDelegate
    : NSObject <UIApplicationDelegate, UIWindowSceneDelegate>

+ (NSString *)getSceneDelegateClassName;

- (UISceneConfiguration *)application:(UIApplication *)application
    configurationForConnectingSceneSession:(UISceneSession *)connectingSceneSession
    options:(UISceneConnectionOptions *)options
    API_AVAILABLE(ios(13.0));

- (void)scene:(UIScene *)scene
    willConnectToSession:(UISceneSession *)session
    options:(UISceneConnectionOptions *)connectionOptions;

@end

namespace {

std::mutex g_pendingDocumentMutex;
std::vector<std::string> g_pendingDocumentPaths;

/*
 * Write both to stderr and to the persistent Engine Simulator log.
 */
void documentBridgeLog(
    const std::string &message)
{
    std::fprintf(
        stderr,
        "[DocumentBridge] %s\n",
        message.c_str());

    std::fflush(
        stderr);

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

/*
 * Immediately consume an iOS document URL while we still have access to it.
 *
 * The file is first staged inside Engine Simulator's own temporary
 * directory. main.cpp later moves it into Documents/Custom Engines.
 */
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

        const char *errorText =
            description.UTF8String;

        documentBridgeLog(
            std::string("Read FAILED: ")
            + (
                errorText != nullptr
                    ? errorText
                    : "unknown error"
            ));

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

        const char *errorText =
            description.UTF8String;

        documentBridgeLog(
            std::string("Directory creation FAILED: ")
            + (
                errorText != nullptr
                    ? errorText
                    : "unknown error"
            ));

        return;
    }

    const char *directoryText =
        uniqueDirectory.UTF8String;

    documentBridgeLog(
        std::string("Staging directory created: ")
        + (
            directoryText != nullptr
                ? directoryText
                : "(null)"
        ));

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

        const char *errorText =
            description.UTF8String;

        documentBridgeLog(
            std::string("Staging FAILED: ")
            + (
                errorText != nullptr
                    ? errorText
                    : "unknown error"
            ));

        return;
    }

    const char *stagedPathText =
        stagedPath.UTF8String;

    if (stagedPathText == nullptr) {
        documentBridgeLog(
            "Staging FAILED: path could not be converted");

        return;
    }

    std::size_t queueSize =
        0;

    {
        std::lock_guard<std::mutex>
            lock(
                g_pendingDocumentMutex);

        g_pendingDocumentPaths.push_back(
            stagedPathText);

        queueSize =
            g_pendingDocumentPaths.size();
    }

    documentBridgeLog(
        std::string("STAGED: ")
        + stagedPathText
        + " bytes="
        + std::to_string(
            static_cast<unsigned long long>(
                data.length)));

    documentBridgeLog(
        std::string("Pending queue size=")
        + std::to_string(
            queueSize));
}

}

/*
 * Called by main.cpp.
 *
 * Swapping clears the queue atomically so each incoming document is only
 * processed once.
 */
std::vector<std::string>
engineSimTakePendingDocumentPaths()
{
    std::vector<std::string>
        result;

    {
        std::lock_guard<std::mutex>
            lock(
                g_pendingDocumentMutex);

        result.swap(
            g_pendingDocumentPaths);
    }

    /*
     * Important: log after releasing g_pendingDocumentMutex.
     */
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
 * Our custom delegate is used both as SDL's application delegate and,
 * critically, as the actual UIScene delegate.
 */
@interface EngineSimSceneDelegate
    : SDLUIKitSceneDelegate
@end

@implementation EngineSimSceneDelegate

/*
 * This is the missing piece from the previous build.
 *
 * SDL's iOS application delegate creates a UISceneConfiguration whose
 * delegateClass normally points straight back at SDLUIKitSceneDelegate.
 *
 * That meant getSceneDelegateClassName() successfully selected this class
 * as the application delegate, but the actual window scene still belonged
 * to SDLUIKitSceneDelegate. Consequently our openURLContexts: callback
 * never ran.
 *
 * Override the scene configuration and explicitly make the real UIScene use
 * EngineSimSceneDelegate.
 */
- (UISceneConfiguration *)application:(UIApplication *)application
    configurationForConnectingSceneSession:(UISceneSession *)connectingSceneSession
    options:(UISceneConnectionOptions *)options
    API_AVAILABLE(ios(13.0))
{
    (void)application;
    (void)options;

    documentBridgeLog(
        "Configuring UIScene with EngineSimSceneDelegate");

    UISceneConfiguration *configuration =
        [
            [UISceneConfiguration alloc]
            initWithName:
                @"EngineSimSceneConfiguration"
            sessionRole:
                connectingSceneSession.role
        ];

    configuration.delegateClass =
        [EngineSimSceneDelegate class];

    return configuration;
}

/*
 * Cold document launch.
 *
 * If Engine Simulator wasn't running when the user chose the .mr file,
 * iOS gives us the document in connectionOptions.URLContexts.
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

    /*
     * Let SDL perform all of its normal scene/window initialization after
     * we've captured the incoming document.
     */
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
 * Warm document open.
 *
 * This is called when Engine Simulator already exists and Files/Safari/etc.
 * asks it to open another .mr.
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
 * SDL asks this class method which application/scene delegate class it
 * should initially use.
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
