//
//  UserDefaults.h
//  Mariani
//
//  Created by sh95014 on 1/2/22.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface UserDefaults : NSObject

+ (instancetype)sharedInstance;

@property (nonatomic) NSURL *recordingsFolder;
@property (nonatomic) NSURL *screenshotsFolder;

@end

NS_ASSUME_NONNULL_END
