/* $Id: mac.c,v 1.21 2003/01/09 18:06:29 simon Exp $ */
/*
 * Copyright (c) 1999 Ben Harris
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
/*
 * mac.c -- miscellaneous Mac-specific routines
 */

#include <MacTypes.h>
#include <Quickdraw.h>
#include <Fonts.h>
#include <MacWindows.h>
#include <Menus.h>
#include <TextEdit.h>
#include <Appearance.h>
#include <CodeFragments.h>
#include <Dialogs.h>
#include <Devices.h>
#include <DiskInit.h>
#include <Gestalt.h>
#include <LowMem.h>
#include <Resources.h>
#include <Script.h>
#include <TextCommon.h>
#include <ToolUtils.h>
#include <UnicodeConverter.h>

#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>		/* putty.h needs size_t */
#include <stdio.h>		/* for vsprintf */

#define PUTTY_DO_GLOBALS

#include "macresid.h"
#include "putty.h"
#include "ssh.h"
#include "mac.h"

QDGlobals qd;

Session *sesslist;

static int cold = 1;
struct mac_gestalts mac_gestalts;

static void mac_startup(void);
static void mac_eventloop(void);
#pragma noreturn (mac_eventloop)
static void mac_event(EventRecord *);
static void mac_contentclick(WindowPtr, EventRecord *);
static void mac_growwindow(WindowPtr, EventRecord *);
static void mac_activatewindow(WindowPtr, EventRecord *);
static void mac_activateabout(WindowPtr, EventRecord *);
static void mac_updatewindow(WindowPtr);
static void mac_updatelicence(WindowPtr);
static void mac_keypress(EventRecord *);
static int mac_windowtype(WindowPtr);
static void mac_menucommand(long);
static void mac_openabout(void);
static void mac_openlicence(void);
static void mac_adjustcursor(RgnHandle);
static void mac_adjustmenus(void);
static void mac_closewindow(WindowPtr);
static void mac_zoomwindow(WindowPtr, short);
static void mac_shutdown(void);
#pragma noreturn (cleanup_exit)

struct mac_windows {
    WindowPtr about;
    WindowPtr licence;
};

struct mac_windows windows;

int main (int argc, char **argv) {

    mac_startup();
    mac_eventloop();
}

#pragma noreturn (main)

static void mac_startup(void) {
    Handle menuBar;
    TECInfoHandle ti;

    /* Init Memory Manager */
    MaxApplZone();
    /* Init QuickDraw */
    InitGraf(&qd.thePort);
    /* Init Font Manager */
    InitFonts();
    /* Init Window Manager */
    InitWindows();
    /* Init Menu Manager */
    InitMenus();
    /* Init TextEdit */
    TEInit();
    /* Init Dialog Manager */
    InitDialogs(NULL);
    cold = 0;
    
    /* Get base system version (only used if there's no better selector) */
    if (Gestalt(gestaltSystemVersion, &mac_gestalts.sysvers) != noErr ||
	(mac_gestalts.sysvers &= 0xffff) < 0x700)
	fatalbox("PuTTY requires System 7 or newer");
    /* Find out if we've got Color Quickdraw */
    if (Gestalt(gestaltQuickdrawVersion, &mac_gestalts.qdvers) != noErr)
    	mac_gestalts.qdvers = gestaltOriginalQD;
    /* ... and the Appearance Manager? */
    if (Gestalt(gestaltAppearanceVersion, &mac_gestalts.apprvers) != noErr)
	if (Gestalt(gestaltAppearanceAttr, NULL) == noErr)
	    mac_gestalts.apprvers = 0x0100;
	else
	    mac_gestalts.apprvers = 0;
#if TARGET_RT_MAC_CFM
    /* Paranoia: Did we manage to pull in AppearanceLib? */
    if (&RegisterAppearanceClient == kUnresolvedCFragSymbolAddress)
	mac_gestalts.apprvers = 0;
#endif
#if TARGET_CPU_68K
    mac_gestalts.cntlattr = 0;
    mac_gestalts.windattr = 0;
#else
    /* Mac OS 8.5 Control Manager (proportional scrollbars)? */
    if (Gestalt(gestaltControlMgrAttr, &mac_gestalts.cntlattr) != noErr ||
	&SetControlViewSize == kUnresolvedCFragSymbolAddress)
	mac_gestalts.cntlattr = 0;
    /* Mac OS 8.5 Window Manager? */
    if (Gestalt(gestaltWindowMgrAttr, &mac_gestalts.windattr) != noErr ||
	&SetWindowContentColor == kUnresolvedCFragSymbolAddress)
	mac_gestalts.windattr = 0;
#endif
    /* Text Encoding Conversion Manager? */
    if (
#if TARGET_RT_MAC_CFM
	&TECGetInfo == kUnresolvedCFragSymbolAddress ||
#else
	InitializeUnicodeConverter(NULL) != noErr ||
#endif
	TECGetInfo(&ti) != noErr)
	mac_gestalts.encvvers = 0;
    else {
	mac_gestalts.encvvers = (*ti)->tecVersion;
	mac_gestalts.uncvattr = (*ti)->tecUnicodeConverterFeatures;
	DisposeHandle((Handle)ti);
    }

    mactcp_init();

    /* We've been tested with the Appearance Manager */
    if (mac_gestalts.apprvers != 0)
	RegisterAppearanceClient();

    menuBar = GetNewMBar(128);
    if (menuBar == NULL)
	fatalbox("Unable to create menu bar.");
    SetMenuBar(menuBar);
    AppendResMenu(GetMenuHandle(mApple), 'DRVR');
    mac_adjustmenus();
    DrawMenuBar();
    InitCursor();
    windows.about = NULL;
    windows.licence = NULL;

    default_protocol = DEFAULT_PROTOCOL;
    default_port = DEFAULT_PORT;
    flags = FLAG_INTERACTIVE;

    /*
     * Really grotty hack to ensure that anything that looks at the
     * global "cfg" variable gets something vaguely sensible.
     * Obviously, nothing should actually be using it, but that will
     * take a while to arrange.
     */
    do_defaults(NULL, &cfg);

    {
	short vol;
	long dirid;

	/* Set the default directory for loading and saving settings. */
	/* XXX Should we create it? */
	if (get_session_dir(FALSE, &vol, &dirid) == noErr) {
	    LMSetSFSaveDisk(-vol);
	    LMSetCurDirStore(dirid);
	}
    }
    init_ucs();
}

static void mac_eventloop(void) {
    Boolean gotevent;
    EventRecord event;
    RgnHandle cursrgn;

    cursrgn = NewRgn();
    for (;;) {
    	mac_adjustcursor(cursrgn);
	gotevent = WaitNextEvent(everyEvent, &event, LONG_MAX, cursrgn);
	mac_adjustcursor(cursrgn);
	if (gotevent)
	    mac_event(&event);
	mactcp_poll();
	mac_pollterm();
    }
    DisposeRgn(cursrgn);
}

static void mac_event(EventRecord *event) {
    short part;
    WindowPtr window;
    Point pt;

    switch (event->what) {
      case mouseDown:
	part = FindWindow(event->where, &window);
	switch (part) {
	  case inMenuBar:
	    mac_adjustmenus();
	    mac_menucommand(MenuSelect(event->where));
	    break;
	  case inSysWindow:
	    SystemClick(event, window);
	    break;
	  case inContent:
	    if (window != FrontWindow())
	    	/* XXX: check for movable modal dboxes? */
		SelectWindow(window);
	    else
		mac_contentclick(window, event);
	    break;
	  case inGoAway:
	    if (TrackGoAway(window, event->where))
		mac_closewindow(window);
	    break;
	  case inDrag:
	    /* XXX: moveable modal check? */
	    DragWindow(window, event->where, &qd.screenBits.bounds);
	    break;
	  case inGrow:
	    mac_growwindow(window, event);
	    break;
	  case inZoomIn:
	  case inZoomOut:
	    if (TrackBox(window, event->where, part))
		mac_zoomwindow(window, part);
	    break;
	}
	break;
      case keyDown:
      case autoKey:
        mac_keypress(event);
        break;
      case activateEvt:
	mac_activatewindow((WindowPtr)event->message, event);
        break;
      case updateEvt:
        mac_updatewindow((WindowPtr)event->message);
        break;
      case diskEvt:
	if (HiWord(event->message) != noErr) {
	    SetPt(&pt, 120, 120);
	    DIBadMount(pt, event->message);
        }
        break;
    }
}

static void mac_contentclick(WindowPtr window, EventRecord *event) {
    short item;

    switch (mac_windowtype(window)) {
      case wTerminal:
	mac_clickterm(window, event);
	break;
      case wAbout:
	if (DialogSelect(event, &window, &item))
	    switch (item) {
	      case wiAboutLicence:
		mac_openlicence();
		break;
	    }
	break;
      case wSettings:
	mac_clickdlg(window, event);
	break;
    }
}

static void mac_growwindow(WindowPtr window, EventRecord *event) {

    switch (mac_windowtype(window)) {
      case wTerminal:
	mac_growterm(window, event);
    }
}

static void mac_activatewindow(WindowPtr window, EventRecord *event) {
    int active;

    active = (event->modifiers & activeFlag) != 0;
    mac_adjustmenus();
    switch (mac_windowtype(window)) {
      case wTerminal:
	mac_activateterm(window, active);
	break;
      case wSettings:
	mac_activatedlg(window, event);
	break;
      case wAbout:
	mac_activateabout(window, event);
	break;
    }
}

static void mac_activateabout(WindowPtr window, EventRecord *event) {
    DialogItemType itemtype;
    Handle itemhandle;
    short item;
    Rect itemrect;
    int active;

    active = (event->modifiers & activeFlag) != 0;
    GetDialogItem(window, wiAboutLicence, &itemtype, &itemhandle, &itemrect);
    HiliteControl((ControlHandle)itemhandle, active ? 0 : 255);
    DialogSelect(event, &window, &item);
}

static void mac_updatewindow(WindowPtr window) {

    switch (mac_windowtype(window)) {
      case wTerminal:
	mac_updateterm(window);
	break;
      case wAbout:
      case wSettings:
	BeginUpdate(window);
	UpdateDialog(window, window->visRgn);
	EndUpdate(window);
	break;
      case wLicence:
	mac_updatelicence(window);
	break;
    }
}

static void mac_updatelicence(WindowPtr window)
{
    Handle h;
    int len;
    long fondsize;

    SetPort(window);
    BeginUpdate(window);
    fondsize = GetScriptVariable(smRoman, smScriptSmallFondSize);
    TextFont(HiWord(fondsize));
    TextSize(LoWord(fondsize));
    h = Get1Resource('TEXT', wLicence);
    len = GetResourceSizeOnDisk(h);
    if (h != NULL) {
	HLock(h);
	TETextBox(*h, len, &window->portRect, teFlushDefault);
	HUnlock(h);
    }
    EndUpdate(window);
}

/*
 * Work out what kind of window we're dealing with.
 * Concept shamelessly nicked from SurfWriter.
 */
static int mac_windowtype(WindowPtr window) {
    int kind;
    long refcon;

    if (window == NULL)
	return wNone;
    kind = ((WindowPeek)window)->windowKind;
    if (kind < 0)
	return wDA;
    if (GetWVariant(window) == zoomDocProc)
	return wTerminal;
    refcon = GetWRefCon(window);
    if (refcon < 1024)
	return refcon;
    else
	return wSettings;
}

/*
 * Handle a key press
 */
static void mac_keypress(EventRecord *event) {
    WindowPtr window;

    window = FrontWindow();
    /*
     * Check for a command-key combination, but ignore it if it counts
     * as a meta-key combination and we're in a terminal window.
     */
    if (event->what == keyDown && (event->modifiers & cmdKey) /*&&
	!((event->modifiers & cfg.meta_modifiers) == cfg.meta_modifiers &&
	    mac_windowtype(window) == wTerminal)*/) {
	mac_adjustmenus();
	mac_menucommand(MenuKey(event->message & charCodeMask));
    } else {
	switch (mac_windowtype(window)) {
	  case wTerminal:
	    mac_keyterm(window, event);
	    break;
	}
    }       
}

static void mac_menucommand(long result) {
    short menu, item;
    Str255 da;
    WindowPtr window;

    menu = HiWord(result);
    item = LoWord(result);
    window = FrontWindow();
    /* Things which do the same whatever window we're in. */
    switch (menu) {
      case mApple:
        switch (item) {
          case iAbout:
	    mac_openabout();
            goto done;
          default:
            GetMenuItemText(GetMenuHandle(mApple), item, da);
            OpenDeskAcc(da);
            goto done;
        }
        break;
      case mFile:
        switch (item) {
          case iNew:
            mac_newsession();
            goto done;
	  case iOpen:
	    mac_opensession();
	    goto done;
          case iClose:
            mac_closewindow(window);
            goto done;
          case iQuit:
            cleanup_exit(0);
            goto done;
        }
        break;
    }
    /* If we get here, handling is up to window-specific code. */
    switch (mac_windowtype(window)) {
      case wTerminal:
	mac_menuterm(window, menu, item);
	break;
    }
  done:
    HiliteMenu(0);
}

static void mac_openabout(void) {
    DialogItemType itemtype;
    Handle item;
    VersRecHndl vers;
    Rect box;
    StringPtr longvers;

    if (windows.about)
	SelectWindow(windows.about);
    else {
	windows.about = GetNewDialog(wAbout, NULL, (WindowPtr)-1);
	vers = (VersRecHndl)Get1Resource('vers', 1);
	if (vers != NULL && *vers != NULL) {
	    longvers = (*vers)->shortVersion + (*vers)->shortVersion[0] + 1;
	    GetDialogItem(windows.about, wiAboutVersion,
			  &itemtype, &item, &box);
	    assert(itemtype & kStaticTextDialogItem);
	    SetDialogItemText(item, longvers);
	}
	ShowWindow(windows.about);
    }
}

static void mac_openlicence(void) {

    if (windows.licence)
	SelectWindow(windows.licence);
    else {
	windows.licence = GetNewWindow(wLicence, NULL, (WindowPtr)-1);
	ShowWindow(windows.licence);
    }
}

static void mac_closewindow(WindowPtr window) {

    switch (mac_windowtype(window)) {
      case wDA:
	CloseDeskAcc(((WindowPeek)window)->windowKind);
	break;
      case wTerminal:
	/* FIXME: end session and stuff */
	break;
      case wAbout:
	windows.about = NULL;
	CloseWindow(window);
	break;
      case wLicence:
	windows.licence = NULL;
	CloseWindow(window);
	break;
      default:
	CloseWindow(window);
	break;
    }
}

static void mac_zoomwindow(WindowPtr window, short part) {

    /* FIXME: do something */
}

/*
 * Make the menus look right before the user gets to see them.
 */
static void mac_adjustmenus(void) {
    WindowPtr window;
    MenuHandle menu;

    window = FrontWindow();
    menu = GetMenuHandle(mApple);
    EnableItem(menu, 0);
    EnableItem(menu, iAbout);

    menu = GetMenuHandle(mFile);
    EnableItem(menu, 0);
    EnableItem(menu, iNew);
    if (window != NULL)
	EnableItem(menu, iClose);
    else
	DisableItem(menu, iClose);
    EnableItem(menu, iQuit);

    switch (mac_windowtype(window)) {
      case wTerminal:
	mac_adjusttermmenus(window);
	break;
      default:
	menu = GetMenuHandle(mEdit);
	DisableItem(menu, 0);
	break;
    }
    DrawMenuBar();
}

/*
 * Make sure the right cursor's being displayed.
 */
static void mac_adjustcursor(RgnHandle cursrgn) {
    Point mouse;
    WindowPtr window, front;
    short part;

    GetMouse(&mouse);
    LocalToGlobal(&mouse);
    part = FindWindow(mouse, &window);
    front = FrontWindow();
    if (part != inContent || window == NULL || window != front) {
	/* Cursor isn't in the front window, so switch to arrow */
	SetCursor(&qd.arrow);
	SetRectRgn(cursrgn, SHRT_MIN, SHRT_MIN, SHRT_MAX, SHRT_MAX);
	if (front != NULL)
	    DiffRgn(cursrgn, front->visRgn, cursrgn);
    } else {
	switch (mac_windowtype(window)) {
	  case wTerminal:
	    mac_adjusttermcursor(window, mouse, cursrgn);
	    break;
	  default:
	    SetCursor(&qd.arrow);
	    CopyRgn(window->visRgn, cursrgn);
	    break;
	}
    }
}

void cleanup_exit(int status)
{

#if !TARGET_RT_MAC_CFM
    if (mac_gestalts.encvvers != 0)
	TerminateUnicodeConverter();
#endif
    mactcp_shutdown();
    exit(status);
}

void fatalbox(char *fmt, ...) {
    va_list ap;
    Str255 stuff;
    
    va_start(ap, fmt);
    /* We'd like stuff to be a Pascal string */
    stuff[0] = vsprintf((char *)(&stuff[1]), fmt, ap);
    va_end(ap);
    ParamText(stuff, NULL, NULL, NULL);
    StopAlert(128, NULL);
    exit(1);
}

void modalfatalbox(char *fmt, ...) {
    va_list ap;
    Str255 stuff;
    
    va_start(ap, fmt);
    /* We'd like stuff to be a Pascal string */
    stuff[0] = vsprintf((char *)(&stuff[1]), fmt, ap);
    va_end(ap);
    ParamText(stuff, NULL, NULL, NULL);
    StopAlert(128, NULL);
    exit(1);
}

/* This should only kill the current session, not the whole application. */
void connection_fatal(void *fontend, char *fmt, ...) {
    va_list ap;
    Str255 stuff;
    
    va_start(ap, fmt);
    /* We'd like stuff to be a Pascal string */
    stuff[0] = vsprintf((char *)(&stuff[1]), fmt, ap);
    va_end(ap);
    ParamText(stuff, NULL, NULL, NULL);
    StopAlert(128, NULL);
    exit(1);
}

/* Null SSH agent client -- never finds an agent. */

int agent_exists(void)
{

    return FALSE;
}

void agent_query(void *in, int inlen, void **out, int *outlen)
{

    *out = NULL;
    *outlen = 0;
}

/* Temporary null routines for testing. */

void verify_ssh_host_key(void *frontend, char *host, int port, char *keytype,
			 char *keystr, char *fingerprint)
{

}

void askcipher(void *frontend, char *ciphername, int cs)
{

}

void old_keyfile_warning(void)
{

}

char *platform_default_s(char *name)
{
    if (!strcmp(name, "Font"))
	return "Monaco";
    return NULL;
}

int platform_default_i(char *name, int def)
{
    if (!strcmp(name, "FontHeight"))
	return 9;
    /* Non-raw cut and paste of line-drawing chars works badly on the
     * current Unix stub implementation of the Unicode functions.
     * So I'm going to temporarily set the default to raw mode so
     * that the failure mode isn't quite so drastically horrid.
     * When Unicode comes in, this can all be put right. */
    if (!strcmp(name, "RawCNP"))
	return 1;
    return def;
}

/*
 * Local Variables:
 * c-file-style: "simon"
 * End:
 */
