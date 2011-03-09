//
// CoreOSC.h
// CoreOSC Framework
//
// Created by Mirek Rusin on 05/03/2011.
// Copyright 2011 Inteliv Ltd. All rights reserved.
//

#include <CoreFoundation/CoreFoundation.h>

#if (TARGET_OS_EMBEDDED)
#include <CFNetwork/CFNetwork.h>
#else
#include <CoreServices/CoreServices.h>
#endif

#include <unistd.h>
#include <netdb.h>

#define OSC_ADDRESSES_LENGTH 1024

#define OSC_STATIC_ADDRESS_LENGTH        128
#define OSC_STATIC_STRING_LENGTH         256
#define OSC_STATIC_BLOB_LENGTH           256

#define OSC_STATIC_TRUE_PACKET_LENGTH    (OSC_STATIC_ADDRESS_LENGTH + 4)
#define OSC_STATIC_FALSE_PACKET_LENGTH   (OSC_STATIC_ADDRESS_LENGTH + 4)
#define OSC_STATIC_SINT32_PACKET_LENGTH  (OSC_STATIC_ADDRESS_LENGTH + 4 + 4)
#define OSC_STATIC_SINT64_PACKET_LENGTH  (OSC_STATIC_ADDRESS_LENGTH + 4 + 8)
#define OSC_STATIC_FLOAT32_PACKET_LENGTH (OSC_STATIC_ADDRESS_LENGTH + 4 + 4)
#define OSC_STATIC_FLOAT64_PACKET_LENGTH (OSC_STATIC_ADDRESS_LENGTH + 4 + 8)
#define OSC_STATIC_STRING_PACKET_LENGTH  (OSC_STATIC_ADDRESS_LENGTH + 4 + OSC_STATIC_STRING_LENGTH)

#define OSC_STATIC_FLOATS32_PACKET_HEADER_LENGTH 128
#define OSC_STATIC_FLOATS32_PACKET_LENGTH        (OSC_STATIC_ADDRESS_LENGTH + OSC_STATIC_FLOATS32_PACKET_HEADER_LENGTH + OSC_STATIC_FLOATS32_PACKET_HEADER_LENGTH * 4)

#define __OSCGet32BitAlignedLength(n) (((((n) - 1) >> 2) << 2) + 4)

#define __OSCBufferAppend(b, a, l, i) memcpy(buffer + i, a, l); i += l;
#define __OSCBufferAppendAddress(b, a, i) __OSCBufferAppend(b, a.buffer, a.length, i)

// Finish buffer by zeroing 32bit aligned, 0-3 bytes and send the buffer returning the result
#define __OSCBufferSend(osc, buffer, i, result) memset(buffer + i, 0, __OSCGet32BitAlignedLength(i) - i); \
                                                result = OSCSendRawBuffer(osc, buffer, __OSCGet32BitAlignedLength(i));

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
  
  CFRunLoopTimerRef runLoopTimer;
  
  // Hold keys (addresses) and array of values.
  // Depending on the mode, new values are replaced or added to the array.
  // Run loop timer on each execution will send the first value for all
  // keys.
  CFMutableDictionaryRef cache;
  
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

void __OSCBufferAppendAddressWithString  (void *buffer, CFStringRef name, int *i);

#pragma mark OSC API

void __OSCRunLoopTimerCallBack(CFRunLoopTimerRef timer, void *info);

OSCRef    OSCCreate                      (CFAllocatorRef allocator, CFStringRef host, CFStringRef port, CFTimeInterval timeInterval);
OSCRef    OSCRetain                      (OSCRef osc);
OSCRef    OSCRelease                     (OSCRef osc);

#pragma mark Addresses

bool      OSCAddressesIsIndexInBounds    (OSCRef osc, CFIndex index);
bool      OSCAddressesIsIndexOutOfBounds (OSCRef osc, CFIndex index);
CFIndex   OSCAddressesAppendWithString   (OSCRef osc, CFStringRef string);
void      OSCAddressesClear              (OSCRef osc);

void      OSCActivateRunLoopTimer        (OSCRef osc, CFTimeInterval timeInterval);
void      OSCDeactivateRunLoopTimer      (OSCRef osc);

#pragma mark Sending

// Async, scheduled for send with run loop timer
OSCResult OSCSetValue              (OSCRef osc, CFStringRef name, CFTypeRef value);
OSCResult OSCSetNumberAsFloat32    (OSCRef osc, CFStringRef name, CFNumberRef value);

OSCResult OSCSendRawBuffer         (OSCRef osc, const void *buffer, CFIndex length);
OSCResult OSCSendRawBufferWithData (OSCRef osc, CFDataRef data);

#pragma mark 

OSCResult OSCSendTrue              (OSCRef osc, CFStringRef name);
OSCResult OSCSendFalse             (OSCRef osc, CFStringRef name);
OSCResult OSCSendBool              (OSCRef osc, CFStringRef name, bool value);
OSCResult OSCSendFloat32           (OSCRef osc, CFStringRef name, Float32 value);
OSCResult OSCSendFloats32          (OSCRef osc, CFStringRef name, const Float32 *values, CFIndex n);
OSCResult OSCSendSInt32            (OSCRef osc, CFStringRef name, SInt32 value);
OSCResult OSCSendCString           (OSCRef osc, CFStringRef name, const UInt8 *value);

#pragma mark CFTypes

OSCResult OSCSendValue             (OSCRef osc, CFStringRef name, CFTypeRef value);
OSCResult OSCSendNumbersAsFloats32 (OSCRef osc, CFStringRef name, const CFNumberRef *values, CFIndex n);

OSCResult OSCSendNumberAsSInt32    (OSCRef osc, CFStringRef name, CFNumberRef value);
OSCResult OSCSendNumberAsFloat32   (OSCRef osc, CFStringRef name, CFNumberRef value);

OSCResult OSCSendBoolean           (OSCRef osc, CFStringRef name, CFBooleanRef value);
OSCResult OSCSendString            (OSCRef osc, CFStringRef name, CFStringRef value);
