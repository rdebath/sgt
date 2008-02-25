/*
 * OS X Cocoa front end to ick-proxy. Wraps most of the Unix front
 * end, but provides a minimal Cocoa GUI to tie the proxy's
 * lifetime to that of the GUI session.
 */

/*
 * Possible future work:
 * 
 *  - it would be nice to have a `Test' dock menu option, which
 *    brought up a dialog box into which one could type URLs and
 *    have ick-proxy run its script on them and print the results.
 *    Apart from the generalised GUI hassle of designing and
 *    setting up the dialog box, the other problem is that the
 *    test itself would have to be run inside the proxy thread, so
 *    we'd need to invent a more complex protocol to run over the
 *    clientfd.
 */

#include <ctype.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#import <Cocoa/Cocoa.h>

#include "unix.h"

/* ----------------------------------------------------------------------
 * Code for forking off a subthread in which to run the actual proxy.
 */

static int pipefdread(int fd) {
    char buf[1];
    int ret;

    ret = read(fd, buf, 1);
    if (ret == 1) {
	/*
	 * A byte has come down the pipe. That means we've been
	 * told to re-read our configuration.
	 *
	 * This function is called in the proxy subthread, so we
	 * can safely call configure_single_user() with no fear of
	 * race conditions.
	 */
	configure_single_user();
    } else {
	/*
	 * The pipe has closed. Return 1, telling the main proxy
	 * code to shut down.
	 */
	return 1;
    }

    return 0;
}

/*
 * NSThread requires an object method as its thread procedure, so
 * here I define a trivial holding class.
 */
@class ProxyThread;
@interface ProxyThread : NSObject
{
    int pipefd;
}
- (ProxyThread *)setPipeFdRead:(int)fd;
- (void)runThread:(id)arg;
@end
@implementation ProxyThread
- (ProxyThread *)setPipeFdRead:(int)fd
{
    pipefd = fd;
    return self;
}
- (void)runThread:(id)arg
{
    uxmain(0, 0, NULL, NULL, pipefd, pipefdread, 0);
}
@end

/* ----------------------------------------------------------------------
 * Utility routines for constructing OS X menus.
 */

NSMenu *newmenu(const char *title)
{
    return [[[NSMenu allocWithZone:[NSMenu menuZone]]
	     initWithTitle:[NSString stringWithCString:title]]
	    autorelease];
}

NSMenu *newsubmenu(NSMenu *parent, const char *title)
{
    NSMenuItem *item;
    NSMenu *child;

    item = [[[NSMenuItem allocWithZone:[NSMenu menuZone]]
	     initWithTitle:[NSString stringWithCString:title]
	     action:NULL
	     keyEquivalent:@""]
	    autorelease];
    child = newmenu(title);
    [item setEnabled:YES];
    [item setSubmenu:child];
    [parent addItem:item];
    return child;
}

id initnewitem(NSMenuItem *item, NSMenu *parent, const char *title,
	       const char *key, id target, SEL action)
{
    unsigned mask = NSCommandKeyMask;

    if (key[strcspn(key, "-")]) {
	while (*key && *key != '-') {
	    int c = tolower((unsigned char)*key);
	    if (c == 's') {
		mask |= NSShiftKeyMask;
	    } else if (c == 'o' || c == 'a') {
		mask |= NSAlternateKeyMask;
	    }
	    key++;
	}
	if (*key)
	    key++;
    }

    item = [[item initWithTitle:[NSString stringWithCString:title]
	     action:NULL
	     keyEquivalent:[NSString stringWithCString:key]]
	    autorelease];

    if (*key)
	[item setKeyEquivalentModifierMask: mask];

    [item setEnabled:YES];
    [item setTarget:target];
    [item setAction:action];

    [parent addItem:item];

    return item;
}

NSMenuItem *newitem(NSMenu *parent, char *title, char *key,
		    id target, SEL action)
{
    return initnewitem([NSMenuItem allocWithZone:[NSMenu menuZone]],
		       parent, title, key, target, action);
}

/* ----------------------------------------------------------------------
 * About box.
 */

@class AboutBox;

@interface AboutBox : NSWindow
{
}
- (id)init;
@end

@implementation AboutBox
- (id)init
{
    NSRect totalrect;
    NSView *views[16];
    int nviews = 0;
    NSImageView *iv;
    NSTextField *tf;
    NSFont *font1 = [NSFont systemFontOfSize:0];
    NSFont *font2 = [NSFont boldSystemFontOfSize:[font1 pointSize] * 1.1];
    const int border = 24;
    int i;
    double y;

    /*
     * Construct the controls that go in the About box.
     */

    iv = [[NSImageView alloc] initWithFrame:NSMakeRect(0,0,64,64)];
    [iv setImage:[NSImage imageNamed:@"NSApplicationIcon"]];
    views[nviews++] = iv;

    tf = [[NSTextField alloc]
	  initWithFrame:NSMakeRect(0,0,400,1)];
    [tf setEditable:NO];
    [tf setSelectable:NO];
    [tf setBordered:NO];
    [tf setDrawsBackground:NO];
    [tf setFont:font2];
    [tf setStringValue:@"ick-proxy"];
    [tf sizeToFit];
    views[nviews++] = tf;

    tf = [[NSTextField alloc]
	  initWithFrame:NSMakeRect(0,0,400,1)];
    [tf setEditable:NO];
    [tf setSelectable:NO];
    [tf setBordered:NO];
    [tf setDrawsBackground:NO];
    [tf setFont:font1];
    [tf setStringValue:[NSString stringWithCString:"2.0"]];
    [tf sizeToFit];
    views[nviews++] = tf;

    /*
     * Lay the controls out.
     */
    totalrect = NSMakeRect(0,0,0,0);
    for (i = 0; i < nviews; i++) {
	NSRect r = [views[i] frame];
	if (totalrect.size.width < r.size.width)
	    totalrect.size.width = r.size.width;
	totalrect.size.height += border + r.size.height;
    }
    totalrect.size.width += 2 * border;
    totalrect.size.height += border;
    y = totalrect.size.height;
    for (i = 0; i < nviews; i++) {
	NSRect r = [views[i] frame];
	r.origin.x = (totalrect.size.width - r.size.width) / 2;
	y -= border + r.size.height;
	r.origin.y = y;
	[views[i] setFrame:r];
    }

    self = [super initWithContentRect:totalrect
	    styleMask:(NSTitledWindowMask | NSMiniaturizableWindowMask |
		       NSClosableWindowMask)
	    backing:NSBackingStoreBuffered
	    defer:YES];

    for (i = 0; i < nviews; i++)
	[[self contentView] addSubview:views[i]];

    [self center];		       /* :-) */

    return self;
}
@end

/* ----------------------------------------------------------------------
 * AppController: the object which receives the messages from all
 * menu selections that aren't standard OS X functions.
 */
@interface AppController : NSObject
{
    int pipefd;
}
- (void)about:(id)sender;
- (void)rereadConfig:(id)sender;
- (void)setPipeFdWrite:(int)fd;
@end

@implementation AppController

- (void)setPipeFdWrite:(int)fd
{
    pipefd = fd;
}

- (void)about:(id)sender
{
    id win;

    win = [[AboutBox alloc] init];
    [win makeKeyAndOrderFront:self];
}

- (void)rereadConfig:(id)sender
{
    write(pipefd, "r", 1);
}

- (NSMenu *)applicationDockMenu:(NSApplication *)sender
{
    NSMenu *menu = newmenu("Dock Menu");
    initnewitem([NSMenuItem allocWithZone:[NSMenu menuZone]],
		menu, "Re-read configuration", "", self,
		@selector(rereadConfig:));
    return menu;
}

@end

/* ----------------------------------------------------------------------
 * Main program. Constructs the menus and runs the application.
 */
int main(int argc, char **argv)
{
    NSAutoreleasePool *pool;
    NSMenu *menu;
    NSMenuItem *item;
    AppController *controller;
    NSImage *icon;
    int mypipe[2];

    pool = [[NSAutoreleasePool alloc] init];

    icon = [NSImage imageNamed:@"NSApplicationIcon"];
    [NSApplication sharedApplication];
    [NSApp setApplicationIconImage:icon];

    controller = [[[AppController alloc] init] autorelease];
    [NSApp setDelegate:controller];

    [NSApp setMainMenu: newmenu("Main Menu")];

    menu = newsubmenu([NSApp mainMenu], "Apple Menu");
    item = newitem(menu, "About ick-proxy", "", NULL, @selector(about:));
    [menu addItem:[NSMenuItem separatorItem]];
    [NSApp setServicesMenu:newsubmenu(menu, "Services")];
    [menu addItem:[NSMenuItem separatorItem]];
    item = newitem(menu, "Hide ick-proxy", "h", NSApp, @selector(hide:));
    item = newitem(menu, "Hide Others", "o-h", NSApp, @selector(hideOtherApplications:));
    item = newitem(menu, "Show All", "", NSApp, @selector(unhideAllApplications:));
    [menu addItem:[NSMenuItem separatorItem]];
    item = newitem(menu, "Quit", "q", NSApp, @selector(terminate:));
    [NSApp setAppleMenu: menu];

    menu = newsubmenu([NSApp mainMenu], "File");
    item = newitem(menu, "Close", "w", NULL, @selector(performClose:));

    menu = newsubmenu([NSApp mainMenu], "Edit");
    item = newitem(menu, "Cut", "x", NULL, @selector(cut:));
    item = newitem(menu, "Copy", "c", NULL, @selector(copy:));
    item = newitem(menu, "Paste", "v", NULL, @selector(paste:));

    menu = newsubmenu([NSApp mainMenu], "Window");
    [NSApp setWindowsMenu: menu];
    item = newitem(menu, "Minimise Window", "m", NULL, @selector(performMiniaturize:));

    /*
     * Now run the real proxy in a subthread. (We can't run it
     * within our main loop here, because Cocoa doesn't provide
     * proper select; and we also wouldn't want to anyway, because
     * it's much nicer to keep the same Unix main loop everywhere
     * than to have to maintain two.)
     */
    if (pipe(mypipe) < 0) {
	return 1;		       /* FIXME */
    }
    [controller setPipeFdWrite:mypipe[1]];
    [NSThread detachNewThreadSelector:@selector(runThread:)
	toTarget:[[[[ProxyThread alloc] init] retain] setPipeFdRead:mypipe[0]]
        withObject:nil];

    [NSApp run];

    close(mypipe[1]);

    [pool release];

    return 0;
}
