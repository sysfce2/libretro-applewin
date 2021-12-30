//
//  PreferencesViewController.m
//  Mariani
//
//  Created by Steven Huang on 12/30/21.
//

#import "PreferencesViewController.h"

@implementation PreferencesViewController

- (void)viewDidLoad {
    [super viewDidLoad];

    self.preferredContentSize = self.view.frame.size;
}

- (void)viewDidAppear {
    [super viewDidAppear];
    
    self.view.window.title = self.title;
}

@end
