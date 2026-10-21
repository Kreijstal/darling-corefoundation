/*
 * Copyright (c) 2026 Darling Project. All rights reserved.
 * Distributed under the terms of the Apple Public Source License (APSL 2.0).
 */

#include <CoreFoundation/CFFileDescriptor.h>
#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CFRunLoop.h>
#include <dispatch/dispatch.h>
#include <unistd.h>
#include <stdio.h>

struct __CFFileDescriptor {
    CFRuntimeBase _base;
    CFFileDescriptorNativeDescriptor _fd;
    CFFileDescriptorCallBack _callout;
    CFFileDescriptorContext _context;
    CFOptionFlags _callBackTypes;
    Boolean _closeOnInvalidate;
    Boolean _valid;
    CFRunLoopSourceRef _source;
};

static CFTypeID __kCFFileDescriptorTypeID = _kCFRuntimeNotATypeID;

static void __CFFileDescriptorDeallocate(CFTypeRef cf) {
    CFFileDescriptorRef f = (CFFileDescriptorRef)cf;
    CFFileDescriptorInvalidate(f);
    if (f->_context.info && f->_context.release) {
        f->_context.release(f->_context.info);
    }
}

static CFStringRef __CFFileDescriptorCopyDescription(CFTypeRef cf) {
    CFFileDescriptorRef f = (CFFileDescriptorRef)cf;
    return CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("<CFFileDescriptor %p>{fd = %d, valid = %s}"), f, f->_fd, f->_valid ? "yes" : "no");
}

static const CFRuntimeClass __CFFileDescriptorClass = {
    0,
    "CFFileDescriptor",
    NULL,      // init
    NULL,      // copy
    __CFFileDescriptorDeallocate,
    NULL,      // equal
    NULL,      // hash
    NULL,      // formattingDesc
    __CFFileDescriptorCopyDescription
};

CFTypeID CFFileDescriptorGetTypeID(void) {
    static dispatch_once_t initOnce;
    dispatch_once(&initOnce, ^{
        __kCFFileDescriptorTypeID = _CFRuntimeRegisterClass(&__CFFileDescriptorClass);
    });
    return __kCFFileDescriptorTypeID;
}

CFFileDescriptorRef CFFileDescriptorCreate(CFAllocatorRef allocator, CFFileDescriptorNativeDescriptor fd, Boolean closeOnInvalidate, CFFileDescriptorCallBack callout, const CFFileDescriptorContext *context) {
    if (fd < 0 || !callout)
        return NULL;

    CFTypeID typeID = CFFileDescriptorGetTypeID();
    CFFileDescriptorRef f = (CFFileDescriptorRef)_CFRuntimeCreateInstance(allocator, typeID, sizeof(struct __CFFileDescriptor) - sizeof(CFRuntimeBase), NULL);
    if (!f)
        return NULL;

    f->_fd = fd;
    f->_closeOnInvalidate = closeOnInvalidate;
    f->_callout = callout;
    f->_callBackTypes = 0;
    f->_valid = true;
    f->_source = NULL;

    if (context) {
        f->_context = *context;
        if (f->_context.info && f->_context.retain) {
            f->_context.info = f->_context.retain(f->_context.info);
        }
    } else {
        memset(&f->_context, 0, sizeof(f->_context));
    }

    return f;
}

CFFileDescriptorNativeDescriptor CFFileDescriptorGetNativeDescriptor(CFFileDescriptorRef f) {
    if (!f) return -1;
    return f->_fd;
}

void CFFileDescriptorGetContext(CFFileDescriptorRef f, CFFileDescriptorContext *context) {
    if (f && context) {
        *context = f->_context;
    }
}

void CFFileDescriptorEnableCallBacks(CFFileDescriptorRef f, CFOptionFlags callBackTypes) {
    if (!f || !f->_valid) return;
    f->_callBackTypes |= callBackTypes;
    if (f->_source) {
        CFRunLoopSourceSignal(f->_source);
    }
}

void CFFileDescriptorDisableCallBacks(CFFileDescriptorRef f, CFOptionFlags callBackTypes) {
    if (!f || !f->_valid) return;
    f->_callBackTypes &= ~callBackTypes;
}

void CFFileDescriptorInvalidate(CFFileDescriptorRef f) {
    if (!f || !f->_valid) return;
    f->_valid = false;
    f->_callBackTypes = 0;
    if (f->_source) {
        CFRunLoopSourceInvalidate(f->_source);
        CFRelease(f->_source);
        f->_source = NULL;
    }
    if (f->_closeOnInvalidate && f->_fd >= 0) {
        close(f->_fd);
        f->_fd = -1;
    }
}

Boolean CFFileDescriptorIsValid(CFFileDescriptorRef f) {
    return f ? f->_valid : false;
}

static void __CFFDRunLoopPerform(void *info) {
    CFFileDescriptorRef f = (CFFileDescriptorRef)info;
    if (!f || !f->_valid || !f->_callout)
        return;

    CFOptionFlags types = f->_callBackTypes;
    f->_callBackTypes = 0; // callbacks are one-shot until re-enabled
    if (types != 0) {
        f->_callout(f, types, f->_context.info);
    }
}

CFRunLoopSourceRef CFFileDescriptorCreateRunLoopSource(CFAllocatorRef allocator, CFFileDescriptorRef f, CFIndex order) {
    if (!f || !f->_valid)
        return NULL;

    if (f->_source) {
        CFRetain(f->_source);
        return f->_source;
    }

    CFRunLoopSourceContext context;
    memset(&context, 0, sizeof(context));
    context.version = 0;
    context.info = (void *)f;
    context.retain = (const void *(*)(const void *))CFRetain;
    context.release = (void (*)(const void *))CFRelease;
    context.copyDescription = (CFStringRef (*)(const void *))CFCopyDescription;
    context.equal = (Boolean (*)(const void *, const void *))CFEqual;
    context.hash = (CFHashCode (*)(const void *))CFHash;
    context.perform = __CFFDRunLoopPerform;

    f->_source = CFRunLoopSourceCreate(allocator, order, &context);
    if (f->_source) {
        return (CFRunLoopSourceRef)CFRetain(f->_source);
    }
    return NULL;
}

void _CFFileDescriptorInduceFakeReadCallBack(CFFileDescriptorRef f) {
    if (!f || !f->_valid) return;
    f->_callBackTypes |= kCFFileDescriptorReadCallBack;
    if (f->_source) {
        CFRunLoopSourceSignal(f->_source);
    }
}
