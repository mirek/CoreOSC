//
// CoreOSC.h
// CoreOSC Framework
//
// Created by Mirek Rusin on 05/03/2011.
// Copyright 2011 Inteliv Ltd. All rights reserved.
//

#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <unistd.h>
#include <netdb.h>

#define OSC_ADDRESSES_LENGTH 1024

#define OSC_STATIC_ADDRESS_LENGTH        128
#define OSC_STATIC_STRING_LENGTH         256
#define OSC_STATIC_BLOB_LENGTH           256

#define OSC_STATIC_INT32_PACKET_LENGTH   (OSC_STATIC_ADDRESS_LENGTH + 4 + 4)
#define OSC_STATIC_INT64_PACKET_LENGTH   (OSC_STATIC_ADDRESS_LENGTH + 4 + 8)
#define OSC_STATIC_FLOAT32_PACKET_LENGTH (OSC_STATIC_ADDRESS_LENGTH + 4 + 4)
#define OSC_STATIC_FLOAT64_PACKET_LENGTH (OSC_STATIC_ADDRESS_LENGTH + 4 + 8)
#define OSC_STATIC_STRING_PACKET_LENGTH  (OSC_STATIC_ADDRESS_LENGTH + 4 + OSC_STATIC_STRING_LENGTH)

#define OSC_STATIC_FLOATS_TYPE_HEADER_LENGTH 64

#define __OSCGet32BitAlignedLength(n) (((((n) - 1) >> 2) << 2) + 4)

#define __OSCBufferAppend(b, a, l, i) memcpy(buffer + i, a, l); i += l;
#define __OSCBufferAppendAddress(b, a, i) __OSCBufferAppend(b, a.buffer, a.length, i)

// Finish buffer by zeroing 32bit aligned, 0-3 bytes and send the buffer returning the result
#define __OSCBufferSend(osc, buffer, i, result) memset(buffer + i, 0, __OSCGet32BitAlignedLength(i) - i); \
                                                result = OSCSend(osc, buffer, __OSCGet32BitAlignedLength(i));

#pragma mark Internal string helper for fast UTF8 buffer access

typedef struct {
  CFAllocatorRef allocator;
  CFStringRef string;
  const char *pointer;
  const char *buffer;
  CFIndex maximumSize;
} __OSCUTF8String;

__OSCUTF8String __OSCUTF8StringMake(CFAllocatorRef allocator, CFStringRef string);
const char     *__OSCUTF8StringGetBuffer(__OSCUTF8String utf8String);
CFIndex         __OSCUTF8StringGetMaximumSize(__OSCUTF8String utf8String);
void            __OSCUTF8StringDestroy(__OSCUTF8String utf8String);

#pragma mark Internal, diagnostics

void    __OSCBufferPrint(char *buffer, int length);

typedef enum OSCResult {
  kOSCResultNotAllocatedError = -1001 // OSCRef object has not been allocated?
} OSCResult;

typedef struct __OSCAddress {
  CFIndex length;
  void *buffer;
} __OSCAddress;

typedef struct OSC {
  CFAllocatorRef allocator;
  CFIndex retainCount;
  
  CFStringRef host;
  CFStringRef port;
  
  CFIndex addressesIndex;
  __OSCAddress *addresses;
  
  int sockfd;
  struct addrinfo hints;
  struct addrinfo *servinfo;
  struct addrinfo *p;
  int rv;
  int numbytes;
} OSC;

typedef OSC *OSCRef;

#pragma mark OSC API

OSCRef    OSCCreate                      (CFAllocatorRef allocator, CFStringRef host, CFStringRef port);
OSCRef    OSCRelease                     (OSCRef osc);

#pragma mark Addresses

bool      OSCAddressesIsIndexInBounds    (OSCRef osc, CFIndex index);
bool      OSCAddressesIsIndexOutOfBounds (OSCRef osc, CFIndex index);
CFIndex   OSCAddressesAppendWithString   (OSCRef osc, CFStringRef string);
void      OSCAddressesClear              (OSCRef osc);

#pragma mark Sending

OSCResult OSCSend        (OSCRef osc, const void *buffer, CFIndex length);
OSCResult OSCSendTrue    (OSCRef osc, CFIndex index);
OSCResult OSCSendFalse   (OSCRef osc, CFIndex index);
OSCResult OSCSendBool    (OSCRef osc, CFIndex index, bool value);
OSCResult OSCSendFloat   (OSCRef osc, CFIndex index, Float32 value);
OSCResult OSCSendFloats  (OSCRef osc, CFIndex index, const Float32 *values, CFIndex n);
OSCResult OSCSendInt32   (OSCRef osc, CFIndex index, int32_t value);
OSCResult OSCSendBytes   (OSCRef osc, CFIndex index, const UInt8 *value);
OSCResult OSCSendCString (OSCRef osc, CFIndex index, const char *value);
OSCResult OSCSendString  (OSCRef osc, CFIndex index, CFStringRef value);
