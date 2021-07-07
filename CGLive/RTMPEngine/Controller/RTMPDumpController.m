//
//  RTMPDumpController.m
//  FFmpegiOS
//
//  Created by Jason on 2021/4/19.
//  Copyright © 2021 Jason. All rights reserved.
//

#import "RTMPDumpController.h"
#import "simplest_librtmp_send264.h"

@interface RTMPDumpController ()

@end

@implementation RTMPDumpController

- (void)viewDidLoad {
    [super viewDidLoad];
    
    self.view.backgroundColor = UIColor.whiteColor;
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    NSString* inPath = [[NSBundle mainBundle] pathForResource:@"cuc_ieschool" ofType:@"h264"];
    load_file((char *)inPath.UTF8String);
}


@end
