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

inline OSCRef OSCCreate(CFAllocatorRef allocator, CFStringRef host, CFStringRef port) {
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

  }
  return osc;
}

inline OSCRef OSCRelease(OSCRef osc) {
  if (osc) {
    if (--osc->retainCount == 0) {
      CFAllocatorRef allocator = osc->allocator;
      
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

inline OSCResult OSCSend(OSCRef osc, const void *buffer, CFIndex length) {
  OSCResult result = kOSCResultNotAllocatedError;
  if (osc) {
    if ((result = (OSCResult)sendto(osc->sockfd, buffer, length, 0, osc->p->ai_addr, osc->p->ai_addrlen)) == -1) {
      // TODO: Error
    }
  }
  return result;
}

inline OSCResult OSCSendTrue(OSCRef osc, CFIndex index) {
  OSCResult result = kOSCResultNotAllocatedError;
  if (osc) {
    if (OSCAddressesIsIndexInBounds(osc, index)) {
      char buffer[1024];
      int i = 0;
      __OSCBufferAppendAddress(buffer, osc->addresses[index], i);
      __OSCBufferAppend(buffer, ",T\0\0", 4, i);
      __OSCBufferSend(osc, buffer, i, result);
    }
  }
  return result;
}

inline OSCResult OSCSendFalse(OSCRef osc, CFIndex index) {
  OSCResult result = kOSCResultNotAllocatedError;
  if (osc) {
    if (OSCAddressesIsIndexInBounds(osc, index)) {
      char buffer[1024];
      int i = 0;
      __OSCBufferAppendAddress(buffer, osc->addresses[index], i);
      __OSCBufferAppend(buffer, ",F\0\0", 4, i);
      __OSCBufferSend(osc, buffer, i, result);
    }
  }
  return result;
}

inline OSCResult OSCSendBool(OSCRef osc, CFIndex index, bool value) {
  OSCResult result = kOSCResultNotAllocatedError;
  if (osc) {
    result = value ? OSCSendTrue(osc, index) : OSCSendFalse(osc, index);
  }
  return result;
}

inline OSCResult OSCSendInt32(OSCRef osc, CFIndex index, int32_t value) {
  OSCResult result = kOSCResultNotAllocatedError;
  if (osc) {
    if (OSCAddressesIsIndexInBounds(osc, index)) {
      char buffer[OSC_STATIC_INT32_PACKET_LENGTH];
      
      int32_t swappedValue = CFSwapInt32HostToBig(value);
      
      int i = 0;
      __OSCBufferAppendAddress(buffer, osc->addresses[index], i);
      __OSCBufferAppend(buffer, ",i\0\0", 4, i);
      __OSCBufferAppend(buffer, &swappedValue, 4, i);
      __OSCBufferSend(osc, buffer, i, result);
    }
  }
  return result;
}

inline OSCResult OSCSendFloat(OSCRef osc, CFIndex index, Float32 value) {
  OSCResult result = kOSCResultNotAllocatedError;
  if (osc) {
    if (OSCAddressesIsIndexInBounds(osc, index)) {
      char buffer[OSC_STATIC_FLOAT32_PACKET_LENGTH];
      
      CFSwappedFloat32 swappedValue = CFConvertFloat32HostToSwapped(value);
      
      int i = 0;
      __OSCBufferAppendAddress(buffer, osc->addresses[index], i);
      __OSCBufferAppend(buffer, ",f\0\0", 4, i);
      __OSCBufferAppend(buffer, &swappedValue, 4, i);
      __OSCBufferSend(osc, buffer, i, result);
    }
  }
  return result;
}

inline OSCResult OSCSendFloats(OSCRef osc, CFIndex index, const Float32 *values, CFIndex n) {
  OSCResult result = kOSCResultNotAllocatedError;
  if (osc) {
    if (OSCAddressesIsIndexInBounds(osc, index)) {
      char buffer[1024];
      
      // Type tag
      char type[OSC_STATIC_FLOATS_TYPE_HEADER_LENGTH];
      memset(type, 0, OSC_STATIC_FLOATS_TYPE_HEADER_LENGTH);
      type[0] = ',';
      memset(type + 1, 'f', n);
      
      int i = 0;
      __OSCBufferAppendAddress(buffer, osc->addresses[index], i);
      __OSCBufferAppend(buffer, type, __OSCGet32BitAlignedLength(1 + n), i);
      for (int j = 0; j < n; j++) {
        CFSwappedFloat32 swappedValue = CFConvertFloat32HostToSwapped(values[j]);
        __OSCBufferAppend(buffer, &swappedValue, 4, i);
      }
      __OSCBufferSend(osc, buffer, i, result);
      
    }
  }
  return result;
}

inline OSCResult OSCSendCString(OSCRef osc, CFIndex index, const char *value) {
  OSCResult result = kOSCResultNotAllocatedError;
  if (osc) {
    if (OSCAddressesIsIndexInBounds(osc, index)) {
      char buffer[OSC_STATIC_STRING_PACKET_LENGTH];
      unsigned long length = strlen(value);
      
      int i = 0;
      __OSCBufferAppendAddress(buffer, osc->addresses[index], i);
      __OSCBufferAppend(buffer, ",s\0\0", 4, i);
      __OSCBufferAppend(buffer, value, length, i);
      __OSCBufferSend(osc, buffer, i, result);
    }
  }
  return result;
}

inline OSCResult OSCSendString(OSCRef osc, CFIndex index, CFStringRef value) {
  OSCResult result = kOSCResultNotAllocatedError;
  if (osc) {
    __OSCUTF8String utf8Value = __OSCUTF8StringMake(osc->allocator, value);
    result = OSCSendCString(osc, index, __OSCUTF8StringGetBuffer(utf8Value));
    __OSCUTF8StringDestroy(utf8Value);
  }
  return result;
}