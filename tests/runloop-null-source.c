// Run linked against the candidate CoreFoundation runtime.
// A NULL run loop source (what IONotificationPortGetRunLoopSource(NULL) hands back when
// IORegisterForSystemPower failed) must not crash CFRunLoopAddSource, CFRunLoopRemoveSource or
// CFRunLoopContainsSource. The first add and remove each log one warning on stderr.
#include <CoreFoundation/CoreFoundation.h>
#include <assert.h>
#include <stdio.h>

int main(void) {
    CFRunLoopRef loop = CFRunLoopGetCurrent();

    CFRunLoopAddSource(loop, NULL, kCFRunLoopDefaultMode);
    CFRunLoopAddSource(loop, NULL, kCFRunLoopCommonModes);
    assert(!CFRunLoopContainsSource(loop, NULL, kCFRunLoopDefaultMode));
    CFRunLoopRemoveSource(loop, NULL, kCFRunLoopDefaultMode);
    CFRunLoopRemoveSource(loop, NULL, kCFRunLoopCommonModes);

    // a real source is still added, found and removed
    CFRunLoopSourceContext context = { 0 };
    CFRunLoopSourceRef source = CFRunLoopSourceCreate(kCFAllocatorDefault, 0, &context);
    CFRunLoopAddSource(loop, source, kCFRunLoopDefaultMode);
    assert(CFRunLoopContainsSource(loop, source, kCFRunLoopDefaultMode));
    CFRunLoopRemoveSource(loop, source, kCFRunLoopDefaultMode);
    assert(!CFRunLoopContainsSource(loop, source, kCFRunLoopDefaultMode));
    CFRelease(source);

    puts("PASS: NULL run loop sources are ignored, real sources still work");
    return 0;
}
