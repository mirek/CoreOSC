//
// CoreOSC.c
// CoreOSC Framework
//
// Created by Mirek Rusin on 05/03/2011.
// Copyright 2011 Inteliv Ltd. All rights reserved.
//

#include "CoreOSC.h"

#pragma mark Internal string helper for fast UTF8 buffer access

inline __OSCUTF8String __OSCUTF8StringMake(CFAllocatorRef allocator, CFStringRef string) {
  __OSCUTF8String utf8String;
  utf8String.allocator = allocator;
  utf8String.string = string;
  utf8String.maximumSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(string), kCFStringEncodingUTF8) + 1;
  if ((utf8String.pointer = (const char *)CFStringGetCStringPtr(string, kCFStringEncodingUTF8))) {
    utf8String.buffer = NULL;
  } else {
    utf8String.buffer = CFAllocatorAllocate(allocator, utf8String.maximumSize, 0);
    if (utf8String.buffer) {
      CFStringGetCString(string, (char *)utf8String.buffer, utf8String.maximumSize, kCFStringEncodingUTF8);
    }
  }
  return utf8String;
}

inline const char *__OSCUTF8StringGetBuffer(__OSCUTF8String utf8String) {
  return utf8String.pointer ? utf8String.pointer : utf8String.buffer;
}

inline CFIndex __OSCUTF8StringGetMaximumSize(__OSCUTF8String utf8String) {
  return utf8String.maximumSize;
}

inline void __OSCUTF8StringDestroy(__OSCUTF8String utf8String) {
  if (utf8String.buffer)
    CFAllocatorDeallocate(utf8String.allocator, (void *)utf8String.buffer);
}

void __OSCBufferAppendAddressWithString(void *buffer, CFStringRef name, int *i) {
  char address[OSC_STATIC_ADDRESS_LENGTH];
  memset(address, 0, OSC_STATIC_ADDRESS_LENGTH);
  CFStringGetCString(name, address, OSC_STATIC_ADDRESS_LENGTH, kCFStringEncodingUTF8);
  // TODO: malloc if false
  unsigned long n = __OSCGet32BitAlignedLength(strlen(address) + 1);
  if (n <= OSC_STATIC_ADDRESS_LENGTH)
    __OSCBufferAppend(buffer, address, n, *i);
}

#pragma mark Internal, diagnostics

// Internal, for diagnostics, print osc buffer where the top line are chars,
// bottom one hex codes:
//
//  /  t  e  s  t  3 __ __  ,  f  f __  O  M  m  @  L  I  y  p
// 2f 74 65 73 74 33 00 00 2c 66 66 00 4f 4d 6d 40 4c 49 79 70
//
void __OSCBufferPrint(char *buffer, int length) {
  for (int i = 0; i < length; i++)
    if (buffer[i] > 0x1f)
      printf("  %c", buffer[i]);
    else
      printf(" __");
  printf("\n");
  for (int i = 0; i < length; i++)
    printf(" %02x", (unsigned char)buffer[i]);
  printf("\n");
}

#pragma mark OSC API

// Iterate over all keys in the cache. Fetch first inserted value and send it.
// Please note this callback behaves the same for different osc schedule modes,
// the only difference is how the values are being added (insert or replace).
inline void __OSCRunLoopTimerCallBack(CFRunLoopTimerRef timer, void *info) {
  OSCRef osc = info;
  if (osc) {
    if (osc->cache) {
      CFIndex n = CFDictionaryGetCount(osc->cache);
      if (n > 0) {
        CFTypeRef *keys = CFAllocatorAllocate(osc->allocator, sizeof(CFTypeRef) * n, 0);
        CFTypeRef *arrays = CFAllocatorAllocate(osc->allocator, sizeof(CFTypeRef) * n, 0);
        CFDictionaryGetKeysAndValues(osc->cache, keys, arrays);
        for (CFIndex i = 0; i < n; i++) {
          CFStringRef key = keys[i];
          CFMutableArrayRef array = (CFMutableArrayRef)arrays[i];
          if (CFArrayGetCount(array) > 0) { // If there is a value in the array, remove it and send
            CFTypeRef value = CFRetain(CFArrayGetValueAtIndex(array, 0));
            CFArrayRemoveValueAtIndex(array, 0);
//            printf("sending: ");
//            CFShow(key);
//            CFShow(value);
//            printf("\n");
            OSCSendValue(osc, key, value);
            CFRelease(value);
          }
        }
        CFAllocatorDeallocate(osc->allocator, arrays);
        CFAllocatorDeallocate(osc->allocator, keys);
      }
    }
  }
}

inline OSCRef OSCCreate(CFAllocatorRef allocator, CFStringRef host, CFStringRef port, CFTimeInterval timeInterval) {
  OSCRef osc = CFAllocatorAllocate(allocator, sizeof(OSC), 0);
  if (osc) {
    osc->allocator = allocator ? CFRetain(allocator) : NULL;
    osc->retainCount = 1;
    osc->addressesIndex = 0;
    osc->addresses = CFAllocatorAllocate(allocator, OSC_ADDRESSES_LENGTH, 0);
    osc->servinfo = NULL;
    osc->sockfd = 0;

    __OSCUTF8String utf8Host = __OSCUTF8StringMake(allocator, host);
    __OSCUTF8String utf8Port = __OSCUTF8StringMake(allocator, port);
    
    memset(&osc->hints, 0, sizeof(osc->hints));
    osc->hints.ai_family = AF_UNSPEC;
    osc->hints.ai_socktype = SOCK_DGRAM;
    
    if ((osc->rv = getaddrinfo(__OSCUTF8StringGetBuffer(utf8Host), __OSCUTF8StringGetBuffer(utf8Port), &osc->hints, &osc->servinfo)) != 0) {
      // TODO: gai_strerror(osc->rv)
      osc = OSCRelease(osc);
    } else {
      
      // Loop through all the results and make a socket
      for (osc->p = osc->servinfo; osc->p != NULL; osc->p = osc->p->ai_next) {
        if ((osc->sockfd = socket(osc->p->ai_family, osc->p->ai_socktype, osc->p->ai_protocol)) == -1)
          continue;
        break;
      }
      
      if (osc)
        if (osc->p == NULL)
          osc = OSCRelease(osc); // TODO: Failed to bind socket
    }
    
    __OSCUTF8StringDestroy(utf8Port);
    __OSCUTF8StringDestroy(utf8Host);
    
    // Create the dictionary
    if (osc)
      osc->cache = CFDictionaryCreateMutable(osc->allocator, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    
    if (osc)
      if (timeInterval > 0.0)
        OSCActivateRunLoopTimer(osc, timeInterval);
  }
  return osc;
}

inline OSCRef OSCRetain(OSCRef osc) {
  if (osc)
    osc->retainCount++;
  return osc;
}

inline OSCRef OSCRelease(OSCRef osc) {
  if (osc) {
    if (--osc->retainCount == 0) {
      CFAllocatorRef allocator = osc->allocator;
      
      OSCDeactivateRunLoopTimer(osc);
      
      if (osc->cache) {
        CFRelease(osc->cache);
        osc->cache = NULL;
      }
      
      if (osc->addresses) {
        OSCAddressesClear(osc);
        CFAllocatorDeallocate(allocator, osc->addresses);
      }
      
      if (osc->servinfo)
        freeaddrinfo(osc->servinfo);
      
      if (osc->sockfd)
        close(osc->sockfd);
      
      CFAllocatorDeallocate(allocator, osc);
      osc = NULL;
      
      if (allocator)
        CFRelease(allocator);
    }
  }
  return osc;
}

#pragma mark Run Loop Timer

inline void OSCActivateRunLoopTimer(OSCRef osc, CFTimeInterval timeInterval) {
  if (osc) {
    if (osc->runLoopTimer)
      OSCDeactivateRunLoopTimer(osc);
    CFRunLoopTimerContext context = { 0, osc, NULL, NULL, NULL };
    osc->runLoopTimer = CFRunLoopTimerCreate(osc->allocator, CFAbsoluteTimeGetCurrent(), timeInterval, 0, 0, __OSCRunLoopTimerCallBack, &context);
    CFRunLoopAddTimer(CFRunLoopGetCurrent(), osc->runLoopTimer, kCFRunLoopCommonModes);
  }
}

inline void OSCDeactivateRunLoopTimer(OSCRef osc) {
  if (osc) {
    if (osc->runLoopTimer) {
      CFRunLoopTimerInvalidate(osc->runLoopTimer);
      CFRelease(osc->runLoopTimer);
      osc->runLoopTimer = NULL;
    }
  }
}
  
#pragma mark Addresses

inline bool OSCAddressesIsIndexInBounds(OSCRef osc, CFIndex index) {
  return index >= 0 && index < osc->addressesIndex;
}

inline bool OSCAddressesIsIndexOutOfBounds(OSCRef osc, CFIndex index) {
  return !OSCAddressesIsIndexInBounds(osc, index);
}

inline CFIndex OSCAddressesAppendWithString(OSCRef osc, CFStringRef string) {
  __OSCAddress address;
  address.length = __OSCGet32BitAlignedLength(CFStringGetLength(string) + 1);
  address.buffer = CFAllocatorAllocate(osc->allocator, address.length, 0);
  memset(address.buffer, 0, address.length);
  CFStringGetCString(string, address.buffer, address.length, kCFStringEncodingASCII);
  osc->addresses[osc->addressesIndex++] = address;
  return osc->addressesIndex - 1;
}

inline void OSCAddressesClear(OSCRef osc) {
  while (--osc->addressesIndex >= 0)
    CFAllocatorDeallocate(osc->allocator, osc->addresses[osc->addressesIndex].buffer);
}

#pragma mark Sending

inline OSCResult OSCSetValue(OSCRef osc, CFStringRef name, CFTypeRef value) {
  OSCResult result = kOSCResultNotAllocatedError;
  if (osc && name && value) {
    if (osc->cache) {
      CFMutableArrayRef array = (CFMutableArrayRef)CFDictionaryGetValue(osc->cache, name);
      if (!array) {
        array = CFArrayCreateMutable(osc->allocator, 0, &kCFTypeArrayCallBacks);
        CFDictionarySetValue(osc->cache, name, array);
        CFRelease(array); // Retained by dictionary, we can use it normally
      }
      CFArraySetValueAtIndex(array, 0, value);
      //      CFArrayAppendValue(array, value);
//      CFShow(osc->cache);
    }
  }
  return result;
}

// Force Float32 number if not of float type
inline OSCResult OSCSetNumberAsFloat32(OSCRef osc, CFStringRef name, CFNumberRef value) {
  OSCResult result = kOSCResultNotAllocatedError;
  if (osc && name && value) {
    if (CFNumberIsFloatType(value)) {
      result = OSCSetValue(osc, name, value);
    } else {
      Float32 value_ = 0.0;
      CFNumberGetValue(value, kCFNumberFloat32Type, &value_);
      CFNumberRef number = CFNumberCreate(osc->allocator, kCFNumberFloat32Type, &value_);
      result = OSCSetValue(osc, name, number);
      CFRelease(number);
    }
  }
  return result;
}

inline OSCResult OSCSendValue(OSCRef osc, CFStringRef name, CFTypeRef value) {
  OSCResult result = kOSCResultNotAllocatedError;
  if (osc && name && value) {
    CFTypeID valueId = CFGetTypeID(value);
    if (valueId == CFNumberGetTypeID()) {
      if (CFNumberIsFloatType(value)) {
        Float32 float32Value = 0.0;
        CFNumberGetValue(value, kCFNumberFloat32Type, &float32Value);
        result = OSCSendFloat32(osc, name, float32Value);
      } else {
        SInt32 sint32Value = 0;
        CFNumberGetValue(value, kCFNumberSInt32Type, &sint32Value);
        result = OSCSendSInt32(osc, name, sint32Value);
      }
    }
//    else if (valueId == CFArrayGetTypeID()) {
//      
//    }
  }
  return result;
}

inline OSCResult OSCSendRawBuffer(OSCRef osc, const void *buffer, CFIndex length) {
  OSCResult result = kOSCResultNotAllocatedError;
  if (osc) {
    if ((result = (OSCResult)sendto(osc->sockfd, buffer, length, 0, osc->p->ai_addr, osc->p->ai_addrlen)) == -1) {
      // TODO: Error
    }
  }
  return result;
}

inline OSCResult OSCSendRawBufferWithData(OSCRef osc, CFDataRef data) {
  OSCResult result = kOSCResultNotAllocatedError;
  if (osc)
    if (data)
      result = OSCSendRawBuffer(osc, CFDataGetBytePtr(data), CFDataGetLength(data));
  return result;
}

inline OSCResult OSCSendTrue(OSCRef osc, CFStringRef name) {
  OSCResult result = kOSCResultNotAllocatedError;
  if (osc && name) {
    char buffer[OSC_STATIC_TRUE_PACKET_LENGTH];
    int i = 0;
    __OSCBufferAppendAddressWithString(buffer, name, &i);
    __OSCBufferAppend(buffer, ",T\0\0", 4, i);
    __OSCBufferSend(osc, buffer, i, result);
  }
  return result;
}

inline OSCResult OSCSendFalse(OSCRef osc, CFStringRef name) {
  OSCResult result = kOSCResultNotAllocatedError;
  if (osc && name) {
    char buffer[OSC_STATIC_FALSE_PACKET_LENGTH];
    int i = 0;
    __OSCBufferAppendAddressWithString(buffer, name, &i);
    __OSCBufferAppend(buffer, ",F\0\0", 4, i);
    __OSCBufferSend(osc, buffer, i, result);
  }
  return result;
}

inline OSCResult OSCSendBool(OSCRef osc, CFStringRef name, bool value) {
  OSCResult result = kOSCResultNotAllocatedError;
  if (osc && name)
    result = value ? OSCSendTrue(osc, name) : OSCSendFalse(osc, name);
  return result;
}

inline OSCResult OSCSendSInt32(OSCRef osc, CFStringRef name, SInt32 value) {
  OSCResult result = kOSCResultNotAllocatedError;
  if (osc && name) {
    char buffer[OSC_STATIC_SINT32_PACKET_LENGTH];
    SInt32 swappedValue = CFSwapInt32HostToBig(value);
    int i = 0;
    __OSCBufferAppendAddressWithString(buffer, name, &i);
    __OSCBufferAppend(buffer, ",i\0\0", 4, i);
    __OSCBufferAppend(buffer, &swappedValue, 4, i);
    __OSCBufferSend(osc, buffer, i, result);
  }
  return result;
}

inline OSCResult OSCSendFloat32(OSCRef osc, CFStringRef name, Float32 value) {
  OSCResult result = kOSCResultNotAllocatedError;
  if (osc) {
    char buffer[OSC_STATIC_FLOAT32_PACKET_LENGTH];
    CFSwappedFloat32 swappedValue = CFConvertFloat32HostToSwapped(value);
    int i = 0;
    __OSCBufferAppendAddressWithString(buffer, name, &i);
    __OSCBufferAppend(buffer, ",f\0\0", 4, i);
    __OSCBufferAppend(buffer, &swappedValue, 4, i);
    __OSCBufferSend(osc, buffer, i, result);
  }
  return result;
}

inline OSCResult OSCSendFloats32(OSCRef osc, CFStringRef name, const Float32 *values, CFIndex n) {
  OSCResult result = kOSCResultNotAllocatedError;
  if (osc && name && values && n > 0) {
    char buffer[OSC_STATIC_FLOATS32_PACKET_LENGTH];
    char type[OSC_STATIC_FLOATS32_PACKET_HEADER_LENGTH];
    memset(type, 0, OSC_STATIC_FLOATS32_PACKET_HEADER_LENGTH);
    type[0] = ',';
    memset(type + 1, 'f', n);
    int i = 0;
    __OSCBufferAppendAddressWithString(buffer, name, &i);
    __OSCBufferAppend(buffer, type, __OSCGet32BitAlignedLength(1 + n), i);
    for (int i = 0; i < n; i++) {
      CFSwappedFloat32 swappedValue = CFConvertFloat32HostToSwapped(values[i]);
      __OSCBufferAppend(buffer, &swappedValue, 4, i);
    }
    __OSCBufferSend(osc, buffer, i, result);
  }
  return result;
}

inline OSCResult OSCSendCString(OSCRef osc, CFStringRef name, const UInt8 *value) {
  OSCResult result = kOSCResultNotAllocatedError;
  if (osc && name && value) {
    char buffer[OSC_STATIC_STRING_PACKET_LENGTH];
    unsigned long length = strlen((const char *)value);
    int i = 0;
    __OSCBufferAppendAddressWithString(buffer, name, &i);
    __OSCBufferAppend(buffer, ",s\0\0", 4, i);
    __OSCBufferAppend(buffer, value, length, i);
    __OSCBufferSend(osc, buffer, i, result);
  }
  return result;
}

inline OSCResult OSCSendString(OSCRef osc, CFStringRef name, CFStringRef value) {
  OSCResult result = kOSCResultNotAllocatedError;
  if (osc && name && value) {
    __OSCUTF8String utf8Value = __OSCUTF8StringMake(osc->allocator, value);
    result = OSCSendCString(osc, name, (const UInt8 *)__OSCUTF8StringGetBuffer(utf8Value));
    __OSCUTF8StringDestroy(utf8Value);
  }
  return result;
}

OSCResult OSCSendNumbersAsFloats32(OSCRef osc, CFStringRef name, const CFNumberRef *values, CFIndex n) {
  OSCResult result = kOSCResultNotAllocatedError;
  if (osc && name && values && n > 0) {
    Float32 *values_ = CFAllocatorAllocate(osc->allocator, n, 0);
    if (values_) {
      for (CFIndex i = 0; i < n; i++)
        CFNumberGetValue(values[i], kCFNumberFloat32Type, &values_[i]);
      result = OSCSendFloats32(osc, name, values_, n);
      CFAllocatorDeallocate(osc->allocator, values_);
    }
  }
  return result;
}

OSCResult OSCSendNumberAsSInt32(OSCRef osc, CFStringRef name, CFNumberRef value) {
  OSCResult result = kOSCResultNotAllocatedError;
  if (osc && name && value) {
    SInt32 value_ = 0;
    CFNumberGetValue(value, kCFNumberSInt32Type, &value_);
    result = OSCSendSInt32(osc, name, value_);
  }
  return result;
}

OSCResult OSCSendNumberAsFloat32(OSCRef osc, CFStringRef name, CFNumberRef value) {
  OSCResult result = kOSCResultNotAllocatedError;
  if (osc && name && value) {
    Float32 value_ = 0;
    CFNumberGetValue(value, kCFNumberFloat32Type, &value_);
    result = OSCSendFloat32(osc, name, value_);
  }
  return result;
}

