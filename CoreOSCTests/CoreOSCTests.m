//
//  CoreOSCTests.m
//  CoreOSCTests
//
//  Created by Mirek Rusin on 05/03/2011.
//  Copyright 2011 Inteliv Ltd. All rights reserved.
//

#import "CoreOSCTests.h"

@implementation CoreOSCTests

- (void) setUp {
  [super setUp];
  
  allocator = TestAllocatorCreate();
}

- (void) tearDown {
  STAssertTrue(TestAllocatorGetAllocationsCount(allocator) > 0, @"Allocations count should be more than 0");
  STAssertTrue(TestAllocatorGetDeallocationsCount(allocator) > 0, @"Deallocations count should be more than 0");
  STAssertEquals(TestAllocatorGetAllocationsCount(allocator), TestAllocatorGetDeallocationsCount(allocator), @"Allocations/deallocations mismatch");
  
  if (TestAllocatorGetAllocationsCount(allocator) != TestAllocatorGetDeallocationsCount(allocator))
    TestAllocatorPrintAddressesAndBacktraces(allocator);

  CFRelease(allocator);
  
  [super tearDown];
}

- (void) testExample {
  OSCRef osc = OSCCreate(allocator, CFSTR("127.0.0.1"), CFSTR("60000"));
  CFIndex testFloatIndex = OSCAddressesAppendWithString(osc, CFSTR("/test/float"));
  OSCSendFloat(osc, testFloatIndex, 3.14);
  OSCRelease(osc);
}

@end
