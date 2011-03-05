//
//  CoreOSCTests.h
//  CoreOSCTests
//
//  Created by Mirek Rusin on 05/03/2011.
//  Copyright 2011 Inteliv Ltd. All rights reserved.
//

#import <SenTestingKit/SenTestingKit.h>
#import "TestAllocator/TestAllocator.h"
#import "CoreOSC.h"

@interface CoreOSCTests : SenTestCase {
@private
  CFAllocatorRef allocator;
}

@end
