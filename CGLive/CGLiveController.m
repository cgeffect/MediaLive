//
//  CGLiveController.m
//  CGLive
//
//  Created by Jason on 2021/5/26.
//

#import "CGLiveController.h"
#import "RTMPViewController.h"

@interface CGLiveController ()

@end

@implementation CGLiveController

- (void)viewDidLoad {
    [super viewDidLoad];

    self.view.backgroundColor = UIColor.orangeColor;
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    UIStoryboard *sb = [UIStoryboard storyboardWithName:@"RTMP" bundle:nil];
    RTMPViewController *vc = sb.instantiateInitialViewController;
    [self.navigationController pushViewController:vc animated:true];
}

@end
