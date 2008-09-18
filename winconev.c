/*
 * Diagnostic utility to dump out the contents of Windows console
 * events (INPUT_RECORDs).
 *
 * Should compile with MS Visual C just by saying "cl winconev.c".
 *
 * Usage: just type "winconev" and the next 100 console events
 * will be dumped to standard output. To change the limit, provide
 * a numeric argument (e.g. "winconev 1000").
 *
 * The numeric limit means I don't have to pick an event which
 * terminates the program (which would be a pain if I then wanted
 * to debug something _involving_ that event). Best way to use up
 * the event count is usually to hold down Escape, because when
 * this program terminates and returns you to the cmd.exe prompt,
 * that will ignore any Esc characters as long as its command line
 * is empty.
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    INPUT_RECORD ir;
    DWORD read;
    HANDLE standard_input = GetStdHandle(STD_INPUT_HANDLE);
    int n = 100;

    if (argc > 1)
	n = atoi(argv[1]);

    while (ReadConsoleInputW(standard_input, &ir, 1, &read) && read == 1) {
	switch (ir.EventType) {
	  case FOCUS_EVENT:
	    printf("FOCUS_EVENT bsf=%s\n",
		   ir.Event.FocusEvent.bSetFocus ? "true" : "false");
	    break;
	  case KEY_EVENT:
	    printf("KEY_EVENT bkd=%d wrc=%d wvkc=%04x wvsc=%02x uc=%04x"
		   " dwcks=%04x\n",
		   ir.Event.KeyEvent.bKeyDown,
		   ir.Event.KeyEvent.wRepeatCount,
		   ir.Event.KeyEvent.wVirtualKeyCode,
		   ir.Event.KeyEvent.wVirtualScanCode,
		   ir.Event.KeyEvent.uChar.UnicodeChar,
		   ir.Event.KeyEvent.dwControlKeyState);
	    break;
	  case MENU_EVENT:
	    printf("MENU_EVENT dwci=%08x\n",
		   ir.Event.MenuEvent.dwCommandId);
	    break;
	  case MOUSE_EVENT:
	    printf("MOUSE_EVENT dwmp=(%d,%d) dwbs=%04x dwcks=%04x"
		   " dwef=%04x\n",
		   ir.Event.MouseEvent.dwMousePosition.X,
		   ir.Event.MouseEvent.dwMousePosition.Y,
		   ir.Event.MouseEvent.dwButtonState,
		   ir.Event.MouseEvent.dwControlKeyState,
		   ir.Event.MouseEvent.dwEventFlags);
	    break;
	  case WINDOW_BUFFER_SIZE_EVENT:
	    printf("WINDOW_BUFFER_SIZE_EVENT dwsize=(%d,%d)\n",
		   ir.Event.WindowBufferSizeEvent.dwSize.X,
		   ir.Event.WindowBufferSizeEvent.dwSize.Y);
	    break;
	  default:
	    printf("unknown [%04x]\n", ir.EventType);
	    break;
	}
	if (n-- == 0)
	    break;
    }

    return 0;
}
