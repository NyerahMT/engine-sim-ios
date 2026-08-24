#import <UIKit/UIKit.h>

#include <cstdio>

@interface EngineSimViewController : UIViewController
@end

@implementation EngineSimViewController

- (void)viewDidLoad
{
    [super viewDidLoad];

    self.view.backgroundColor =
        [UIColor colorWithRed:0.035
                        green:0.035
                         blue:0.045
                        alpha:1.0];

    UILabel *titleLabel = [[UILabel alloc] init];

    titleLabel.translatesAutoresizingMaskIntoConstraints = NO;

    titleLabel.text = @"ENGINE SIM";

    titleLabel.textColor = UIColor.whiteColor;

    titleLabel.font =
        [UIFont systemFontOfSize:34.0
                          weight:UIFontWeightBold];

    titleLabel.textAlignment = NSTextAlignmentCenter;

    [self.view addSubview:titleLabel];


    UILabel *statusLabel = [[UILabel alloc] init];

    statusLabel.translatesAutoresizingMaskIntoConstraints = NO;

    statusLabel.text =
        @"iOS host online\n"
         "EngineSim core linked\n"
         "arm64 / iPhoneOS";

    statusLabel.textColor =
        [UIColor colorWithWhite:0.72
                          alpha:1.0];

    statusLabel.font =
        [UIFont monospacedSystemFontOfSize:17.0
                                   weight:UIFontWeightRegular];

    statusLabel.textAlignment = NSTextAlignmentCenter;

    statusLabel.numberOfLines = 0;

    [self.view addSubview:statusLabel];


    UILabel *milestoneLabel = [[UILabel alloc] init];

    milestoneLabel.translatesAutoresizingMaskIntoConstraints = NO;

    milestoneLabel.text =
        @"First native EngineSim iOS host";

    milestoneLabel.textColor =
        [UIColor colorWithWhite:0.50
                          alpha:1.0];

    milestoneLabel.font =
        [UIFont systemFontOfSize:13.0
                          weight:UIFontWeightMedium];

    milestoneLabel.textAlignment = NSTextAlignmentCenter;

    [self.view addSubview:milestoneLabel];


    [NSLayoutConstraint activateConstraints:@[
        [titleLabel.centerXAnchor
            constraintEqualToAnchor:self.view.centerXAnchor],

        [titleLabel.centerYAnchor
            constraintEqualToAnchor:self.view.centerYAnchor
                           constant:-70.0],

        [statusLabel.centerXAnchor
            constraintEqualToAnchor:self.view.centerXAnchor],

        [statusLabel.topAnchor
            constraintEqualToAnchor:titleLabel.bottomAnchor
                           constant:24.0],

        [milestoneLabel.centerXAnchor
            constraintEqualToAnchor:self.view.centerXAnchor],

        [milestoneLabel.topAnchor
            constraintEqualToAnchor:statusLabel.bottomAnchor
                           constant:36.0]
    ]];


    NSLog(@"=======================================");
    NSLog(@" EngineSim iOS host launched");
    NSLog(@" EngineSim core linked into executable");
    NSLog(@"=======================================");
}

- (BOOL)prefersStatusBarHidden
{
    return YES;
}

@end


@interface EngineSimAppDelegate : UIResponder <UIApplicationDelegate>

@property(nonatomic, strong) UIWindow *window;

@end


@implementation EngineSimAppDelegate

- (BOOL)application:(UIApplication *)application
    didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
    (void)application;
    (void)launchOptions;

    NSLog(@"EngineSim iOS application starting.");

    UIScreen *screen = UIScreen.mainScreen;

    self.window =
        [[UIWindow alloc] initWithFrame:screen.bounds];

    EngineSimViewController *viewController =
        [[EngineSimViewController alloc] init];

    self.window.rootViewController = viewController;

    [self.window makeKeyAndVisible];

    return YES;
}

@end


int main(int argc, char *argv[])
{
    @autoreleasepool
    {
        std::printf(
            "EngineSim iOS native entry point reached.\n"
        );

        return UIApplicationMain(
            argc,
            argv,
            nil,
            NSStringFromClass(
                [EngineSimAppDelegate class]
            )
        );
    }
}
