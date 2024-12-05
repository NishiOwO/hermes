/* forms.c - forms handling for html+

ParseHTML() creates a linked list of forms. Each form has a linked
list of fields. This file includes routines for drawing fields,
handling mouse clicks/drags and key presses on fields, plus sending
the contents of forms to HTTP servers.

When resizing windows, it is important to keep the existing form
data structures as otherwise the current values of fields will be
lost and overridden by the original starting values. This requires
the global new_form to be set to false. It should be set to true
when reading new documents. In addition, The browser history list
needs to preserve the form data structures so that they can be
reused upon backtracking to restore previous field values.

The desire to evolve to a wysiwyg editor means that its a bad idea
to have pointers into the paint stream as this would be expensive
to update when users edit the html document source . Consequently,
one has to search the form structure to find fields that need to
be redrawn under programatic control, e.g. unsetting of radio
buttons. Note that anyway, the y-position of a field is unresolved
when it is added to the paint stream, and only becomes fixed when
the end of the line is reached. A compromise is to cache the
baseline position when painting each field and trash the cache
each time the user alters the document.

Another complication is the need to save current field values
when following a link to another document, as otherwise there
is now way of restoring them when the user backtracks to the
document containing the form. A simple approach is to save the
linked list of forms in memory as part of a memory resident
history list. Note for local links, the same form data should
be used, rather than recreating the list from new. The corollary
is that the list should only be freed when backtracking the
document containing the form or reloading the same.

Note that in many cases a sequence of form interactions with
a server causes a state change in the server (e.g. updating a
database) and hence is not simply reversed by backtracking in
the normal way. In this case it makes sense to avoid pushing
intermediate steps onto the history stack, e.g. when submitting
the form. This probably requires the document or server to
disable the history nechanism via suitable attributes.

*/

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <string.h>
#include <ctype.h>
#include "www.h"

extern Display *display;
extern int screen;
extern Window win;
extern GC disp_gc, gc_fill;
extern Cursor hourglass;
extern int UsePaper;

extern int debug;  /* controls display of errors */
extern int document;  /* HTMLDOCUMENT or TEXTDOCUMENT */
extern int busy;
extern int OpenURL;
extern int IsIndex;
extern int FindStr;
extern char *FindNextStr;
extern int SaveFile;
extern int sbar_width;
extern int statusHeight;
extern int ToolBarHeight;
extern unsigned long windowColor;
extern unsigned int win_width, win_height, tileWidth, tileHeight;
extern XFontStruct *h1_font, *h2_font, *h3_font,
        *normal_font, *italic_font, *bold_font, *fixed_i_font,
        *fixed_b_font, *fixed_font, *legend_font;

extern unsigned long textColor, labelColor, windowTopShadow,
                     strikeColor, windowBottomShadow, statusColor, windowColor;

/*
    The current top line is displayed at the top of the window,the pixel
    offset is the number of pixels from the start of the document.
*/

extern char *buffer;            /* the start of the document buffer */
extern long PixelOffset;        /* the pixel offset to top of window */
extern int PixelIndent;
extern Doc NewDoc, CurrentDoc;
extern XFontStruct *pFontInfo;
extern XFontStruct *Fonts[FONTS];
extern int LineSpacing[FONTS], BaseLine[FONTS], StrikeLine[FONTS];
extern int preformatted;
extern int font;  /* index into Fonts[] array */
extern int above, below;
extern XRectangle displayRect; /* clipping limits for painting html */

Form *forms = NULL;
Field *focus;   /* which field has focus (NULL if none do) */
int cursorpos = 5; /* cursor position in characters */
int new_form;   /* if false then reuse existing data structures */

/* version of strdup for use with non-terminated strings */

char *strdup2(char *s, int len)
{
    char *str;

    str = (char *)malloc(len + 1);
    memcpy(str, s, len);
    str[len] = '\0';
    return str;
}

void FreeForms(void)
{
    Option *option;
    Form *form;
    Field *fields, *field;

    new_form = 1;

    while (forms != NULL)
    {
        form = forms;
        forms = forms->next;
        fields = form->fields;

        while (fields)
        {
            field = fields;
            free(field->name);
            free(field->value);
            fields = fields->next;

            while (field->options)
            {
                option = field->options;

                if (option->label)
                    free(option->label);

                field->options = option->next;
                free(option);
            }

            free(field);
        }

        if (form->action)
            free(form->action);

        free(form);
    }
}

/*
   Preserves form contents across window resizing / backtracking.
   Multiple FORM elements with the same ACTION URL will be treated
   as if they belong to the same form! It is assumed that FreeForms()
   is called before loading new documents or reloading existing ones.
*/

Form *GetForm(char *url, int len)
{
    Form *form;

    /* is form already defined */

    for (form = forms; form != NULL; form = form->next)
    {
        if (strlen(form->action) == len && strncasecmp(url, form->action, len) == 0)
        {
            form->i = 0;
            return form;
        }
    }

    form = (Form *)malloc(sizeof(Form));

    form->next = forms;
    form->fields = NULL;
    form->action = strdup2(url, len);
    form->i = 0;       /* reset field index */
    forms = form;
    return form;
}

Form *DefaultForm(void)
{
    return GetForm(CurrentDoc.url, strlen(CurrentDoc.url));
}

/* font should be passed in lower 4 bits of flags */

Field *GetField(Form *form, int type, int x, char *name, int nlen,
               char *value, int vlen, int rows, int cols , int flags)
{
    int em, font, y_above, y_below;
    Field *field;

    if (form == NULL)
        return NULL;

    /* if we are resizing the form or backtracking to it then
       we need to resuse the existing data structures to preserve
       any chances that have been made to the default values */

    if (!new_form)
    {
        for (field = form->fields; field != NULL; field = field->next)
        {
            if (field->i == form->i)
            {
                form->i += 1;
                field->x = x;
                field->baseline = -1; /* invalidate position */
                font = flags & 0x0F;
                goto set_size;
            }
        }

        Warn("Can't find field %d in form for %s", form->i, form->action);
        return NULL;
    }

    field = (Field *)malloc(sizeof(Field));

    if (type == SUBMITBUTTON || type == RESETBUTTON)
    {
        flags &= 0xF0;
        flags |= IDX_H3FONT;
    }

    font = flags & 0x0F;
    field->next = form->fields;
    field->form = form;
    form->fields = field;
    field->i = form->i;
    form->i += 1;

    field->type = type;
    field->name = strdup2(name, nlen);
    field->value = strdup2(value, vlen);
    field->bufsize = vlen+1;
    field->buflen = vlen;    
    field->flags = flags;
    field->x = x;
    field->x_indent = 0;
    field->y_indent = 0;
    field->baseline = -1;
    field->options = 0;

    em = WIDTH(font, "m", 1);

 set_size:
    field->j = 0;  /* reset option index */

    if (type == RADIOBUTTON || type == CHECKBOX)
    {
        field->width = ASCENT(font) + DESCENT(font) - 2;
        field->height = field->width;

        y_above = ASCENT(font) - 2;

        if (above < y_above)
            above = 1 + y_above;

        y_below = field->height - y_above;

        if (below < y_below)
            below = 1 + y_below;
    }
    else  /* TEXTFIELD and OPTIONLIST */
    {
        if (type == SUBMITBUTTON)
                field->width = 8 + WIDTH(font, " Submit Query ", 14);
        else if (type == RESETBUTTON)
                field->width = 8 + WIDTH(font, " Reset ", 7);
        else
                field->width = 8 + cols * em;

        field->height = 4 + rows * SPACING(Fonts[font]);

        y_above = ASCENT(font) + 2;

        if (above < y_above)
            above = 1 + y_above;

        y_below = field->height - y_above;

        if (below < y_below)
            below = 1 + y_below;
    }

    return field;
}

Option *AddOption(Field *field, int flags, char *label, int len)
{
    int width, font;
    Option *option, *options;

    /* if we are resizing the form or backtracking to it then
       we need to resuse the existing data structures to preserve
       any chances that have been made to the default values */

    if (!new_form)
    {
        for (option = field->options; option != NULL; option = option->next)
        {
            if (option->j == option->j)
            {
                field->j += 1;
                font = flags & 0x0F;
                width = 6 + WIDTH(font, label, len) + field->height;

                if (width > field->width)
                    field->width = width;

                return option;
            }
        }

        Warn("Can't find option %d in field for %s", field->i, field->name);
        return NULL;
    }

    option = (Option *)malloc(sizeof(Option));

    option->next = NULL;
    option->flags = flags;
    option->j = field->j;
    field->j += 1;
    option->label = strdup(label);
    font = flags & 0xF;
    width = 6 + WIDTH(font, label, len) + field->height;

    field->y_indent++;

    if (width > field->width)
            field->width = width;

    if (field->options)
    {
        options = field->options;

        while (options->next)
            options = options->next;

        options->next = option;
    }
    else
        field->options = option;
}

Field *CurrentRadioButton(Form *form, char *name)
{
    Field *field;

    if (form == NULL)
        return NULL;

    field = form->fields;

    while (field)
    {
        if (strcmp(field->name, name) == 0 && (field->flags & CHECKED))
            return field;

        field = field->next;
    }

    return field;
}

void HideDropDown(GC gc, Field *field)
{
    int width, height, h, x1, y1;

    height = field->height;
    width = field->width - height + 2;
    x1 = field->x - PixelIndent;
    y1 = height + Abs2Win(field->baseline) - ASCENT(font) - 2;
    DisplayHTML(x1, y1, width, 6 + field->y_indent * (height - 4));
    ClipToWindow();
}

/* called when testing mouse down/up (event = BUTTONDOWN or BUTTONUP) */
int ClickedInField(GC gc, int baseline, Field *field, int x, int y, int event)
{
    int y1;
    Field *field1;

    if (field->type == CHECKBOX || field->type == RADIOBUTTON)
        y1 = baseline - ASCENT(font) + 2;
    else if (field->type == OPTIONLIST)
        y1 = baseline - ASCENT(font) - 2;
    else   /* TEXTFIELD */
        y1 = baseline - ASCENT(font) - 2;

    if (field->x <= x && x < field->x + field->width &&
                y1 <= y && y < y1 + field->height)
    {
        if (event == BUTTONUP)
        {
            /* remove focus from current field */

            if (focus && focus != field)
            {
                focus->flags &= ~CHECKED;

                if (focus->type == OPTIONLIST)
                    HideDropDown(gc, focus);
                else if (focus->type == TEXTFIELD)
                    PaintField(gc, Abs2Win(focus->baseline), focus);

                focus = NULL;
            }

            if (field->type == RADIOBUTTON)
            {
                /* deselect matching radio button */
                field1 = CurrentRadioButton(field->form, field->name);

                if (field1 && field1 != field)
                {
                    field1->flags  &= ~CHECKED;

                    if (field1->baseline >= 0)
                    PaintField(gc, Abs2Win(field1->baseline), field1);
                }

                if (!(field->flags & CHECKED))
                {
                    field->flags |= CHECKED;
                    PaintField(gc, baseline, field);
                }
            }
            else if (field->type == CHECKBOX)
            {
                if (field->flags & CHECKED)
                    field->flags &= ~CHECKED;
                else
                    field->flags |= CHECKED;

                PaintField(gc, baseline, field);
            }

            if (field->type == OPTIONLIST)
            {
                if (field->flags & CHECKED)
                {
                    field->flags &= ~CHECKED;
                    HideDropDown(gc, field);
                }
                else
                {
                    field->flags |= CHECKED;
                    PaintDropDown(gc, field);
                    focus = field;
                }
            }
            else if (field->type == TEXTFIELD)
            {
                field->flags |= CHECKED;
                PaintField(gc, baseline, field);
                focus = field;
            }
        }

        return 1;
    }

    return 0;
}

/* set clip rectangle to intersection with displayRect
   to clip text strings in text fields */

int ClipIntersection(GC gc, int x, int y, unsigned int width, unsigned int height)
{
    int xl, yl, xm, ym;
    XRectangle rect;

    xl = x;
    xm = x + width;
    yl = y;
    ym = y + height;

    if (xl < displayRect.x)
        xl = displayRect.x;

    if (yl < displayRect.y)
        yl = displayRect.y;

    if (xm > displayRect.x + displayRect.width)
        xm = displayRect.x + displayRect.width;

    if (ym > displayRect.y + displayRect.height)
        ym = displayRect.y + displayRect.height;

    if (xm > xl && ym > yl)
    {
        rect.x = xl;
        rect.y = yl;
        rect.width = xm - xl;
        rect.height = ym - yl;
        XSetClipRectangles(display, gc, 0, 0, &rect, 1, Unsorted);
    }

    return 0;
}

int ClickedInDropDown(GC gc, Field *field, int event, int x, int y)
{
    int font, baseline, bw, bh, dh, x1, y1, y2, n, i;
    Option *option;

    font = field->flags & 0x0F;
    baseline = Abs2Win(field->baseline);
    bw = field->width - field->height + 2;
    bh = 6 + field->y_indent * (field->height - 4);
    dh = field->height;
    x1 = field->x - PixelIndent;
    y1 = field->height + baseline - ASCENT(font) - 2;
    n = field->x_indent;

    if (x1 + 2 < x && x < x1 + bw - 2 && y1 + 2 < y && y < y1 + bh)
    {
        if (event == BUTTONDOWN)
            return 1;

        n = ((y - y1) * field->y_indent) / bh;
        focus->x_indent = n;

        option = field->options;

        while (n-- > 0)
            option = option->next;

        focus->flags &= ~CHECKED;
        free(focus->value);
        focus->value = strdup(option->label);
        ClipToWindow();
        PaintField(gc, Abs2Win(focus->baseline), focus);
        HideDropDown(gc, focus);
        return 1;
    }

    return 0;
}

void PaintDropDown(GC gc, Field *field)
{
    int font, baseline, width, height, h, x1, y1, y2, n, i;
    Option *option;

    baseline = Abs2Win(field->baseline);
    font = field->flags & 0x0F;
    SetFont(gc, font);
    height = field->height;
    width = field->width - height + 2;
    x1 = field->x - PixelIndent;
    y1 = height + baseline - ASCENT(font) - 2;
    n = field->x_indent;
    h = 6 + field->y_indent * (height - 4);
    y2 = baseline + height;
    option = field->options;

    XSetForeground(display, gc, windowColor);
    XFillRectangle(display, win, gc, x1, y1, width, h);
    DrawOutSet(gc, x1, y1, width, h);

    y1 += 2;

    for (option = field->options, i = 0; option; option = option->next)
    {
        if (i == n)
        {
            XSetForeground(display, gc, statusColor);
            XFillRectangle(display, win, gc, x1+2, y1, width-4, height-2);
        }

        XSetForeground(display, gc, textColor);
        XDrawString(display, win, gc, x1+4, y2, option->label, strlen(option->label));
        y2 += height - 4;
        y1 += height - 4;
        ++i;
    }
}

void PaintTickMark(GC gc, int x, int y, unsigned int w, unsigned int h)
{
    int x1, y1, x2, y2, x3, y3;

    x1 = x;
    x2 = x + w/3;
    x3 = x + w-1;
    y1 = y + h - h/3 - 1;
    y2 = y + h - 1;
    y3 = y;

    XSetForeground(display, gc, textColor);
    XDrawLine(display, win, gc, x1, y1, x2, y2);
    XDrawLine(display, win, gc, x2, y2, x3, y3);
}

void PaintCross(GC gc, int x, int y, unsigned int w, unsigned int h)
{
    XSetForeground(display, gc, strikeColor);
    XDrawLine(display, win, gc, x, y, x+w, y+w);
    XDrawLine(display, win, gc, x, y+w, x+w, y);
    XSetForeground(display, gc, textColor);
}

void PaintField(GC gc, int baseline, Field *field)
{
    char *s;
    int font, active, width, height, x1, y1, y2, r, n;
    Option *option;

    active = field->flags & CHECKED;
    font = field->flags & 0x0F;
    width = field->width;
    height = field->height;

 /* cache absolute position of baseline */
    field->baseline = Win2Abs(baseline);

    if (field->type == TEXTFIELD)
    {
        x1 = field->x - PixelIndent - field->x_indent;
        y2 = baseline - ASCENT(font) - 2;
        ClipIntersection(gc, x1, y2, width, height);
        XSetForeground(display, gc, (active ? statusColor : windowColor));
        XFillRectangle(display, win, gc, x1, y2, width, height);
        DrawInSet(gc, x1, y2, width, height);
        ClipIntersection(gc, x1+2, y2, width-4, height);

        if (field->buflen > 0)
        {
            y1 = y2 + ASCENT(font) + 2;
            SetFont(gc, font);
            XSetForeground(display, gc, textColor);
            XDrawString(display, win, gc, x1+4, y1, field->value, field->buflen);
        }

        if (active)
        {
            if (field->buflen > 0)
                r = WIDTH(font, field->value, cursorpos);
            else
                r = 0;

            XSetForeground(display, gc, strikeColor);
            XFillRectangle(display, win, gc, x1 + 3 + r, y2+2, 1, SPACING(Fonts[font]));
        }

        XSetForeground(display, gc, textColor);
        XSetClipRectangles(display, gc, 0, 0, &displayRect, 1, Unsorted);
    }
    else if (field->type == SUBMITBUTTON || field->type == RESETBUTTON)
    {
        x1 = field->x - PixelIndent - field->x_indent;
        y2 = baseline - ASCENT(font) - 2;
        ClipIntersection(gc, x1, y2, width, height);
        XSetForeground(display, gc, windowColor);
        XFillRectangle(display, win, gc, x1, y2, width, height);
        DrawOutSet(gc, x1, y2, width, height);
        ClipIntersection(gc, x1+2, y2, width-4, height);
        s = (field->type == SUBMITBUTTON ? " Submit Query " : " Reset ");

        if (field->buflen > 0)
        {
            y1 = y2 + ASCENT(font) + 2;
            SetFont(gc, font);
            XSetForeground(display, gc, textColor);
            XDrawString(display, win, gc, x1+4, y1, s, strlen(s));
        }

        XSetForeground(display, gc, textColor);
        XSetClipRectangles(display, gc, 0, 0, &displayRect, 1, Unsorted);
    }
    else if (field->type == OPTIONLIST)
    {
        x1 = field->x - PixelIndent;
        y2 = baseline - ASCENT(font) - 2;
        XSetForeground(display, gc, windowColor);
        XFillRectangle(display, win, gc, x1, y2, width, height);
        DrawOutSet(gc, x1, y2, width, height);

        if (field->flags & MULTIPLE)
        {
            DrawOutSet(gc, x1+3+width-height, y2 - 2 + height/3, height-7, 6);
            DrawOutSet(gc, x1+3+width-height, y2 - 3 + 2*height/3, height-7, 6);
        }
        else /* single choice menu drawn with one bar */
            DrawOutSet(gc, x1+3+width-height, y2 - 3 + height/2, height-7, 6);

        if (field->y_indent > 0)
        {
            option = field->options;
            n = field->x_indent;

            while (n-- > 0)
                option = option->next;

            y1 = y2 + ASCENT(font) + 2;
            SetFont(gc, font);
            XSetForeground(display, gc, textColor);
            XDrawString(display, win, gc, x1+4, y1, option->label, strlen(option->label));
        }

        XSetForeground(display, gc, textColor);
    }
    else if (field->type == CHECKBOX)
    {
        x1 = field->x - PixelIndent;
        y2 = baseline-ASCENT(font) + 2;
        XSetForeground(display, gc, (active ? statusColor : windowColor));
        XFillRectangle(display, win, gc, x1, y2, width, width);

        if (active)
        {
            PaintTickMark(gc, x1+3, y2+3, width-6, width-7);
#if 0
            XSetForeground(display, gc, windowBottomShadow);
            XFillRectangle(display, win, gc, x1+3, y2+3, width-6, width-6);
#endif
            DrawInSet(gc, x1, y2, width, width);
        }
        else
            DrawOutSet(gc, x1, y2, width, width);

        XSetForeground(display, gc, textColor);
    }
    else if (field->type == RADIOBUTTON)
    {
        x1 = field->x - PixelIndent;
        y2 = baseline-ASCENT(font)+2;
        XSetForeground(display, gc, (active ? statusColor : windowColor));
        XFillArc(display, win, gc, x1, y2, width, width, 0, 360<<6);

        if (active)
        {
            r = width/4;
            DrawInSetCircle(gc, x1, y2, width, width);
            XSetForeground(display, gc, windowBottomShadow);
            width -= r+r;
            XFillArc(display, win, gc, x1+r, y2+r, width, width, 0, 360<<6);
        }
        else
            DrawOutSetCircle(gc, x1, y2, width, width);

        XSetForeground(display, gc, textColor);
    }
}


/* use this routine to hide/show cursor by
   using color = windowShadow/textColor respectively */

void PaintFieldCursor(GC gc, unsigned long color)
{
    int font, width, height, x1, y2, r;

    font = focus->flags & 0x0F;
    width = focus->width;
    height = focus->height;
    x1 = focus->x - PixelIndent - focus->x_indent;
    y2 = focus->baseline - ASCENT(font) - 2;

    if (focus->buflen > 0)
        r = WIDTH(font, focus->value, cursorpos);
    else
        r = 0;

    XSetForeground(display, gc, color);
    XFillRectangle(display, win, gc, x1 + 3 + r, y2+2, 1, height-4);
}

/*
   Repair a given area of a field - called by ScrollField()
   also useful when typing a char into field
*/
void RepairField(GC gc, int start, int width)
{
    int font, height, x1, y1, y2, r;
    XRectangle rect;

    font = focus->flags & 0x0F;
    width = focus->width;
    height = focus->height;
    x1 = focus->x - PixelIndent - focus->x_indent;

    rect.x = start;
    rect.y = focus->baseline - ASCENT(font);
    rect.width = width;
    rect.height = focus->height - 4;
    XSetClipRectangles(display, gc, 0, 0, &rect, 1, Unsorted);

    y2 = focus->baseline - ASCENT(font) - 2;
    XSetForeground(display, gc, statusColor);
    XFillRectangle(display, win, gc, x1, y2, width, height);

    if (focus->buflen > 0)
    {
        y1 = y2 + ASCENT(font) + 2;
        SetFont(gc, font);
        XSetForeground(display, gc, textColor);
        XDrawString(display, win, gc, x1+4, y1, focus->value, focus->buflen);
    }

    if (focus->buflen > 0)
        r = WIDTH(font, focus->value, cursorpos);

    XSetForeground(display, gc, strikeColor);
    XFillRectangle(display, win, gc, x1 + 3 + r, y2+2, 1, height-4);

    rect.x = focus->x + 2;
    rect.y = focus->baseline - ASCENT(font);
    rect.width = focus->width - 4;
    rect.height = focus->height - 4;
    XSetClipRectangles(display, gc, 0, 0, &rect, 1, Unsorted);
}

/*
   Text field lies in rectangle box, scroll it horizontally
   by delta pixels and repair exposed area. When scrolling to left
   restrict scroll area to right of "start"

   delta is greater than zero for right scrolls
   and less than zero for left scrolls

   Useful for backspace, del char, and cursor motion
   at extreme limits of field
*/

void ScrollField(GC gc, int toleft, int start, int delta)
{
    XRectangle rect;
    int width;

    rect.x  = focus->x + 2;
    rect.y = focus->baseline - ASCENT(font);
    rect.width = focus->width - 4;
    rect.height = focus->height - 4;
    XSetClipRectangles(display, gc, 0, 0, &rect, 1, Unsorted);

    if (delta < 0)  /* scroll left: delta < 0 */
    {
        if (start <= rect.x)
        {
            XCopyArea(display, win, win, gc,
                    rect.x - delta, rect.y,
                    rect.width + delta, rect.y,
                    rect.x, rect.y);

            RepairField(gc, rect.x + rect.width + delta, -delta);
        }
        else if (start < rect.x + width)
        {
            width = rect.width + rect.x - start + delta;

            XCopyArea(display, win, win, gc,
                    start - delta, rect.y,
                    width, rect.y,
                    rect.x, rect.y);

            RepairField(gc, start + width, -delta);
        }
    }
    else  /* scroll right: delta is > 0 */
    {
        if (delta < rect.width)
        {
            XCopyArea(display, win, win, gc,
                    rect.x, rect.y,
                    rect.width - delta, rect.y,
                    rect.x + delta, rect.y);

            RepairField(gc, rect.x, delta);
        }
    }
}

