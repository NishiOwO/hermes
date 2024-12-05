/* a status bar at bottom on window */

#include <stdio.h>
#include <stdarg.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/keysym.h>
#include "www.h"

extern int debug;
extern int initialised;   /* if false it is unsafe to do X output */
extern int busy;
extern Display *display;
extern int screen;
extern int sbar_width;
extern XFontStruct *pFontInfo;
extern unsigned long textColor, labelColor, windowColor, strikeColor,
                     windowTopShadow, windowBottomShadow, statusColor;
extern unsigned int win_width, win_height;
extern char *user;
extern char *gatewayUser;
extern int ToolBarHeight;
extern Doc CurrentDoc, NewDoc;

#define STATSIZ 256
#define ActiveTextColor labelColor

Window win;
GC status_gc;
int statusHeight;
char status[STATSIZ];
char authreq[STATSIZ];
int AbortFlag;
int AbortButton = 1;
int AbortButtonChanged = 0;
int ABup = 1;
int OpenURL;
int FindStr;
int IsIndex;
int SaveFile;
int Authorize;
int statusOffset;
int cursor; /* position of cursor */

/* save strings for specific funcions */

char *OpenString;
char *SaveAsString;
char *FindStrVal;
char *SearchStrVal;
char *FindNextStr;

static int charheight;
static XFontStruct *pStatusFontInfo;
static Button abButton;

void SetStatusWin(Window aWin)
{
    win = aWin;
}

void SetStatusGC(GC aGC)
{
    status_gc = aGC;
}

void SetStatusFont(XFontStruct *pf)
{
    pStatusFontInfo = pf;
    charheight = pf->max_bounds.ascent + pf->max_bounds.descent;
    statusHeight = (Authorize ? 16 + (charheight<<1) : 14 + charheight);
    abButton.label = "Abort";
}

char *LabelString(int *len)
{
    if (OpenURL)
    {
        *len = 5;
        return "Open:";
    }

    if (SaveFile)
    {
        *len = 7;
        return "SaveAs:";
    }

    if (FindStr)
    {
        *len = 5;
        return "Find:";
    }

    if (IsIndex)
    {
        *len = 6;
        return "Match:";
    }

    return NULL;
}

int StatusActive(void)
{
    int c;

    return (Authorize || LabelString(&c));
}


void RestoreStatusString(void)
{
    if (OpenURL)
    {
        if (OpenString)
            strcpy(status, OpenString);
        else
            *status = '\0';
    }
    else if (SaveFile)
    {
        if (SaveAsString)
            strcpy(status, SaveAsString);
        else
            *status = '\0';
    }
    else if (FindStr)
    {
        if (FindStrVal)
            strcpy(status, FindStrVal);
        else
            *status = '\0';
    }
    else if (IsIndex)
    {
        if (SearchStrVal)
            strcpy(status, SearchStrVal);
        else
            *status = '\0';
    }

    cursor = strlen(status);
}

void SaveStatusString(void)
{
    if (OpenURL)
    {
        if (OpenString)
            free(OpenString);

        OpenString = strdup(status);
    }
    else if (SaveFile)
    {
        if (SaveAsString)
            free(SaveAsString);

        SaveAsString = strdup(status);
    }
    else if (FindStr)
    {
        if (FindStrVal)
            free(FindStrVal);

        FindStrVal = strdup(status);
    }
    else if (IsIndex)
    {
        if (SearchStrVal)
            free(SearchStrVal);

        SearchStrVal = strdup(status);
    }
}
void DrawOutSet(GC gc, int x, int y, unsigned int w, unsigned int h)
{
    XSetForeground(display, gc, windowTopShadow);

    XFillRectangle(display, win, gc, x, y, w, 1);
    XFillRectangle(display, win, gc, x, y+1, w-1, 1);
    XFillRectangle(display, win, gc, x, y, 1, h);
    XFillRectangle(display, win, gc, x+1, y+1, 1, h-1);

    XSetForeground(display, gc, windowBottomShadow);

    XFillRectangle(display, win, gc, x, y+h-1, w, 1);
    XFillRectangle(display, win, gc, x+1, y+h-2, w-1, 1);
    XFillRectangle(display, win, gc, x+w-1, y, 1, h);
    XFillRectangle(display, win, gc, x+w-2, y+1, 1, h-1);
}

void DrawInSet(GC gc, int x, int y, unsigned int w, unsigned int h)
{
    XSetForeground(display, gc, windowBottomShadow);

    XFillRectangle(display, win, gc, x, y, w, 1);
    XFillRectangle(display, win, gc, x, y+1, w-1, 1);
    XFillRectangle(display, win, gc, x, y, 1, h);
    XFillRectangle(display, win, gc, x+1, y+1, 1, h-1);

    XSetForeground(display, gc, windowTopShadow);

    XFillRectangle(display, win, gc, x, y+h-1, w, 1);
    XFillRectangle(display, win, gc, x+1, y+h-2, w-1, 1);
    XFillRectangle(display, win, gc, x+w-1, y, 1, h);
    XFillRectangle(display, win, gc, x+w-2, y+1, 1, h-1);
}

void DrawOutSetCircle(GC gc, int x, int y, unsigned int w, unsigned int h)
{
    XSetForeground(display, gc, windowTopShadow);
    XDrawArc(display, win, gc, x, y, w, h, 45<<6, 180<<6);
    XDrawArc(display, win, gc, x+1, y+1, w-2, h-2, 45<<6, 180<<6);

    XSetForeground(display, gc, windowBottomShadow);
    XDrawArc(display, win, gc, x, y, w, h, 225<<6, 180<<6);
    XDrawArc(display, win, gc, x+1, y+1, w-2, h-2, 225<<6, 180<<6);
}

void DrawInSetCircle(GC gc, int x, int y, unsigned int w, unsigned int h)
{
    XSetForeground(display, gc, windowBottomShadow);
    XDrawArc(display, win, gc, x, y, w, h, 45<<6, 180<<6);
    XDrawArc(display, win, gc, x+1, y+1, w-2, h-2, 45<<6, 180<<6);

    XSetForeground(display, gc, windowTopShadow);
    XDrawArc(display, win, gc, x, y, w, h, 225<<6, 180<<6);
    XDrawArc(display, win, gc, x+1, y+1, w-2, h-2, 225<<6, 180<<6);
}

void DisplayStatusBar()
{
    int x, y, r, n, ch, active;
    unsigned int w, h;
    char *p, *s;
    XRectangle rect;

    active = ((Authorize || (s = LabelString(&n))) ? 1 : 0);
    statusHeight = (Authorize ? 16 + (charheight<<1) : 14 + charheight);

    rect.x = x = 0;
    rect.y = y = win_height - statusHeight;
    rect.width = w = win_width;
    rect.height = h = statusHeight;

    XSetClipRectangles(display, status_gc, 0, 0, &rect, 1, Unsorted);

    XSetForeground(display, status_gc, windowColor);
    XFillRectangle(display, win, status_gc, x, y, w, h);

    DrawOutSet(status_gc, x, y, w, h);

    if (Authorize)
    {
        XSetForeground(display, status_gc, labelColor);

        rect.width -= 2;
        XSetClipRectangles(display, status_gc, 0, 0, &rect, 1, Unsorted);
        XDrawString(display, win, status_gc,
                18, y + 3 + pStatusFontInfo->max_bounds.ascent,
                authreq, strlen(authreq));
    }

    x += 50;
    y = win_height - 10 - charheight;
    h = 6 + charheight;
    w -= 70;

    XSetForeground(display, status_gc, (active ? statusColor : windowColor));
    XFillRectangle(display, win, status_gc, x, y, w, h);

    DrawInSet(status_gc, x, y, w, h);

    /* needs a call to set clipping rectangle */
    /* would currently interact with scrollbar gc */

    rect.x = x + 2;
    rect.y = y;
    rect.width = w - 4;
    rect.height = h;

    r = XTextWidth(pStatusFontInfo, "Abort", 5) + 8;

    abButton.x = 6;
    abButton.y = y + 2;
    abButton.w = r;
    abButton.h = h-3;

    if (AbortButton)
    {
        if (ABup)
            DrawOutSet(status_gc, 6, y+2, r, h-3);
        else
            DrawInSet(status_gc, 6, y+2, r, h-3);

        XSetForeground(display, status_gc, labelColor);

        XDrawString(display, win, status_gc,
                10, y + 3 + pStatusFontInfo->max_bounds.ascent,
                "Abort", 5);
    }
    else if (s)
    {
        XSetForeground(display, status_gc, labelColor);

        XDrawString(display, win, status_gc,
            8, y + 2 + pStatusFontInfo->max_bounds.ascent, s, n);
    }
    else
        XSetForeground(display, status_gc, labelColor);

    if (Authorize && (p = strchr(status, ':')))
        n = p - status + 1;
    else
        n = strlen(status);

    XSetForeground(display, status_gc, (active ? ActiveTextColor : labelColor));
    XSetClipRectangles(display, status_gc, 0, 0, &rect, 1, Unsorted);
    XDrawString(display, win, status_gc,
        x + 4 - statusOffset, y + 2 + pStatusFontInfo->max_bounds.ascent,
        status, n);

    r = XTextWidth(pStatusFontInfo, status, cursor);

    if (s || Authorize)   /* draw cursor */
    {
        XSetForeground(display, status_gc, strikeColor);
        XFillRectangle(display, win, status_gc, x+3+r - statusOffset, y+2, 1, h-4);
    }
}

void SetStatusString(char *s)
{
    char *p;
    int x, y, r, n, k, ch;
    unsigned int w, h;
    XRectangle rect;

    if (initialised)
    {
        if (s)
        {
            strncpy(status, s, STATSIZ-1);
            status[STATSIZ-1] = '\0';

            /* trim trailing \r\n */

            n = strlen(status) - 1;

            if (n > 0 && status[n] == '\n')
            {
                status[n--] = '\0';

                if (status[n] == '\r')
                     status[n] = '\0';
            }
        }

        ch = pStatusFontInfo->max_bounds.ascent + pStatusFontInfo->max_bounds.descent;
        statusHeight = (Authorize ? 16 + (charheight<<1) : 14 + ch);

        rect.x = x = 0;
        rect.y = win_height - statusHeight;
        rect.width = w = win_width;
        rect.height = statusHeight;

        XSetClipRectangles(display, status_gc, 0, 0, &rect, 1, Unsorted);
        XSetForeground(display, status_gc, windowColor);

        h = 14 + charheight;
        y = win_height - h;

        s = LabelString(&r);

        if (AbortButton)
        {
            if (AbortButtonChanged)
            {
                XFillRectangle(display, win, status_gc, 6, y+2, 42, h-4);

                if (ABup)
                    DrawOutSet(status_gc, abButton.x, abButton.y, abButton.w, abButton.h);
                else
                    DrawInSet(status_gc, abButton.x, abButton.y, abButton.w, abButton.h);

                XSetForeground(display, status_gc, labelColor);

                XDrawString(display, win, status_gc,
                        abButton.x + 4, abButton.y + 1 + pStatusFontInfo->max_bounds.ascent,
                        "Abort", 5);

                AbortButtonChanged = 0;  /* avoid flickering buffer */
            }
        }
        else if (s)
        {
            XFillRectangle(display, win, status_gc, 6, y+2, 42, h-4);
            XSetForeground(display, status_gc, labelColor);

            XDrawString(display, win, status_gc,
                8, y + 6 + pStatusFontInfo->max_bounds.ascent, s, r);
        }
        else
            XFillRectangle(display, win, status_gc, 6, y+2, 42, h-4);

        rect.x = x = 52;
        rect.y = y = win_height - 8 - charheight;
        rect.width = w = win_width - 74;
        rect.height = h = 2 + charheight;

        XSetClipRectangles(display, status_gc, 0, 0, &rect, 1, Unsorted);

        XSetForeground(display, status_gc, ((s||Authorize) ? statusColor : windowColor) );
        XFillRectangle(display, win, status_gc, x, y, w, h);

        cursor = n = strlen(status);

        if (Authorize && (p = strchr(status, ':')))
            n = p - status + 1;

        XSetForeground(display, status_gc, ((s || Authorize) ? ActiveTextColor : labelColor));
        XDrawString(display, win, status_gc,
            x + 2 - statusOffset, y + pStatusFontInfo->max_bounds.ascent,
            status, n);

        r = XTextWidth(pStatusFontInfo, status, cursor);

        if (s||Authorize)   /* draw cursor */
        {
            XSetForeground(display, status_gc, strikeColor);
            XFillRectangle(display, win, status_gc, x+1+r - statusOffset, y, 1, h);
        }
    }
    else if (s)
    {
        strncpy(status, s, STATSIZ-1);
        status[STATSIZ-1] = '\0';

        /* trim trailing \r\n */

        n = strlen(status) - 1;

        if (n > 0 && status[n] == '\n')
        {
            status[n--] = '\0';

            if (status[n] == '\r')
                status[n] = '\0';
        }

        cursor = strlen(status);
    }
}


void RepairStatus(int x1, int moved)
{
    char *p;
    int x, y, r, n, active;
    unsigned int w, h;
    XRectangle rect;

    rect.x = x = 52;
    rect.y = y = win_height - 8 - charheight;
    rect.width = w = win_width - 74;
    rect.height = h = 2 + charheight;

    if (!moved)
    {
        if (rect.x < x1)
        {
            rect.width -= x1 - x;
            rect.x = x1;
        }

/*
        r = statusHeight;

        if (r < rect.width)
            rect.width = r;
*/    }

    active = ((Authorize || LabelString(&n)) ? 1 : 0);
    XSetClipRectangles(display, status_gc, 0, 0, &rect, 1, Unsorted);

    XSetForeground(display, status_gc, (active ? statusColor : windowColor));
    XFillRectangle(display, win, status_gc, x, y, w, h);

    if (Authorize && (p = strchr(status, ':')))
        n = p - status + 1;
    else
        n = strlen(status);

    XSetForeground(display, status_gc, (active ? ActiveTextColor : labelColor));
    XDrawString(display, win, status_gc,
        x + 2 - statusOffset, y + pStatusFontInfo->max_bounds.ascent,
        status, n);

    r = XTextWidth(pStatusFontInfo, status, cursor);

    if (Authorize || LabelString(&n))   /* draw cursor */
    {
        XSetForeground(display, status_gc, strikeColor);
        XFillRectangle(display, win, status_gc, x+1+r - statusOffset, y, 1, h);
    }
}


void ClearStatus()
{
    cursor = 0;
    status[0] = '\0';
    statusOffset = 0;
}

int IsEditChar(char c)
{
    if (c == '\b' || c == '\n' || c == '\r')
        return 1;

    if (c >= ' ')
        return 1;

    return 0;
}

void EditChar(char c)
{
    char *who;
    int i, n, x1, x2, moved;

    /* x1 is position of cursor, x2 is right edge of clipped text */

    n = strlen(status);

    if (busy && !Authorize)
        Beep();
    else if (c == '\b')
    {
        if (cursor > 0)
        {
            --cursor;
            strcpy(status+cursor, status+cursor+1);
            x1 = 54 + XTextWidth(pStatusFontInfo, status, cursor);
            x2 = 52 + win_width - 72 - 4;
            n = (x1 > x2 ? x1 - x2 : 0);
            moved = ( (n == statusOffset) ? 0 : 1);
            statusOffset = n;
            RepairStatus(x1 - statusOffset - 1, moved);
        }
        else
            XBell(display, 0);
    }
    else if (c == 127)  /* DEL */
    {
        if (cursor < strlen(status))
        {
            strcpy(status+cursor, status+cursor+1);
            x1 = 54 + XTextWidth(pStatusFontInfo, status, cursor);
            x2 = 52 + win_width - 72 - 4;
            n = (x1 > x2 ? x1 - x2 : 0);
            moved = ( (n == statusOffset) ? 0 : 1);
            statusOffset = n;
            RepairStatus(x1 - statusOffset - 1, moved);
        }
        else
            XBell(display, 0);
    }
    else if ((Authorize || LabelString(&i)) && (c == '\n' || c == '\r') )
    {
        SaveStatusString();

        if (Authorize)
        {
            if (Authorize == GATEWAY)
            {
                if (gatewayUser)
                    free(gatewayUser);

                gatewayUser = strdup(status);
                Authorize = 0;
                busy = 0;
                status[0] = '\0';
                DisplayStatusBar();
                DisplaySizeChanged(0);
                DisplayScrollBar();
                i = 2 + charheight;
                DisplayDoc(WinLeft, WinBottom-i, WinWidth, i);
                XFlush(display);
                OpenDoc(NewDoc.url, 0, (strchr(NewDoc.url, ':') ? REMOTE : LOCAL));
            }
            else
            {
                who = strdup(status);
                Authorize = 0;
                busy = 0;
                status[0] = '\0';
                DisplayStatusBar();
                DisplaySizeChanged(0);
                DisplayScrollBar();
                i = 2 + charheight;
                DisplayDoc(WinLeft, WinBottom-i, WinWidth, i);
                XFlush(display);
                OpenDoc(NewDoc.url, who, (strchr(NewDoc.url, ':') ? REMOTE : LOCAL));
                free(who);
            }
        }
        else if (OpenURL)
            OpenDoc(status, NULL, (strchr(NewDoc.url, ':') ? REMOTE : LOCAL));
        if (SaveFile)
            SaveDoc(status);
        else if (FindStr)
        {
            FindNextStr = 0;
            FindString(status, &FindNextStr);
        }
        else if (IsIndex)
            SearchIndex(status);
    }
    else if (c >= ' ')
    {
        if (n < STATSIZ-1)
        {
            for (i = n; i >= cursor; --i)
                status[i+1] = status[i];

            status[cursor++] = c;
            
            x1 = 53 + XTextWidth(pStatusFontInfo, status, cursor);
            x2 = 52 + win_width - 72 - 4;
            n = (x1 > x2 ? x1 - x2 : 0);
            moved = ( (n == statusOffset) ? 0 : 1);
            statusOffset = n;
            RepairStatus(x1 - statusOffset - XTextWidth(pStatusFontInfo, &c, 1), moved);
        }
        else
            XBell(display, 0);
    }
}

void MoveStatusCursor(int key)
{
    int was, x, y, x1, x2, moved, r, n;
    unsigned int w, h;
    XRectangle rect;

    was = cursor;

    if (key == XK_Left)
    {
        if (cursor > 0)
        {
            --cursor;
            x1 = 54 + XTextWidth(pStatusFontInfo, status, cursor);
            x2 = 52 + win_width - 72 - 4;
            n = (x1 > x2 ? x1 - x2 : 0);
            moved = ( (n == statusOffset) ? 0 : 1);
            statusOffset = n;

            if (moved)
                RepairStatus(x1 - statusOffset - 1, moved);
        }
        else
            XBell(display, 0);
    }
    else if (key == XK_Right)
    {
        if (cursor < strlen(status))
        {
            ++cursor;
            x1 = 53 + XTextWidth(pStatusFontInfo, status, cursor);
            x2 = 52 + win_width - 72 - 4;
            n = (x1 > x2 ? x1 - x2 : 0);
            moved = ( (n == statusOffset) ? 0 : 1);
            statusOffset = n;

            if (moved)
                RepairStatus(x1 - statusOffset - XTextWidth(pStatusFontInfo, status+cursor-1, 1), moved);
        }
        else
            XBell(display, 0);
    }

    if (!moved && was != cursor)
    {


        rect.x = x = 52;
        rect.y = y = win_height - 8 - charheight;
        rect.width = w = win_width - 74;
        rect.height = h = 2 + charheight;

        XSetClipRectangles(display, status_gc, 0, 0, &rect, 1, Unsorted);

        XSetForeground(display, status_gc, statusColor);
        r = XTextWidth(pStatusFontInfo, status, was);
        XFillRectangle(display, win, status_gc, x+1+r - statusOffset, y, 1, h);

        XSetForeground(display, status_gc, strikeColor);
        r = XTextWidth(pStatusFontInfo, status, cursor);
        XFillRectangle(display, win, status_gc, x+1+r - statusOffset, y, 1, h);
    }
}

void Announce(char *args, ...)
{
    va_list ap;
    char buf[256];

    if (initialised)
    {
        va_start(ap, args);
        vsprintf(buf, args, ap);
        va_end(ap);

        if (debug)
            fprintf(stderr, "%s\n", buf);

        SetStatusString(buf);
        XFlush(display);
    }
    else
    {
        va_start(ap, args);
        vsprintf(buf, args, ap);
        va_end(ap);

        SetStatusString(buf);

        if (debug)
            fprintf(stderr, "%s\n", buf);
    }
}

void Warn(char *args, ...)
{
    va_list ap;
    char buf[256];

    if (initialised)
    {
        va_start(ap, args);
        vsprintf(buf, args, ap);
        va_end(ap);

        if (debug)
            fprintf(stderr, "%s\n", buf);

        SetStatusString(buf);
        XBell(display, 0);
        XFlush(display);
    }
    else
    {
        va_start(ap, args);
        vsprintf(buf, args, ap);
        va_end(ap);

        SetStatusString(buf);
        fprintf(stderr, "%s\n", buf);
    }
}

void Beep()
{
    XBell(display, 0);
    XFlush(display);
}

int StatusButtonDown(int button, int px, int py)
{
    char *s;
    int x, y, n;
    unsigned int w, h;
    XRectangle rect;

    x = abButton.x;
    y = abButton.y;
    w = abButton.w;
    h = abButton.h;

    rect.x = x;
    rect.y = y;
    rect.width = w;
    rect.height = h;

    if (button == Button1 && AbortButton &&
        x <= px &&
        px < x + w &&
        y <= py &&
        py < y + h)
    {
        ABup = 0;
        XSetClipRectangles(display, status_gc, 0, 0, &rect, 1, Unsorted);
        DrawInSet(status_gc, x, y, w, h);
        return STATUS;
    }

    /* now check for middle or right button down over status window */

    x = 52;
    y = win_height - statusHeight + 6;
    w = win_width - 74;
    h = statusHeight - 12;

    if ((button == Button2 || button == Button3) && LabelString(&n) &&
        x <= px &&
        px < x + w &&
        y <= py &&
        py < y + h)
    {
        s = XFetchBytes(display, &n);

        if (s)
        {
            x = strlen(status);
            if (x + n > STATSIZ-1)
                n = STATSIZ-1-x;

            strncpy(status+x, s, n);
            status[x+n] = '\0';
            cursor = x+n;

            XFree(s);
            Redraw(x, y, w, h);
        }
    }

    return VOID;
}

void HideAuthorizeWidget(void)
{
    AbortButton = 0;
    Authorize = 0;
    busy = 0;
    ClearStatus();
    DisplayStatusBar();
    DisplaySizeChanged(0);
    DisplayScrollBar();
    DisplayDoc(WinLeft, WinTop, WinWidth, WinHeight);
    XFlush(display);
}

void StatusButtonUp(int px, int py)
{
    int x, y;
    unsigned int w, h;
    XRectangle rect;

    x = abButton.x;
    y = abButton.y;
    w = abButton.w;
    h = abButton.h;

    rect.x = x;
    rect.y = y;
    rect.width = w;
    rect.height = h;

    if (AbortButton)
    {
        /* redraw button in up state */

        if (ABup == 0)
        {
            ABup = 1;
            XSetClipRectangles(display, status_gc, 0, 0, &rect, 1, Unsorted);
            DrawOutSet(status_gc, x, y, w, h);
        }

        /* check if up event occurs in button */

        if (x <= px &&
            px < x + w &&
            y <= py &&
            py < y + h)
        {
            AbortFlag = 1;

            if (Authorize)  /* hide password widget */
                HideAuthorizeWidget();
        }
    }
}

void ShowAbortButton(int n)
{
    AbortFlag = 0;
    AbortButton = n;
    AbortButtonChanged = 1;
}

/* reconfigure status bar to ask for authorisation
   mode is REMOTE for remote hosts, and GATEWAY for the gateway */

void GetAuthorization(int mode, char *host)
{
    Authorize = mode;
    busy = 1;
    ClearStatus();

    ShowAbortButton(1);

    if (mode == GATEWAY)
    {
        strcpy(authreq, "Enter name:password for gateway ");
        strncpy(authreq+32, host, STATSIZ-33);
    }
    else
    {
        strcpy(authreq, "Enter name:password for ");
        strncpy(authreq+24, host, STATSIZ-25);
    }


    authreq[STATSIZ-1] = '\0';

    if (user)
        sprintf(status, "%s:", user);
    else
        status[0] = '\0';

    cursor = strlen(status);

    if (initialised)
    {
        DisplayStatusBar();
        DisplaySizeChanged(0);
        DisplayScrollBar();
    }
    else
        DisplaySizeChanged(0);
}

/* extract name from "name:password" */

char *UserName(char *who)
{
    char *p;
    static char name[32];

    strncpy(name, who, 30);
    name[31] = '\0';
    p = strchr(name, ':');

    if (p)
        *p = '\0';
    return name;
}

/* extract password from "name:password" */

char *PassStr(char *who)
{
    char *p;

    p = strchr(who, ':');

    return (p ? p+1 : "");
}
