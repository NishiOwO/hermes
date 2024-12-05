/* a Tool (button) bar at top of window */

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/keysym.h>
#include <sys/wait.h>
#include <stdlib.h>
#include "www.h"

extern Display *display;
extern int screen;
extern int sbar_width;
extern int document;  /* HTMLDOCUMENT or TEXTDOCUMENT */
extern int hdrlen;
extern int busy;
extern unsigned long textColor, labelColor, windowColor, strikeColor,
                     windowTopShadow, windowBottomShadow, windowShadow;
extern unsigned int win_width, win_height;
extern char *buffer;
extern Cursor hourglass;
extern char *help;
extern char *printer;
extern char *SaveAsString;
extern int SaveFile;
extern int OpenURL;
extern int FindStr;
extern int FtpCtrlSkt;
extern Doc CurrentDoc, NewDoc;

#define GAP     1

static Button *buttons;  /* typedef in www.h */
static int nButtons;
static int nButtonDown;

Window win;
GC toolbar_gc;
int ToolBarHeight;

static XFontStruct *pToolFontInfo;

char *buttonNames[] =
{
    "Quit",
    "Open..",
    "Reload",
    "SaveAs..",
    "Print",
    "View",
    "Clone",
    "Find..",
    "Back",
    "Help",
    0
};

void SetToolBarWin(Window aWin)
{
    win = aWin;
}

void SetToolBarGC(GC aGC)
{
    toolbar_gc = aGC;
}

void SetToolBarFont(XFontStruct *pf)
{
    int i, x, y;

    pToolFontInfo = pf;
    ToolBarHeight = 14 + pf->max_bounds.ascent + pf->max_bounds.descent;

    x = 5;
    y = 5;

    if (!buttons)
    {
        nButtons = 0;

        while (buttonNames[nButtons])
            ++nButtons;

        buttons = (Button *)malloc(nButtons * sizeof(Button));

        for (i = 0; i < nButtons; ++i)
        {
            buttons[i].label = buttonNames[i];
            buttons[i].x = x;
            buttons[i].y = y;
            buttons[i].w = 6 + XTextWidth(pf, buttons[i].label, strlen(buttons[i].label));
            buttons[i].h = 6 + pf->max_bounds.ascent + pf->max_bounds.descent;
            x += GAP + buttons[i].w;
        }
    }
}

void DisplayToolBar()
{
    char *s;
    int x, y, r, i;
    unsigned int w, h;
    XRectangle rect;

    rect.x = x = 0;
    rect.y = y = 0;
    rect.width = w = win_width;
    rect.height = h = ToolBarHeight;

    XSetClipRectangles(display, toolbar_gc, 0, 0, &rect, 1, Unsorted);

    XSetForeground(display, toolbar_gc, windowColor);
    XFillRectangle(display, win, toolbar_gc, x, y, w, h);

    DrawOutSet(toolbar_gc, x, y, w, h);

    r = pToolFontInfo->max_bounds.ascent + 2;

    for (i = 0; i < nButtons; ++i)
    {
        x = buttons[i].x;
        y = buttons[i].y;
        w = buttons[i].w;
        h = buttons[i].h;
        s = buttons[i].label;

        XSetForeground(display, toolbar_gc, labelColor);
        XDrawString(display, win, toolbar_gc, x + 3, y + r, s, strlen(s));
        DrawOutSet(toolbar_gc, x, y, w, h);
    }
}

void PaintVersion(int bad)
{
    int i, x, y, w, h;
    char *s, msg[64];
    XRectangle rect;

    rect.x = x = buttons[nButtons-1].x + buttons[nButtons-1].w;
    rect.y = y = buttons[nButtons-1].y;
    rect.width = w = win_width - x - 2;
    rect.height = h = buttons[nButtons-1].h;

    XSetClipRectangles(display, toolbar_gc, 0, 0, &rect, 1, Unsorted);

    XSetForeground(display, toolbar_gc, windowColor);
    XFillRectangle(display, win, toolbar_gc, x, y, w, h);

    if (bad)
    {
        s = "Bad HTML";

        if (w > 100)
        {
            sprintf(msg, "Bad HTML (%o)", bad);
            s = msg;
        }
    }
    else
        s = VERSION;

    i = strlen(s);
    x = win_width - XTextWidth(pToolFontInfo, s, i) - (bad ? 4 : 5);
    y = buttons[0].y + pToolFontInfo->max_bounds.ascent + 2;
    XSetForeground(display, toolbar_gc, (bad ? strikeColor : labelColor));
    XDrawString(display, win, toolbar_gc, x, y, s, i);
}

int ToolBarButtonDown(int x, int y)
{
    int i;
    unsigned int w, h;
    XRectangle rect;

    nButtonDown = 0;  /* 0 implies no button down */

    for (i = 0; i < nButtons; ++i)
    {
        if (buttons[i].x <= x &&
            x < buttons[i].x + buttons[i].w &&
            buttons[i].y <= y &&
            y < buttons[i].y + buttons[i].h)
        {
            if (busy && i > 0)
            {
                Beep();
                return VOID;
            }

            nButtonDown = i + 1;
            x = buttons[i].x;
            y = buttons[i].y;
            w = buttons[i].w;
            h = buttons[i].h;

            rect.x = x;
            rect.y = y;
            rect.width = w;
            rect.height = h;

            XSetClipRectangles(display, toolbar_gc, 0, 0, &rect, 1, Unsorted);

            DrawInSet(toolbar_gc, x, y, w, h);
            XFlush(display);
            return TOOLBAR;
        }
    }

    return VOID;
}

void ToolBarButtonUp(int x, int y)
{
    int i, x1, y1;
    unsigned int w, h;
    XRectangle rect;
    void PrintDoc();

    if (nButtonDown)
    {
        --nButtonDown;
        x1 = buttons[nButtonDown].x;
        y1 = buttons[nButtonDown].y;
        w = buttons[nButtonDown].w;
        h = buttons[nButtonDown].h;

        rect.x = x1;
        rect.y = y1;
        rect.width = w;
        rect.height = h;

        XSetClipRectangles(display, toolbar_gc, 0, 0, &rect, 1, Unsorted);

        DrawOutSet(toolbar_gc, x1, y1, w, h);
        XFlush(display);
    }

    for (i = 0; i < nButtons; ++i)
    {
        if (buttons[i].x <= x &&
            x < buttons[i].x + buttons[i].w &&
            buttons[i].y <= y &&
            y < buttons[i].y + buttons[i].h &&
            i == nButtonDown)
        {
            if (strcmp(buttons[i].label, "Quit") == 0)
            {
                CloseFTP(); /* tidy up open FTP connection */
           /*     XCloseDisplay(display);  */
                exit(1);
            }
            else if (strcmp(buttons[i].label, "Open..") == 0)
            {
                SaveStatusString();
                SaveFile = FindStr = 0;
                OpenURL = 1;
                RestoreStatusString();
                DisplayStatusBar();
            }
            else if (strcmp(buttons[i].label, "Reload") == 0)
            {
                ReloadDoc(CurrentDoc.url, NULL);
            }
            else if (strcmp(buttons[i].label, "SaveAs..") == 0)
            {
                SaveStatusString();
                OpenURL = FindStr = 0;
                SaveFile = 1;

                if (SaveAsString)
                    free(SaveAsString);

                SaveAsString = strdup(CurrentDoc.path);
                
                RestoreStatusString();
                DisplayStatusBar();
            }
            else if (strcmp(buttons[i].label, "Find..") == 0)
            {
                SaveStatusString();
                OpenURL = SaveFile = 0;
                FindStr = 1;
                RestoreStatusString();
                DisplayStatusBar();
            }
            else if (strcmp(buttons[i].label, "Print") == 0)
                PrintDoc();
            else if (strcmp(buttons[i].label, "View") == 0)
                ToggleView();
            else if (strcmp(buttons[i].label, "Clone") == 0)
                CloneSelf();
            else if (strcmp(buttons[i].label, "Back") == 0)
                BackDoc();
            else if (strcmp(buttons[i].label, "Help") == 0)
                OpenDoc(help, NULL, (strchr(help, ':') ? REMOTE : LOCAL));
        }
    }

    nButtonDown = 0;
}

/* write buffer to temporary file and execute printing script */

void PrintDoc()
{
    int c;
    FILE *fp;
    char *tmpfile, *p, buf[256];

    XDefineCursor(display, win, hourglass);
    XFlush(display);

    tmpfile = tempnam(".", "w3");

    p = buffer;

    if ((fp = fopen(tmpfile, "w")) == 0)
        Warn("Can't open temporary print file: %s\n", tmpfile);
    else
    {
        while (c = *p++)
            putc(c, fp);

        fclose(fp);

        if (document == HTMLDOCUMENT)
            sprintf(buf, "prhtml -udi %s -d%s %s &", CurrentDoc.url, printer, tmpfile);
        else
            sprintf(buf, "prtxt -d%s %s &", printer, tmpfile);

        system(buf);
        Announce("Printing to %s ...", printer);
    }

    XUndefineCursor(display, win);
    XFlush(display);
}

/* save and display ps document */

void DisplayPSdocument(char *buffer, long length, char *path)
{
    FILE *fp;
    char *tmpfile, *p, buf[256], cmd[256];

#if 0
    XDefineCursor(display, win, hourglass);
    XFlush(display);
#endif

    p = strrchr(path, '/');

    if (p)
    {
        sprintf(buf, "/tmp%s", p);
        tmpfile = buf;
    }
    else
        tmpfile = tempnam(".", "w3");

    p = buffer;

    if ((fp = fopen(tmpfile, "w")) == 0)
        Warn("Can't open temporary postscript: file, %s\n", tmpfile);
    else
    {
        while (length-- > 0)
            putc(*p++, fp);

        fclose(fp);

        sprintf(cmd, "gs %s &", tmpfile);

        system(cmd);
        Announce("Using gs for %s", NewDoc.url);
    }

#if 0
    XUndefineCursor(display, win);
    XFlush(display);
#endif
}


/* save and display document in separate window */

void DisplayExtDocument(char *buffer, long length, int type, char *path)
{
    FILE *fp;
    char *tmpfile, *p, *app, cmd[256], buf[256];

    p = strrchr(path, '/');

    if (p)
    {
        sprintf(buf, "/tmp%s", p);
        tmpfile = buf;
    }
    else
        tmpfile = tempnam(".", "w3");

    p = buffer;

    if ((fp = fopen(tmpfile, "w")) == 0)
        Warn("Can't open temporary file: %s\n", tmpfile);
    else
    {
        while (length-- > 0)
            putc(*p++, fp);

        fclose(fp);

        if (type == DVIDOCUMENT)
            app = "xdvi";
        else if (type == PSDOCUMENT)
            app = "ghostview";
        else if (type == XVDOCUMENT)
            app = "xv";
        else if (type == MPEGDOCUMENT)
            app = "mpeg_play";
        else if (type == AUDIODOCUMENT)
            app = "showaudio";
        else if (type == XWDDOCUMENT)
            app = "xwud -in";
        else if (type == MIMEDOCUMENT)
            app = "xterm -e metamail";
        else
            app = "unknown app";

        sprintf(cmd, "w3show %s %s &", app, tmpfile);

        system(cmd);
        Announce("using %s to display %s ...", app, NewDoc.url);
    }
}


/* write buffer to file */
void SaveDoc(char *file)
{
    long i;
    int c;
    FILE *fp;
    char *p, buf[256];

    SaveFile = 0;
    XDefineCursor(display, win, hourglass);
    XFlush(display);

    p = buffer+hdrlen;

    if (*file == '\0' || (fp = fopen(file, "w")) == 0)
    {
        sprintf(buf, "can't create file: %s\n", file);
        SetStatusString(buf);
    }
    else
    {
        if (document == HTMLDOCUMENT)
            fprintf(fp, "<BASE href=\"%s\">\n", CurrentDoc.url);

        for (i = CurrentDoc.length-hdrlen; i > 0; --i)
            putc(*p++, fp);

        fclose(fp);

        Announce("saved to %s", file);
    }

    XUndefineCursor(display, win);
    XFlush(display);
}
