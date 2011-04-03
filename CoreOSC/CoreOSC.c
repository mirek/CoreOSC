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

#pragma mark Data related functions - packet construction

inline void OSCDataAppendZeroBytesFor32Alignment(CFMutableDataRef data) {
  CFDataAppendBytes(data, (const UInt8 *)"\0\0\0", (4 - CFDataGetLength(data) % 4) % 4);
}

inline void OSCDataAppendString(CFAllocatorRef allocator, CFMutableDataRef data, CFStringRef value) {
  CFIndex bufferLength = CFStringGetMaximumSizeForEncoding(CFStringGetLength(value), kCFStringEncodingUTF8);
  CFIndex usedBufferLength = 0;
  UInt8 *buffer = CFAllocatorAllocate(allocator, bufferLength, 0);
  CFStringGetBytes(value, CFRangeMake(0, CFStringGetLength(value)), kCFStringEncodingUTF8, 0, 0, buffer, bufferLength, &usedBufferLength);
  CFDataAppendBytes(data, buffer, usedBufferLength);
  CFDataAppendBytes(data, (const UInt8 *)"\0", 1);
  OSCDataAppendZeroBytesFor32Alignment(data);
  CFAllocatorDeallocate(allocator, buffer);
}

inline void OSCDataAppendSInt32(CFMutableDataRef data, SInt32 value) {
  uint32_t swapped = CFSwapInt32HostToBig(value);
  CFDataAppendBytes(data, (const UInt8 *)&swapped, sizeof(uint32_t));
}

inline void OSCDataAppendNumberAsSInt32(CFMutableDataRef data, CFNumberRef value) {
  SInt32 value_ = 0;
  CFNumberGetValue(value, kCFNumberSInt32Type, &value_);
  OSCDataAppendSInt32(data, value_);
}

inline void OSCDataAppendNumberAsFloat32(CFMutableDataRef data, CFNumberRef value) {
  Float32 value_ = 0;
  CFNumberGetValue(value, kCFNumberFloat32Type, &value_);
  CFSwappedFloat32 swapped = CFConvertFloat32HostToSwapped(value_);
  CFDataAppendBytes(data, (const UInt8 *)&swapped, sizeof(CFSwappedFloat32));
}

inline void OSCDataAppendMessage(CFAllocatorRef allocator, CFMutableDataRef data, CFStringRef name, CFTypeRef value) {
  CFTypeID valueType = CFGetTypeID(value);
  if (valueType == CFNumberGetTypeID()) {
    if (CFNumberIsFloatType(value)) {
      OSCDataAppendString(allocator, data, name);
      OSCDataAppendString(allocator, data, CFSTR(",f"));
      OSCDataAppendNumberAsFloat32(data, value);
    } else {
      OSCDataAppendString(allocator, data, name);
      OSCDataAppendString(allocator, data, CFSTR(",i"));
      OSCDataAppendNumberAsSInt32(data, value);
    }
  } else if (valueType == CFBooleanGetTypeID()) {
    OSCDataAppendString(allocator, data, name);
    if (CFBooleanGetValue(value))
      OSCDataAppendString(allocator, data, CFSTR(",T"));
    else
      OSCDataAppendString(allocator, data, CFSTR(",F"));
  } else if (valueType == CFStringGetTypeID()) {
    OSCDataAppendString(allocator, data, name);
    OSCDataAppendString(allocator, data, value);
  } else if (valueType == CFDataGetTypeID()) {
    OSCDataAppendString(allocator, data, name);
    OSCDataAppendString(allocator, data, CFSTR(",b"));
    OSCDataAppendSInt32(data, CFDataGetLength(value));
    CFDataAppendBytes(data, CFDataGetBytePtr(value), CFDataGetLength(value));
    OSCDataAppendZeroBytesFor32Alignment(data);
  }
}

//inline void OSCDataAppendTimeTagWithAboluteTimeInterval(CFDataRef data, CFTimeInterval timeInterval) {
//  const unsigned long long EPOCH = 2208988800ULL;
//  const unsigned long long NTP_SCALE_FRAC = 4294967295ULL;
//  
//  unsigned long long tv_to_ntp(struct timeval tv)
//  {
//    unsigned long long tv_ntp, tv_usecs;
//    
//    tv_ntp = tv.tv_sec + EPOCH;
//    tv_usecs = (NTP_SCALE_FRAC * tv.tv_usec) / 1000000UL;
//    
//    return (tv_ntp << 32) | tv_usecs;
//  }
//  
//}

inline void OSCDataAppendImmediateTimeTag(CFMutableDataRef data) {
  uint64_t immediate = CFSwapInt64HostToBig(1);
  CFDataAppendBytes(data, (const UInt8 *)&immediate, 8);
}

inline void OSCDataAppendData(CFMutableDataRef data, CFDataRef value) {
  CFDataAppendBytes(data, CFDataGetBytePtr(value), CFDataGetLength(value));
  OSCDataAppendZeroBytesFor32Alignment(data);
}

inline void OSCDataAppendBundleWithDictionary(CFAllocatorRef allocator, CFMutableDataRef data, CFDictionaryRef keyValuePairs) {
  if (CFGetTypeID(keyValuePairs) == CFDictionaryGetTypeID()) {
    OSCDataAppendString(allocator, data, CFSTR("#bundle"));
    OSCDataAppendImmediateTimeTag(data);
    CFIndex n = CFDictionaryGetCount(keyValuePairs);
    CFTypeRef *keys = CFAllocatorAllocate(allocator, sizeof(CFTypeRef) * n, 0);
    CFTypeRef *values = CFAllocatorAllocate(allocator, sizeof(CFTypeRef) * n, 0);
    CFDictionaryGetKeysAndValues(keyValuePairs, keys, values);
    for (CFIndex i = 0; i < n; i++) {
      CFMutableDataRef message = CFDataCreateMutable(allocator, 0);
      OSCDataAppendMessage(allocator, message, keys[i], values[i]);
      OSCDataAppendSInt32(data, CFDataGetLength(message));
      OSCDataAppendData(data, message);
      CFRelease(message);
    }
    CFAllocatorDeallocate(allocator, values);
    CFAllocatorDeallocate(allocator, keys);
  }
}

#pragma mark OSC API

// Iterate over all keys in the cache. Fetch first inserted value and send it.
// Please note this callback behaves the same for different osc schedule modes,
// the only difference is how the values are being added (insert or replace).
inline void __OSCRunLoopTimerCallBack(CFRunLoopTimerRef timer, void *info) {
  OSCRef osc = info;
  
  CFMutableDictionaryRef dict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
  
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
//            OSCSendValue(osc, key, value);
            
            CFDictionarySetValue(dict, key, value);
            
//            printf("sending");
//            CFShow(value);
            CFRelease(value);
//            break;
          }
        }
        CFAllocatorDeallocate(osc->allocator, arrays);
        CFAllocatorDeallocate(osc->allocator, keys);
      }
    }
  }
  
//  CFShow(dict);
  
  if (CFDictionaryGetCount(dict) > 0) {
    CFMutableDataRef data = CFDataCreateMutable(NULL, 0);
    OSCDataAppendBundleWithDictionary(NULL, data, dict);
    
    OSCSendRawBufferWithData(osc, data);
    
//    __OSCBufferPrint((char *)CFDataGetBytePtr(data), CFDataGetLength(data));
  }
  
  CFRelease(dict);
  
}

inline OSCRef OSCCreateWithUserInfo(CFAllocatorRef allocator, void *userInfo) {
  OSCRef osc = CFAllocatorAllocate(allocator, sizeof(OSC), 0);
  if (osc) {
    osc->allocator = allocator ? CFRetain(allocator) : NULL;
    osc->retainCount = 1;
    osc->userInfo = userInfo;
    osc->runLoopTimer = NULL;
    osc->sockfd = 0;
    osc->p = NULL;
    osc->cache = CFDictionaryCreateMutable(osc->allocator, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
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

inline struct addrinfo *OSCConnect(OSCRef osc, CFStringRef host, UInt16 port) {
  osc->servinfo = NULL;
  osc->sockfd = 0;
  
  char hostBuffer[256];
  char portBuffer[256];
  
  CFStringGetCString(host, hostBuffer, sizeof(hostBuffer), kCFStringEncodingUTF8);
  sprintf(portBuffer, "%i", port);
  
//  printf("OSCConnect -> '%s:%s'\n", hostBuffer, portBuffer);
  
  memset(&osc->hints, 0, sizeof(osc->hints));
  osc->hints.ai_family = AF_UNSPEC;
  osc->hints.ai_socktype = SOCK_DGRAM;
  
  if ((osc->rv = getaddrinfo(hostBuffer, portBuffer, &osc->hints, &osc->servinfo)) != 0) {
    printf("gai_strerror(osc->rv)\n");
    // TODO: gai_strerror(osc->rv)
  } else {
    
    // Loop through all the results and make a socket
    for (osc->p = osc->servinfo; osc->p != NULL; osc->p = osc->p->ai_next) {
      if ((osc->sockfd = socket(osc->p->ai_family, osc->p->ai_socktype, osc->p->ai_protocol)) == -1)
        continue;
      break;
    }
    
    if (osc->p == NULL) {
      printf("failed to bind socket\n");
    }
  }
  return osc->p;
}

inline void OSCDisconnect(OSCRef osc) {
  
}

#pragma mark Addresses

CFArrayRef OSCCreateAddressArray(OSCRef osc) {
  CFArrayRef array = NULL;
  if (osc) {
    if (osc->cache) {
      CFIndex n = CFDictionaryGetCount(osc->cache);
      CFTypeRef *keys = CFAllocatorAllocate(osc->allocator, sizeof(CFTypeRef) * n, 0);
      CFDictionaryGetKeysAndValues(osc->cache, keys, NULL);
      array = CFArrayCreate(osc->allocator, keys, n, &kCFTypeArrayCallBacks);
      CFAllocatorDeallocate(osc->allocator, keys);
    }
  }
  return array;
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
    if (CFGetTypeID(value) == CFNumberGetTypeID()) {
      if (CFNumberIsFloatType(value)) {
        result = OSCSetValue(osc, name, value);
      } else {
        Float32 value_ = (Float32)0.0;
        CFNumberGetValue(value, kCFNumberFloat32Type, &value_);
        CFNumberRef number = CFNumberCreate(osc->allocator, kCFNumberFloat32Type, &value_);
        result = OSCSetValue(osc, name, number);
        CFRelease(number);
      }
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
        Float32 float32Value = (Float32)0.0;
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

ssize_t sendallto(int s, const void *buf, size_t len,
               int flags, const struct sockaddr *to,
               socklen_t tolen) {
  int total = 0;        // how many bytes we've sent
  int bytesleft = len; // how many we have left to send
  int n;
  
  while(total < len) {
    n = sendto(s, buf+total, bytesleft, flags, to, tolen);
    if (n == -1) { break; }
    total += n;
    bytesleft -= n;
  }
  
  len = total; // return number actually sent here
  
  return n==-1?-1:0; // return -1 on failure, 0 on success
} 

inline OSCResult OSCSendRawBuffer(OSCRef osc, const void *buffer, CFIndex length) {
  OSCResult result = kOSCResultNotAllocatedError;
  if (osc && osc->sockfd && osc->p) {
    if ((result = (OSCResult)sendallto(osc->sockfd, buffer, length, 0, osc->p->ai_addr, osc->p->ai_addrlen)) == -1) {
//      if ((result = (OSCResult)sendto(osc->sockfd, buffer, length, 0, osc->p->ai_addr, osc->p->ai_addrlen)) == -1) {
      // TODO: Error
    }
  } else {
    printf("failed to send buffer osc=%p", osc);
    if (osc)
      printf(", osc->sosckfd=%i, osc->p=%p", osc->sockfd, osc->p);
    printf("\n");
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
    
//    __OSCBufferPrint(buffer, i);
    
    printf("> %f\n", value);
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

