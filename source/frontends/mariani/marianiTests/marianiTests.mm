//
//  marianiTests.mm
//  marianiTests
//
//  Created by sh95014 on 12/27/21.
//

#import <XCTest/XCTest.h>

#include "TestCPU6502.cpp"

@interface marianiTests : XCTestCase

@end

@implementation marianiTests

- (void)setUp {
    [super setUp];
    init();
    reset();
}

- (void)tearDown {
    [super tearDown];
}

- (void)testGH264 {
    XCTAssertEqual(GH264_test(), 0, @"GH264");
}

- (void)testGH271 {
    XCTAssertEqual(GH264_test(), 0, @"GH271");
}

- (void)testGH278 {
    XCTAssertEqual(GH264_test(), 0, @"GH278");
}

- (void)testGH282 {
    XCTAssertEqual(GH264_test(), 0, @"GH282");
}

- (void)testGH291 {
    XCTAssertEqual(GH264_test(), 0, @"GH291");
}

- (void)testSyncEvents {
    XCTAssertEqual(SyncEvents_test(), 0, @"SyncEvents");
}

@end
