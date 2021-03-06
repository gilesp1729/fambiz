#include "stdafx.h"
#include "jpeglib.h"
#include "Fambiz.h"
#include <CommCtrl.h>
#include <CommDlg.h>
#include <windowsx.h>
#include <stdio.h>

//#define DEBUG_JULIAN

// Graphics stuff for charts.

#define BOX_WIDTH       90
#define BOX_HEIGHT      55
#define MIN_SPACING     25
#define ROUND_DIAM      10
#define CHAR_HEIGHT     8
#define ZOOM_MULTIPLE   1.2
#define L_MARGIN        30
#define T_MARGIN        20

// Offset array based at the centre - descendants are positive, ancestors negative.
int offsets[MAX_GEN];
int *gen_offset = &offsets[CENTRE_GEN];
int desc_generations = 0;
int anc_generations = 0;

int max_offset = 0;
int h_scrollpos, v_scrollpos;
int h_scrollwidth, v_scrollheight;
Person *highlight_person = NULL;
Family *highlight_family = NULL;

// Zooming and fractions of the fixed widths/heights
int box_width;
int box_height;
int l_margin;
int t_margin;
int min_spacing;
int round_diam;
int small_space;
int char_height;
int zooms[20];

// Modified flag
BOOL modified = FALSE;

ViewPrefs default_prefs =
{
    "\0",       // Title of view
    NULL,       // Root person of view
    0,          // Root ID
    TRUE,       // TRUE to view descendants
    FALSE,      // TRUE to view ancestors
    0,          // How many descendant generations to view (0 = no limit)
    0,          // How many ancestor gens similary
    200,        // The zoom percentage of the view
    "\0",       // Printer name
    "A4",       // Printer form name (A4, A0, etc)
    DMORIENT_PORTRAIT, // Paper orientation (DMORIENT_PORTRAIT/LANDSCAPE)
    TRUE,       // TRUE if stripping onto the page (and the sizes allow it)
    297         // Strip height in mm
};

ViewPrefs view_prefs[MAX_PREFS];
ViewPrefs *prefs = &view_prefs[0];
int n_views = 0;

// Find a spouse for p in family f.
Person *find_spouse(Person *p, Family *f)
{
    if (p == f->husband)
        return f->wife;
    else if (p == f->wife)
        return f->husband;

    return NULL;        // should never happen, families must have two parents
}

// Determine the widths for children and all descendents of the selected person.
void
determine_desc_widths(Person *p, int gen)
{
    Person *c, *s;
    PersonList *cl;
    Family *f;
    FamilyList *fl;
    int child_width;

    if (prefs->desc_limit != 0 && gen > prefs->desc_limit)
        return;

    p->generation = gen;

    // Get widths of p and spouse(s).
    p->accum_width = 1;
    for (fl = p->spouses; fl != NULL; fl = fl->next)
    {
        s = find_spouse(p, fl->f);
        s->generation = gen;
        p->accum_width++;
    }

    // Accumulate widths of children and their descendants depth-first.
    child_width = 0;
    if (!p->hidden)
    {
        for (fl = p->spouses; fl != NULL; fl = fl->next)
        {
            f = fl->f;
            s = find_spouse(p, f);
            if (!s->hidden)
            {
                for (cl = f->children; cl != NULL; cl = cl->next)
                {
                    c = cl->p;
                    determine_desc_widths(c, gen + 1);
                    child_width += c->accum_width;
                }
            }
        }
    }
    if (child_width > p->accum_width)
        p->accum_width = child_width;

    // Set widths of spouse(s)
    for (fl = p->spouses; fl != NULL; fl = fl->next)
    {
        s = find_spouse(p, fl->f);
        s->accum_width = p->accum_width;
    }
}

// Determine the offsets for children and all descendents of the selected person.
void
determine_desc_offsets(Person *p, int gen)
{
    Person *c, *s;
    PersonList *cl;
    Family *f;
    FamilyList *fl;
    int last_offset;
    int prev_max_offset = max_offset;

    if (prefs->desc_limit != 0 && gen > prefs->desc_limit)
        return;

    // Accumulate offsets of children and their descendants depth-first.
    for (fl = p->spouses; fl != NULL; fl = fl->next)
    {
        fl->family_max_offset = max_offset - 1;
        if (!p->hidden)
        {
            f = fl->f;
            s = find_spouse(p, f);
            if (!s->hidden)
            {
                for (cl = f->children; cl != NULL; cl = cl->next)
                {
                    c = cl->p;
                    determine_desc_offsets(c, gen + 1);
                }
            }
        }
    }

    // Get offset of p based on stuff below.
    if (gen == 0)
        last_offset = gen_offset[gen];
    else
        last_offset = MAX(gen_offset[gen], gen_offset[gen - 1] - p->accum_width / 2);
    last_offset = MAX(last_offset, prev_max_offset);
    last_offset += MAX(0, p->accum_width / 2 - 1);
    p->offset = last_offset;

    // Set offsets of spouse(s). Take account of previous family's max offset
    for (fl = p->spouses; fl != NULL; fl = fl->next, last_offset++)
    {
        s = find_spouse(p, fl->f);
        s->accum_width = p->accum_width;
        if (fl->family_max_offset > last_offset)
            last_offset = fl->family_max_offset;
        s->offset = last_offset + 1;
    }

    // Accumulate offsets per gen row.
    last_offset++;
    if (last_offset > gen_offset[gen])
        gen_offset[gen] = last_offset;
    if (last_offset > max_offset)
        max_offset = last_offset;
    if (gen > desc_generations)
        desc_generations = gen;
}

// Determine widths of ancestors of person p.
void
determine_anc_widths(Person *p, int gen)
{
    int parent_width = 0;
    Family *f = p->family;

    if (prefs->anc_limit != 0 && -gen > prefs->anc_limit)
        return;

    p->generation = gen;
    p->accum_width = 1;

    if (gen < anc_generations)
        anc_generations = gen;
    if (f == NULL)
        return;

    if (!p->hidden)
    {
        if (f->husband != NULL)
        {
            determine_anc_widths(f->husband, gen - 1);
            parent_width += f->husband->accum_width;
        }
        if (f->wife != NULL)
        {
            determine_anc_widths(f->wife, gen - 1);
            parent_width += f->wife->accum_width;
        }
    }

    if (parent_width > p->accum_width)
        p->accum_width = parent_width;
}

// Determine offsets of ancestors of person p.
void
determine_anc_offsets(Person *p, int gen)
{
    Family *f = p->family;
    int last_offset;

    if (prefs->anc_limit != 0 && -gen > prefs->anc_limit)
        return;

    if (f != NULL && !p->hidden)
    {
        if (f->husband != NULL)
            determine_anc_offsets(f->husband, gen - 1);
        if (f->wife != NULL)
            determine_anc_offsets(f->wife, gen - 1);
    }

    // Get offset of p based on stuff above.
    if (gen == 0 && prefs->view_desc)
        return;   // don't change an existing box for the root person
    if (gen > -2)
        last_offset = gen_offset[gen];
    else
        last_offset = MAX(gen_offset[gen], gen_offset[gen + 1] - p->accum_width / 2);
    last_offset += p->accum_width / 2;
    p->offset = last_offset;

    // Accumulate offsets per gen row.
    last_offset++;
    if (last_offset > gen_offset[gen])
        gen_offset[gen] = last_offset;
    if (last_offset > max_offset)
        max_offset = last_offset;
}


// Wrap text in a box.
void
wrap_text_out(HDC hdc, int x_text, int *y_text, char *name, int namelen)
{
    SIZE sz;

    GetTextExtentPoint(hdc, name, namelen, &sz);
    if (sz.cx < box_width - 2 * small_space)
    {
        TextOut(hdc, x_text, *y_text, name, namelen);
    }
    else
    {
        char *pspace = strrchr(name, ' ');

        if (pspace == NULL)
        {
            // No space - press on regardless
            TextOut(hdc, x_text, *y_text, name, namelen);
        }
        else
        {
            char *ptry = pspace;

            // Go back by spaces until it fits. Only break once.
            while (1)
            {
                GetTextExtentPoint(hdc, name, ptry - name, &sz);
                if (sz.cx < box_width - 2 * small_space)
                {
                    pspace = ptry;
                    break;
                }

                while (*ptry == ' ' && ptry > name)     // don't go back past the start
                    ptry--;
                while (*ptry != ' ' && ptry > name)
                    ptry--;
                if (ptry == name)
                    break;          // give up and use the last break point
            }

            TextOut(hdc, x_text, *y_text, name, pspace - name);
            *y_text += char_height;
            while (*pspace == ' ')
                pspace++;  // skip space(s)
            TextOut(hdc, x_text, *y_text, pspace, strlen(pspace));
        }
    }
    *y_text += char_height;
}

// Draw a box for person p.
void
draw_box(HDC hdc, Person *p)
{
    int x_box = l_margin + p->offset * (box_width + min_spacing) + (min_spacing / 2) - h_scrollpos;
    int y_box = t_margin + (p->generation - anc_generations) * (box_height + min_spacing) + (min_spacing / 2) - v_scrollpos;
    int x_text, y_text;
    Event *ev;
    HPEN hPenOld = NULL;
    char buf[MAXSTR];

    if (p == highlight_person)
        SelectObject(hdc, GetStockObject(LTGRAY_BRUSH));
    else
        SelectObject(hdc, GetStockObject(WHITE_BRUSH));

    if (p->show_link)
    {
        SetDCPenColor(hdc, RGB(0, 0, 0xEE));
        hPenOld = SelectObject(hdc, GetStockObject(DC_PEN));
    }

    y_text = y_box + small_space;
    if (p->sex[0] == 'M')
    {
        if (p->hidden)
        {
            int d = 2 * small_space;

            Rectangle(hdc, x_box + d, y_box + d, x_box + box_width + d, y_box + box_height + d);
            d = small_space;
            Rectangle(hdc, x_box + d, y_box + d, x_box + box_width + d, y_box + box_height + d);
        }
        Rectangle(hdc, x_box, y_box, x_box + box_width, y_box + box_height);
        x_text = x_box + small_space;
        if (p == prefs->root_person || p->show_link)
        {
            Rectangle(hdc, x_box + 2, y_box + 2, x_box + box_width - 2, y_box + box_height - 2);
            x_text += 2;
        }
    }
    else
    {
        if (p->hidden)
        {
            int d = 2 * small_space;

            RoundRect(hdc, x_box + d, y_box + d, x_box + box_width + d, y_box + box_height + d, round_diam, round_diam);
            d = small_space;
            RoundRect(hdc, x_box + d, y_box + d, x_box + box_width + d, y_box + box_height + d, round_diam, round_diam);
        }
        RoundRect(hdc, x_box, y_box, x_box + box_width, y_box + box_height, round_diam, round_diam);
        x_text = x_box + 2 * small_space;
        if (p == prefs->root_person || p->show_link)
        {
            RoundRect(hdc, x_box + 2, y_box + 2, x_box + box_width - 2, y_box + box_height - 2, round_diam, round_diam);
            x_text += 2;
        }
    }

    if (p->show_link)
        SelectObject(hdc, hPenOld);

    // Wrap text at the surname if it won't fit.
    sprintf_s(buf, MAXSTR, "%s %s", p->given, p->surname);
    wrap_text_out(hdc, x_text, &y_text, buf, strlen(buf));
    for (ev = p->event; ev != NULL; ev = ev->next)
    {
        // special case for Born - don't say just Born if we don't have a date
        if (ev->type == EV_BIRTH && ev->date[0] == '\0')
            continue;

        sprintf_s(buf, MAXSTR, "%s %s", codes[ev->type].display, ev->date);
        wrap_text_out(hdc, x_text, &y_text, buf, strlen(buf));
        if (ev->place != NULL && ev->place[0] != '\0')
            wrap_text_out(hdc, x_text, &y_text, ev->place, strlen(ev->place));
    }
#ifdef DEBUG_JULIAN
    sprintf_s(buf, MAXSTR, "%d", p->lildate);
    wrap_text_out(hdc, x_text, &y_text, buf, strlen(buf));
#endif

    // Put little icons at bottom right to indicate that there are notes or attachments.
    if (p->notes != NULL)
    {
        int icon_width = char_height;
        int icon_height = char_height * 1.2;
        HFONT hFont = CreateFont(icon_height, 0, 0, 0, 0, 0, 0, 0, SYMBOL_CHARSET, 0, 0, 0, 0, "Webdings");
        HFONT font_old = SelectObject(hdc, hFont);
        char notepad[2] = { 0xA5, 0 };   // notepad webding

        TextOut(hdc, x_box + box_width - icon_width, y_box + box_height - icon_height - small_space, notepad, 1);
        SelectObject(hdc, font_old);
    }

    if (p->attach != NULL)
    {
        Attachment *a;
        BOOL has_doc = FALSE;
        BOOL has_pic = FALSE;
        int icon_width = char_height;
        int icon_height = char_height * 1.3;  // bigger, to match the notepad
        HFONT hFont = CreateFont(icon_height, 0, 0, 0, 0, 0, 0, 0, SYMBOL_CHARSET, 0, 0, 0, 0, "Webdings");
        HFONT font_old = SelectObject(hdc, hFont);
        char document[2] = { 0x9E, 0 };  // mixed document (e.g. PDF)
        char picture[2] = { 0x9F, 0 };   // picture webding

        for (a = p->attach; a != NULL; a = a->next)
        {
            if (a->is_image)
                has_pic = TRUE;
            else
                has_doc = TRUE;
        }
        if (has_pic)
            TextOut(hdc, x_box + box_width - 2 * icon_width, y_box + box_height - icon_height - small_space, picture, 1);
        if (has_doc)
            TextOut(hdc, x_box + box_width - 3 * icon_width, y_box + box_height - icon_height - small_space, document, 1);
        SelectObject(hdc, font_old);
    }

#ifdef DEBUG_CHART
    sprintf_s(buf, MAXSTR, "%d: Off %d Wid %d", p->id, p->offset, p->accum_width);
    wrap_text_out(hdc, x_text, &y_text, buf, strlen(buf));
#endif
    p->xbox = x_box;
    p->ybox = y_box;
}

// Draw boxes and connecting lines for spouses and descendants of person p.
void
draw_desc_boxes(HDC hdc, Person *p)
{
    Person *c, *s, *prev;
    PersonList *cl;
    Family *f;
    FamilyList *fl;
    Event *ev;
    int x_line, y_line, y_event;

    draw_box(hdc, p);
    prev = p;
    for (fl = p->spouses; fl != NULL; fl = fl->next)
    {
        f = fl->f;
        s = find_spouse(p, f);
        draw_box(hdc, s);

        // highlight the family if required
        if (f == highlight_family)
        {
            HPEN hPenOld = SelectObject(hdc, GetStockObject(NULL_PEN));

            SelectObject(hdc, GetStockObject(LTGRAY_BRUSH));
            RoundRect
            (
                hdc, 
                prev->xbox + box_width + small_space, 
                s->ybox + small_space, 
                s->xbox - small_space, 
                s->ybox + box_height - small_space, 
                round_diam, 
                round_diam
            );
            SelectObject(hdc, hPenOld);
        }

        // marriage double lines assume spouses are to the right.
        y_line = s->ybox + box_height / 2;
        MoveToEx(hdc, s->xbox, y_line, NULL);
        LineTo(hdc, prev->xbox + box_width - 1, y_line);
        y_line += small_space;
        MoveToEx(hdc, s->xbox, y_line, NULL);
        LineTo(hdc, prev->xbox + box_width - 1, y_line);

        // draw icon for any notes present on family

        // draw any marriage/divorce dates. If marriage has a type (e.g. partner) use that.
        y_event = s->ybox + box_height;
        for (ev = f->event; ev != NULL; ev = ev->next)
        {
            char buf[64];

            if (ev->mtype[0] != '\0')
                sprintf_s(buf, 64, "%s %s %s", ev->mtype, ev->date, ev->place);
            else
                sprintf_s(buf, 64, "%s %s %s", codes[ev->type].display, ev->date, ev->place);
            TextOut(hdc, s->xbox + small_space, y_event, buf, strlen(buf));
            y_event += char_height;
        }
#ifdef DEBUG_JULIAN
        {
            char buf[MAXSTR];
            sprintf_s(buf, MAXSTR, "%d", f->lildate);
            TextOut(hdc, s->xbox + small_space, y_event, buf, strlen(buf));
        }
#endif

        if ((prefs->desc_limit == 0 || p->generation < prefs->desc_limit) && !p->hidden && !s->hidden)
        {
            // connect marriage double lines to children.
            if (f->children != NULL)
            {
                //x_line = (s->xbox + prev->xbox + box_width) / 2;
                x_line = s->xbox - min_spacing / 2;                     // TODO this could go the other way for long marriage lines, also do in ancestors
                MoveToEx(hdc, x_line, y_line, NULL);
                y_line = p->ybox + box_height + min_spacing / 2;
                LineTo(hdc, x_line, y_line);
            }

            // draw children of the family
            for (cl = f->children; cl != NULL; cl = cl->next)
            {
                c = cl->p;
                draw_desc_boxes(hdc, c);
                MoveToEx(hdc, x_line, y_line, NULL);
                LineTo(hdc, c->xbox + box_width / 2, y_line);
                LineTo(hdc, c->xbox + box_width / 2, c->ybox);
            }
        }
        prev = s;
    }
}

// Draw boxes and connecting lines for ancestors of person p.
void
draw_anc_boxes(HDC hdc, Person *p)
{
    Family *f = p->family;
    int x_line, y_line, y_event;
    Person *h, *w;
    Event *ev;

    draw_box(hdc, p);
    if (f == NULL)
        return;
    if (prefs->anc_limit != 0 && -p->generation >= prefs->anc_limit)
        return;
    if (p->hidden)
        return;

    h = f->husband;
    w = f->wife;
    if (h != NULL)
        draw_anc_boxes(hdc, h);
    if (w != NULL)
        draw_anc_boxes(hdc, w);
    if (h != NULL && w != NULL)
    {
        // highlight the family if required
        if (f == highlight_family)
        {
            HPEN hPenOld = SelectObject(hdc, GetStockObject(NULL_PEN));

            SelectObject(hdc, GetStockObject(LTGRAY_BRUSH));
            RoundRect
            (
                hdc,
                h->xbox + box_width + small_space,
                w->ybox + small_space,
                w->xbox - small_space,
                w->ybox + box_height - small_space,
                round_diam,
                round_diam
            );
            SelectObject(hdc, hPenOld);
        }

        // Marriage double lines
        y_line = h->ybox + box_height / 2;
        MoveToEx(hdc, w->xbox, y_line, NULL);
        LineTo(hdc, h->xbox + box_width - 1, y_line);
        y_line += small_space;
        MoveToEx(hdc, w->xbox, y_line, NULL);
        LineTo(hdc, h->xbox + box_width - 1, y_line);

        // draw icon for any notes present on family

        // connect to parents
        x_line = (h->xbox + box_width + w->xbox) / 2;
        MoveToEx(hdc, x_line, y_line, NULL);
        y_line = p->ybox - min_spacing / 2;
        LineTo(hdc, x_line, y_line);
        LineTo(hdc, p->xbox + box_width / 2, y_line);
        LineTo(hdc, p->xbox + box_width / 2, p->ybox);

        // print marriage etc. events
        y_event = w->ybox + box_height;
        for (ev = f->event; ev != NULL; ev = ev->next)
        {
            char buf[64];

            sprintf_s(buf, 64, "%s %s %s", codes[ev->type].display, ev->date, ev->place);
            TextOut(hdc, w->xbox + small_space, y_event, buf, strlen(buf));
            y_event += char_height;
        }
    }
}

void
update_scrollbars(HWND hWnd)
{
    RECT rc;
    SCROLLINFO hscrollinfo, vscrollinfo;

    GetClientRect(hWnd, &rc);
    h_scrollwidth = (max_offset + 1) * ((BOX_WIDTH + MIN_SPACING) * prefs->zoom_percent) / 100;
    v_scrollheight = (desc_generations - anc_generations + 1) * ((BOX_HEIGHT + MIN_SPACING) * prefs->zoom_percent) / 100;

    if (h_scrollpos < 0 || h_scrollwidth - (rc.right - rc.left - 1) < 0)
        h_scrollpos = 0;
    else if (h_scrollpos > h_scrollwidth - (rc.right - rc.left - 1) < 0)
        h_scrollpos = h_scrollwidth - (rc.right - rc.left - 1);

    if (v_scrollpos < 0 || v_scrollheight - (rc.bottom - rc.top - 1) < 0)
        v_scrollpos = 0;
    else if (v_scrollpos > v_scrollheight - (rc.bottom - rc.top - 1) < 0)
        v_scrollpos = v_scrollheight - (rc.bottom - rc.top - 1);

    hscrollinfo.cbSize = sizeof(SCROLLINFO);
    hscrollinfo.fMask = SIF_ALL | SIF_DISABLENOSCROLL;
    hscrollinfo.nMin = 0;
    hscrollinfo.nMax = h_scrollwidth;
    hscrollinfo.nPage = rc.right - rc.left;
    hscrollinfo.nPos = h_scrollpos;
    SetScrollInfo(hWnd, SB_HORZ, &hscrollinfo, TRUE);

    vscrollinfo.cbSize = sizeof(SCROLLINFO);
    vscrollinfo.fMask = SIF_ALL | SIF_DISABLENOSCROLL;
    vscrollinfo.nMin = 0;
    vscrollinfo.nMax = v_scrollheight;
    vscrollinfo.nPage = rc.bottom - rc.top;
    vscrollinfo.nPos = v_scrollpos;
    SetScrollInfo(hWnd, SB_VERT, &vscrollinfo, TRUE);

    // Repaint the window
    InvalidateRect(hWnd, NULL, TRUE);
}

// Move person (and spouse(s) and descendants) by delta X in its offset.
void
move_person_by_deltax(Person *p, int dx)
{
    FamilyList *fl;
    PersonList *cl;
    Person *s;
    Family *f;

    p->offset += dx;
    for (fl = p->spouses; fl != NULL; fl = fl->next)
    {
        f = fl->f;
        if (f == NULL)
            continue;
        s = find_spouse(p, f);
        if (s == NULL)
            continue;
        s->offset += dx;
        for (cl = f->children; cl != NULL; cl = cl->next)
            move_person_by_deltax(cl->p, dx);
    }
}

// Check if modified, and if necessary save the file.
void check_before_closing(HWND hWnd)
{
    OPENFILENAME ofn;
    if (!modified)
        return;

    modified = FALSE;
    if (MessageBox(hWnd, "File modified. Save changes?", curr_filename, MB_YESNO | MB_ICONWARNING) == IDYES)
    {
        if (curr_filename[0] == '\0')
        {
            memset(&ofn, 0, sizeof(OPENFILENAME));
            ofn.lStructSize = sizeof(OPENFILENAME);
            ofn.hwndOwner = hWnd;
            ofn.lpstrFilter = "GEDCOM Files (*.GED)\0*.GED\0All Files\0*.*\0\0";
            ofn.lpstrTitle = "Save a GEDCOM File";
            ofn.lpstrFile = curr_filename;
            ofn.nMaxFile = MAXSTR;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
            ofn.lpstrDefExt = "ged";
            if (!GetSaveFileName(&ofn))
                return;  // tell user somehow - TODO
        }
        write_ged(curr_filename);
    }
}

// Clear all data.
void clear_all(void)
{
    n_person = 0;
    n_family = 0;
    n_views = 0;
    memset(lookup_person, 0, MAX_PERSON * sizeof(Person *));
    memset(lookup_family, 0, MAX_FAMILY * sizeof(Family *));
    prefs->root_person = NULL;
    curr_filename[0] = '\0';
    attach_dir[0] = '\0';
    attach_default = FALSE;
    view_prefs[0] = default_prefs;
    prefs = &view_prefs[0];
}

// Remove spaces from a filename (leave any prepended directory alone, though).
// Replace them wth underscores.
void clean_blanks(char *string)
{
    char *slosh = strrchr(string, '\\');
    char *p;

    if (slosh == NULL)
        slosh = string;
    for (p = slosh; *p != '\0'; p++)
    {
        if (*p == ' ')
            *p = '_';
    }
}

// Return a string with all quotes escaped for HTML. CRLF's are replaced by <br>.
// Returns its buffer so it can be used in printf. The result may be longer than the input. (not checked! TODO)
char *escq(char *buf, char *string)
{
    char *p, *q;

    for (p = string, q = buf; *p != '\0'; p++)
    {
        if (*p == '\"')
        {
            *q++ = '&';
            *q++ = 'q';
            *q++ = 'u';
            *q++ = 'o';
            *q++ = 't';
            *q++ = ';';
        }
        else if (*p == '\'')
        {
            *q++ = '&';
            *q++ = 'a';
            *q++ = 'p';
            *q++ = 'o';
            *q++ = 's';
            *q++ = ';';
        }
        else if (*p == '\r' && *(p + 1) == '\n')
        {
            p++;
            *q++ = '<';
            *q++ = 'b';
            *q++ = 'r';
            *q++ = '>';
        }
        else
            *q++ = *p;
    }
    *q = '\0';
    return buf;
}

// Generate a chart based on prefs.
void generate_chart(ViewPrefs *prefs)
{
    int i;
    int desc_accum_width, anc_accum_width;

    // Clear offset arrays.
    for (i = 0; i < MAX_GEN; i++)
        offsets[i] = 0;
    desc_generations = 0;
    anc_generations = 0;
    max_offset = 0;

    // Put all boxes off the screen so old ones don't accidentally get selected.
    for (i = 0; i <= n_person; i++)
    {
        Person *p = lookup_person[i];

        if (p != NULL)
        {
            p->generation = 0;
            p->accum_width = 0;
            p->offset = 0;
            p->xbox = -99999;
            p->ybox = 0;
            p->show_link = FALSE;
            p->view_link = 0;
        }
    }

    // Generate widths. Take care of root_person->accum_width being overwritten!
    if (prefs->view_desc)
        determine_desc_widths(prefs->root_person, 0);
    desc_accum_width = prefs->root_person->accum_width;
    if (prefs->view_anc)
        determine_anc_widths(prefs->root_person, 0);
    anc_accum_width = prefs->root_person->accum_width;

    if (prefs->view_desc && prefs->view_anc)
    {
        // Update gen offsets to accommodate ancestors wider than descendants,
        // or the converse.
        if (desc_accum_width > anc_accum_width)
        {
            prefs->root_person->accum_width = desc_accum_width;
            determine_desc_offsets(prefs->root_person, 0);
            for (i = -1; i >= -CENTRE_GEN; i--)
                gen_offset[i] = (desc_accum_width - anc_accum_width) / 2;
            prefs->root_person->accum_width = anc_accum_width;
            determine_anc_offsets(prefs->root_person, 0);
        }
        else if (anc_accum_width > desc_accum_width)
        {
            int saved_max;

            prefs->root_person->accum_width = anc_accum_width;
            determine_anc_offsets(prefs->root_person, 0);
            for (i = 0; i < CENTRE_GEN; i++)    // start at 0 as root offset not stored
                gen_offset[i] = (anc_accum_width - desc_accum_width) / 2;
            saved_max = max_offset;             // Save the max offset so it doesn't push descendants over
            max_offset = (anc_accum_width - desc_accum_width) / 2;
            prefs->root_person->accum_width = desc_accum_width;
            determine_desc_offsets(prefs->root_person, 0);
            max_offset = MAX(max_offset, saved_max);
        }
        else
        {
            // Generate all offsets. Replace overwritten accum_widths.
            prefs->root_person->accum_width = desc_accum_width;
            if (prefs->view_desc)
                determine_desc_offsets(prefs->root_person, 0);
            prefs->root_person->accum_width = anc_accum_width;
            if (prefs->view_anc)
                determine_anc_offsets(prefs->root_person, 0);
        }
    }
    else
    {
        // Generate all offsets. Replace overwritten accum_widths.
        prefs->root_person->accum_width = desc_accum_width;
        if (prefs->view_desc)
            determine_desc_offsets(prefs->root_person, 0);
        prefs->root_person->accum_width = anc_accum_width;
        if (prefs->view_anc)
            determine_anc_offsets(prefs->root_person, 0);
    }

    // Recalculate chart extents
    h_scrollwidth = (max_offset + 1) * ((BOX_WIDTH + MIN_SPACING) * prefs->zoom_percent) / 100;
    v_scrollheight = (desc_generations - anc_generations + 1) * ((BOX_HEIGHT + MIN_SPACING) * prefs->zoom_percent) / 100;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int wmId, wmEvent, i, j, v;
    PAINTSTRUCT ps;
    HDC hdc, hdcMem;
    HBITMAP hbmp, old_bmp;
    BITMAP bmp;
    BITMAPINFOHEADER bi;
    HMENU hMenu;
    HFONT hFont = NULL;
    HFONT hFontLarge = NULL;
    HFONT hFontOld;
    HPEN hPenOld;
    OPENFILENAME ofn;
    static PRINTDLG prd;
    static int printx, printy, printsizex, printsizey;
    static int screenx, screeny, screensizex, screensizey;
    int h_scroll_save, v_scroll_save;
    int pagex, n_pagesx, pagey, n_pagesy, n_page;
    int copy, max_copies, num_copies, dummy;
    int n_strips, strip_height;
    BOOL stripping = TRUE;
    DOCINFO di;
    int printer_percentx, printer_percenty;
    SCROLLINFO scrollinfo;
    RECT rc;
    int cmd;
    int x_move, y_move;
    static int h_dragpos;
    static int x_down, y_down;
    char buf[MAXSTR], buf2[MAXSTR], notebuf[MAX_NOTESIZE];
    DEVMODE *devmode;
    DEVNAMES *devnames;
    Note **note_ptr;
    Person *p;
    int line_size;
    char *lpbitmap;
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    FILE *jpeg, *html;
    char html_basename[MAXSTR], html_filename[MAXSTR], jpeg_filename[MAXSTR], web_dir[MAXSTR];
    char *dot, *slosh;

    switch (message)
    {
    case WM_CREATE:
        // Return print sizes for default printer
        memset(&prd, 0, sizeof(PRINTDLG));
        prd.lStructSize = sizeof(PRINTDLG);
        prd.Flags = PD_RETURNDEFAULT | PD_RETURNDC;
        PrintDlg(&prd);
        printx = GetDeviceCaps(prd.hDC, LOGPIXELSX);
        printy = GetDeviceCaps(prd.hDC, LOGPIXELSY);
        printsizex = GetDeviceCaps(prd.hDC, HORZRES);
        printsizey = GetDeviceCaps(prd.hDC, VERTRES);
        DeleteDC(prd.hDC);

        // Get the screen res and size
        hdc = GetDC(hWnd);
        screenx = GetDeviceCaps(hdc, LOGPIXELSX);
        screeny = GetDeviceCaps(hdc, LOGPIXELSY);
        screensizex = GetDeviceCaps(hdc, HORZRES);
        screensizey = GetDeviceCaps(hdc, VERTRES);

        // Set up the default preferences
        view_prefs[0] = default_prefs;
        prefs = &view_prefs[0];
        prefs->root_person = NULL;
        break;

    case WM_COMMAND:
        wmId = LOWORD(wParam);
        wmEvent = HIWORD(wParam);
        // Parse the menu selections:
        switch (wmId)
        {
        case ID_FILE_OPEN:
            check_before_closing(hWnd);
            clear_all();
            memset(&ofn, 0, sizeof(OPENFILENAME));
            ofn.lStructSize = sizeof(OPENFILENAME);
            ofn.hwndOwner = hWnd;
            ofn.lpstrFilter = "GEDCOM Files (*.GED)\0*.GED\0All Files\0*.*\0\0";
            ofn.nFilterIndex = 1;
            ofn.lpstrDefExt = "ged";
            ofn.lpstrFile = curr_filename;
            ofn.nMaxFile = 256;
            ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST;
            if (!(GetOpenFileName(&ofn) && read_ged(curr_filename)))
                break;
            
            SetWindowText(hWnd, curr_filename);
            if (prefs->root_person == NULL)
                prefs->root_person = lookup_person[1];
            if (n_views == 0)
                n_views = 1;    // default view has already been set up
            if (attach_dir[0] == '\0')
            {
                // If no photos or attachments have been seen, the default dir is the file basename.
                strcpy_s(attach_dir, MAXSTR, curr_filename);
                dot = strrchr(attach_dir, '.');
                *dot = '\0';
                strcat_s(attach_dir, MAXSTR, "\\");
                attach_default = TRUE;
            }

        generate_chart:
            generate_chart(prefs);

            // Set up scroll bars to reflect total width and height and update view.
            // Keep highlighted person (or if there isn't one, the root person) on screen.
            box_width = (BOX_WIDTH * prefs->zoom_percent) / 100;
            box_height = (BOX_HEIGHT * prefs->zoom_percent) / 100;
            l_margin = L_MARGIN;
            t_margin = T_MARGIN;
            min_spacing = (MIN_SPACING * prefs->zoom_percent) / 100;
            p = highlight_person;
            if (p == NULL)
                p = prefs->root_person;
            h_scrollpos = p->offset * (box_width + min_spacing) - (screensizex / 2);
            v_scrollpos = (p->generation - anc_generations) * (box_height + min_spacing) - (screensizey / 2);
            update_scrollbars(hWnd);

            // Update window title to reflect file and chart size
        update_titlebar:
            pagex = (printsizex * screenx) / printx;
            n_pagesx = (h_scrollwidth + pagex - 1) / pagex;
            pagey = (printsizey * screeny) / printy;
            n_pagesy = (v_scrollheight + pagey - 1) / pagey;

            if (prefs->dm_devicename[0] != '\0')
            {
                // Use device and paper names from the preferences for this view
                sprintf_s(buf, MAXSTR, "%s (%d by %d, %d by %d %s pages on %s)",
                          curr_filename,
                          max_offset, desc_generations - anc_generations,
                          n_pagesx, n_pagesy, prefs->dm_formname, prefs->dm_devicename);
            }
            else
            {
                // The view doesn't have a printer device name; use the default from the DEVMODE
                devmode = GlobalLock(prd.hDevMode);
                sprintf_s(buf, MAXSTR, "%s (%d by %d, %d by %d %s pages on %s)",
                          curr_filename,
                          max_offset, desc_generations - anc_generations,
                          n_pagesx, n_pagesy, devmode->dmFormName, devmode->dmDeviceName);
                GlobalUnlock(prd.hDevMode);
            }
            SetWindowText(hWnd, buf);
            break;

        case ID_FILE_SAVEAS:
        save_as:
            memset(&ofn, 0, sizeof(OPENFILENAME));
            ofn.lStructSize = sizeof(OPENFILENAME);
            ofn.hwndOwner = hWnd;
            ofn.lpstrFilter = "GEDCOM Files (*.GED)\0*.GED\0All Files\0*.*\0\0";
            ofn.lpstrTitle = "Save a GEDCOM File";
            ofn.lpstrFile = curr_filename;
            ofn.nMaxFile = MAXSTR;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
            ofn.lpstrDefExt = "ged";
            if (!GetSaveFileName(&ofn))
                break;
            // fall through
        case ID_FILE_SAVE:
            if (curr_filename[0] == '\0')
                goto save_as;
            write_ged(curr_filename);
            modified = FALSE;
            break;

        case ID_FILE_NEW:
            check_before_closing(hWnd);
            clear_all();
            prefs->root_person = highlight_person = new_person_by_id(1);
            curr_filename[0] = '\0';
            SetWindowText(hWnd, "Family Business");
            cmd = DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_PERSON), hWnd, person_dialog, (LPARAM)prefs->root_person);
            switch (cmd)
            {
            case IDCANCEL:
                break;
            case IDOK:
                InvalidateRect(hWnd, NULL, TRUE);
                break;
            case ID_PERSON_ADDSPOUSE:
                goto add_spouse;
            case ID_PERSON_ADDPARENT:
                goto add_parent;
            case ID_PERSON_ADDSIBLING:
                goto add_sibling;
            }
            break;

        case ID_FILE_CLOSE:
            check_before_closing(hWnd);
            clear_all();
            InvalidateRect(hWnd, NULL, TRUE);
            SetWindowText(hWnd, "Family Business");
            break;

        case ID_FILE_EXPORT:
            // Build HTML and JPEG image for each view.
            // Strip extension from GED filename, and allow user to change
            strcpy_s(html_basename, MAXSTR, curr_filename);
            dot = strrchr(html_basename, '.');
            *dot = '\0';

            memset(&ofn, 0, sizeof(OPENFILENAME));
            ofn.lStructSize = sizeof(OPENFILENAME);
            ofn.hwndOwner = hWnd;
            ofn.lpstrTitle = "Enter Folder/Basename for Website Content";
            ofn.lpstrFile = html_basename;
            ofn.nMaxFile = MAXSTR;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
            if (!GetSaveFileName(&ofn))
                break;

            // Obtain folder name to allow copying of attachments
            strcpy_s(web_dir, MAXSTR, html_basename);
            slosh = strrchr(web_dir, '\\');
            *(slosh + 1) = '\0';

            SetCursor(LoadCursor(NULL, IDC_WAIT));
            for (v = 0; v < n_views; v++)
            {
                ViewPrefs *vp = &view_prefs[v];

                // Generate chart according to prefs of view
                generate_chart(vp);

                // Enable link drawing for roots of other views
                for (i = 0; i < n_views; i++)
                {
                    Person *root = view_prefs[i].root_person;

                    if (i == v)
                        continue;  // don't link to self
                    root->show_link = TRUE;
                    root->view_link = i;
                }

                // Preload filename for HTML from basename and view title (name will also be used for JPEG image)
                strcpy_s(html_filename, MAXSTR, html_basename);
                strcat_s(html_filename, MAXSTR, "_");
                strcat_s(html_filename, MAXSTR, vp->title);

                // Get rid of spaces in the filename part
                clean_blanks(html_filename);
                strcpy_s(jpeg_filename, MAXSTR, html_filename);
                strcat_s(html_filename, MAXSTR, ".html");
                strcat_s(jpeg_filename, MAXSTR, ".jpg");

                // Paint content into an image at screen resolution
                hdc = GetDC(hWnd);
                hdcMem = CreateCompatibleDC(hdc);
                hbmp = CreateCompatibleBitmap(hdc, h_scrollwidth, v_scrollheight);
                old_bmp = SelectObject(hdcMem, hbmp);

                h_scroll_save = h_scrollpos;
                v_scroll_save = v_scrollpos;
                h_scrollpos = 0;
                v_scrollpos = 0;

                box_width = (BOX_WIDTH * vp->zoom_percent) / 100;
                box_height = (BOX_HEIGHT * vp->zoom_percent) / 100;
                l_margin = L_MARGIN;
                t_margin = T_MARGIN;
                min_spacing = (MIN_SPACING * vp->zoom_percent) / 100;
                round_diam = (ROUND_DIAM * vp->zoom_percent) / 100;
                small_space = round_diam / 4;
                char_height = (CHAR_HEIGHT  * vp->zoom_percent) / 100;

                // Fill with white (otherwise background comes out black)
                rc.left = 0;
                rc.right = h_scrollwidth;
                rc.top = 0;
                rc.bottom = v_scrollheight;
                FillRect(hdcMem, &rc, GetStockObject(WHITE_BRUSH));
                hFont = CreateFont(char_height, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "Arial");
                hFontOld = SelectObject(hdcMem, hFont);

                if (vp->view_desc)
                    draw_desc_boxes(hdcMem, vp->root_person);
                if (vp->view_anc)
                    draw_anc_boxes(hdcMem, vp->root_person);

                h_scrollpos = h_scroll_save;
                v_scrollpos = v_scroll_save;

                SelectObject(hdcMem, hFontOld);
                SelectObject(hdcMem, old_bmp);
                GetObject(hbmp, sizeof(BITMAP), &bmp);

                bi.biSize = sizeof(BITMAPINFOHEADER);
                bi.biWidth = bmp.bmWidth;
                bi.biHeight = bmp.bmHeight;
                bi.biPlanes = 1;
                bi.biBitCount = 24;
                bi.biCompression = BI_RGB;
                bi.biSizeImage = 0;
                bi.biXPelsPerMeter = 0;
                bi.biYPelsPerMeter = 0;
                bi.biClrUsed = 0;
                bi.biClrImportant = 0;

                // One scanline of bitmap
                line_size = ((bmp.bmWidth * bi.biBitCount + 31) / 32) * 4;
                lpbitmap = malloc(line_size);

                // write scanlines to JPEG
                cinfo.err = jpeg_std_error(&jerr);
                jpeg_create_compress(&cinfo);
                fopen_s(&jpeg, jpeg_filename, "wb");
                jpeg_stdio_dest(&cinfo, jpeg);

                cinfo.image_width = bmp.bmWidth;
                cinfo.image_height = bmp.bmHeight;
                cinfo.input_components = 3;
                cinfo.in_color_space = JCS_RGB;
                jpeg_set_defaults(&cinfo);
                jpeg_start_compress(&cinfo, TRUE);

                for (i = bmp.bmHeight; i >= 0; i--)
                {
                    char *r, *b;

                    // Swap BGR to RGB.
                    GetDIBits(hdc, hbmp, i, 1, lpbitmap, (BITMAPINFO *)&bi, DIB_RGB_COLORS);
                    for (j = 0, b = lpbitmap, r = lpbitmap+2; j < bmp.bmWidth; j++, b+=3, r+=3)
                    {
                        char temp = *b;
                        *b = *r;
                        *r = temp;
                    }
                    jpeg_write_scanlines(&cinfo, &lpbitmap, 1);
                }
                jpeg_finish_compress(&cinfo);
                jpeg_destroy_compress(&cinfo);
                free(lpbitmap);
                DeleteObject(hbmp);
                DeleteDC(hdcMem);

                // Write HTML with image map over the image
                fopen_s(&html, html_filename, "wt");
                fprintf_s(html, "<HTML>\n");
                fprintf_s(html, "<head>\n");
                fprintf_s(html, "<title>%s</title>\n", html_filename);
                fprintf_s(html, "<script type=\"text/javascript\">\n");
                fprintf_s(html, "var w\n");
                fprintf_s(html, "function popf(fn)\n");
                fprintf_s(html, "{\n");
                fprintf_s(html, "if (w != undefined && !w.closed)\n");
                fprintf_s(html, "    w.close()\n");
                fprintf_s(html, "  w = window.open('', '', 'width=400, height=400')\n");
                fprintf_s(html, "  fn(w)\n");
                fprintf_s(html, "  w.focus()\n");
                fprintf_s(html, "}\n");

                // A function to fill window contents for each person mentioned in the chart
                for (i = 0; i < n_person; i++)
                {
                    Event *ev;
                    Note *n;
                    Attachment *a;
                    char *slosh;

                    p = lookup_person[i];
                    if (p == NULL || p->xbox < 0)
                        continue;
                    fprintf_s(html, "function person%04d(w) { ", p->id);
                    fprintf_s(html, "w.document.write(\'%d: %s %s<br>",
                              p->id, escq(buf, p->given), escq(buf2, p->surname));
                    for (ev = p->event; ev != NULL; ev = ev->next)
                        fprintf_s(html, "%s %s %s<br>", codes[ev->type].display, escq(buf, ev->date), escq(buf2, ev->place));

                    fprintf_s(html, "<br>");
                    for (n = p->notes; n != NULL; n = n->next)
                        fprintf_s(html, "%s<br><br>", escq(notebuf, n->note));

                    for (a = p->attach; a != NULL; a = a->next)
                    {
                        char dest_file[MAXSTR];

                        fprintf_s(html, "%s<br>", escq(buf, a->title));
                        slosh = strrchr(a->filename, '\\');
                        if (a->is_image)
                            fprintf_s(html, "<img src=\"%s\"/><br>", slosh + 1);
                        else
                            fprintf_s(html, "<a href=\"%s\">%s</a><br>", slosh + 1, slosh + 1);

                        // Copy attachment file to web dir
                        strcpy_s(dest_file, MAXSTR, web_dir);
                        strcat_s(dest_file, MAXSTR, slosh + 1);
                        CopyFile(a->filename, dest_file, FALSE);
                    }

                    fprintf_s(html, "\') }\n");
                }

                fprintf_s(html, "</script>\n");
                fprintf_s(html, "</head>\n");
                fprintf_s(html, "<body>\n");

                // Link texts for views other than this one
                for (i = 0; i < n_views; i++)
                {
                    ViewPrefs *vp = &view_prefs[i];

                    if (i == v)     // This view, just write the title
                    {
                        fprintf_s(html, "%s<br>\n", vp->title);
                    }
                    else            // Another view, put the title as a link
                    {
                        strcpy_s(buf, MAXSTR, html_basename);
                        strcat_s(buf, MAXSTR, "_");
                        strcat_s(buf, MAXSTR, vp->title);
                        clean_blanks(buf);
                        strcat_s(buf, MAXSTR, ".html");
                        slosh = strrchr(buf, '\\');
                        fprintf_s(html, "<a href=\"%s\">%s</a><br>\n", slosh + 1, vp->title);
                    }
                }


                // strip directory string from filename. It will be there (from GetSaveFileName)
                slosh = strrchr(jpeg_filename, '\\');
                fprintf_s(html, "<img src=\"%s\" border=0 usemap=\"#link\">\n", slosh + 1);
                fprintf_s(html, "<map name=\"link\">\n");

                // Image map with an area for each person
                for (i = 0; i < n_person; i++)
                {
                    p = lookup_person[i];
                    if (p == NULL || p->xbox < 0)
                        continue;
                    fprintf_s(html, "<area shape=\"RECT\" coords=\"%d,%d,%d,%d\" ",
                              p->xbox, p->ybox, p->xbox + box_width, p->ybox + box_height);
                    if (p->show_link)
                    {
                        // plant a link to the other view's HTML file
                        strcpy_s(buf, MAXSTR, html_basename);
                        strcat_s(buf, MAXSTR, "_");
                        strcat_s(buf, MAXSTR, view_prefs[p->view_link].title);
                        clean_blanks(buf);
                        strcat_s(buf, MAXSTR, ".html");
                        slosh = strrchr(buf, '\\');
                        fprintf_s(html, "href=\"%s\">\n", slosh + 1);
                    }
                    else   // not a link, just put in the JS to show the popup with info
                    {
                        fprintf_s(html, "href=\"javascript:void()\" onClick=\"popf(person%04d)\">\n", p->id);
                    }
                }

                fprintf_s(html, "</map>\n");
                fprintf_s(html, "</body>\n");
                fprintf_s(html, "</HTML>\n");
                fclose(html);
            }

            // Write index.html with links to all the views, so the web site works
            strcpy_s(buf, MAXSTR, html_basename);
            slosh = strrchr(buf, '\\');
            *(slosh + 1) = '\0';
            strcat_s(buf, MAXSTR - (slosh - buf), "index.html");
            fopen_s(&html, buf, "wt");
            fprintf_s(html, "<HTML>\n");
            fprintf_s(html, "<body>\n");
            for (i = 0; i < n_views; i++)
            {
                ViewPrefs *vp = &view_prefs[i];

                strcpy_s(buf, MAXSTR, html_basename);
                strcat_s(buf, MAXSTR, "_");
                strcat_s(buf, MAXSTR, vp->title);
                clean_blanks(buf);
                strcat_s(buf, MAXSTR, ".html");
                slosh = strrchr(buf, '\\');
                fprintf_s(html, "<a href=\"%s\">%s</a><br>\n", slosh + 1, vp->title);
            }
            fprintf_s(html, "</body>\n");
            fprintf_s(html, "</HTML>\n");
            fclose(html);

            // Restore cursor and chart on screen
            SetCursor(LoadCursor(NULL, IDC_ARROW));
            goto generate_chart;

        case ID_FILE_PAGESETUP:
            if (prefs != NULL)
            {
                // setup prd DEVMODE with current view print settings, if there is one
                devmode = GlobalLock(prd.hDevMode);
                devmode->dmSpecVersion = DM_SPECVERSION;
                devmode->dmSize = sizeof(DEVMODE);
                strcpy_s(devmode->dmFormName, 32, prefs->dm_formname);
                strcpy_s(devmode->dmDeviceName, 32, prefs->dm_devicename);
                devmode->dmOrientation = prefs->dm_orientation;
                devmode->dmFields = DM_FORMNAME | DM_ORIENTATION;
                GlobalUnlock(prd.hDevMode);
                GlobalFree(prd.hDevNames);      // force dmDeviceName to be used for printer
                prd.hDevNames = NULL;
            }
            prd.Flags = PD_PRINTSETUP | PD_RETURNDC;
            if (!PrintDlg(&prd))
                break;

            // store printer paper info in prefs
            devmode = GlobalLock(prd.hDevMode);
            strcpy_s(prefs->dm_formname, 32, devmode->dmFormName);
            strcpy_s(prefs->dm_devicename, 32, devmode->dmDeviceName);
            prefs->dm_orientation = devmode->dmOrientation;
            GlobalUnlock(prd.hDevMode);
            modified = TRUE;

            // Work out how many copies this thing can print
            devnames = GlobalLock(prd.hDevNames);
            max_copies = DeviceCapabilities
                (
                (char *)devnames + devnames->wDeviceOffset, 
                (char *)devnames + devnames->wOutputOffset, 
                DC_COPIES, 
                &dummy, 
                NULL
                );
            GlobalUnlock(prd.hDevNames);

            // recalculate page sizes and redraw page boundaries
            printx = GetDeviceCaps(prd.hDC, LOGPIXELSX);
            printy = GetDeviceCaps(prd.hDC, LOGPIXELSY);
            printsizex = GetDeviceCaps(prd.hDC, HORZRES);
            printsizey = GetDeviceCaps(prd.hDC, VERTRES);
            DeleteDC(prd.hDC);
            if (prefs->root_person != NULL)
                InvalidateRect(hWnd, NULL, TRUE);
            goto update_titlebar;

        case ID_FILE_PRINT:
            if (prefs != NULL)
            {
                // setup prd DEVMODE with current view print settings, if there is one
                devmode = GlobalLock(prd.hDevMode);
                devmode->dmSpecVersion = DM_SPECVERSION;
                devmode->dmSize = sizeof(DEVMODE);
                strcpy_s(devmode->dmFormName, 32, prefs->dm_formname);
                strcpy_s(devmode->dmDeviceName, 32, prefs->dm_devicename);
                devmode->dmOrientation = prefs->dm_orientation;
                devmode->dmFields = DM_FORMNAME | DM_ORIENTATION;
                GlobalFree(prd.hDevNames);      // force dmDeviceName to be used for printer
                prd.hDevNames = NULL;
                GlobalUnlock(prd.hDevMode);
            }
            prd.Flags = PD_RETURNDC;
            prd.nCopies = 1;
            prd.nFromPage = 1;
            prd.nToPage = 0xFFFF;
            prd.nMinPage = 1;
            prd.nMaxPage = 0xFFFF;
            if (!PrintDlg(&prd))
                break;

            // store printer paper info in prefs
            devmode = GlobalLock(prd.hDevMode);
            strcpy_s(prefs->dm_formname, 32, devmode->dmFormName);
            strcpy_s(prefs->dm_devicename, 32, devmode->dmDeviceName);
            prefs->dm_orientation = devmode->dmOrientation;
            GlobalUnlock(prd.hDevMode);
            modified = TRUE;

            // Work out how many copies this thing can print
            devnames = GlobalLock(prd.hDevNames);
            max_copies = DeviceCapabilities
                (
                (char *)devnames + devnames->wDeviceOffset,
                (char *)devnames + devnames->wOutputOffset,
                DC_COPIES,
                &dummy,
                NULL
                );
            GlobalUnlock(prd.hDevNames);

            // Start a doc
            hdc = prd.hDC;
            memset(&di, 0, sizeof(DOCINFO));
            di.cbSize = sizeof(DOCINFO);
            if (prefs != NULL)
                di.lpszDocName = prefs->title;
            else
                di.lpszDocName = "Family Business";
            di.lpszOutput = (LPTSTR)NULL;
            di.lpszDatatype = (LPTSTR)NULL;
            di.fwType = 0;

            // recalculate printer sizes and #pages in case user has changed printer
            printx = GetDeviceCaps(hdc, LOGPIXELSX);
            printy = GetDeviceCaps(hdc, LOGPIXELSY);
            printsizex = GetDeviceCaps(prd.hDC, HORZRES);
            printsizey = GetDeviceCaps(prd.hDC, VERTRES);
            pagex = (printsizex * screenx) / printx;
            n_pagesx = (h_scrollwidth + pagex - 1) / pagex;
            pagey = (printsizey * screeny) / printy;
            n_pagesy = (v_scrollheight + pagey - 1) / pagey;
            if (n_pagesy == 1 && prefs->stripping)
            {
                // To strip pages, n_pagesy must be 1.
                strip_height = (prefs->strip_height * printy) / 25.4f;
                n_strips = printsizey / strip_height;
            }
            else
            {
                strip_height = 0;   // not stripping
                n_strips = 1;
            }

            // prd.nCopies and (prd.Flags & PD_COLLATE) contain user's copies and collate settings
            // (don't look in the DEVMODE for these). If driver does copies, make sure we don't.
            // We don't do collation here, regardless of what the luser says.
            num_copies = prd.nCopies;
            if (max_copies > 1)
                num_copies = 1;

            // Calculate scales for the printer
            printer_percentx = (printx * prefs->zoom_percent) / screenx;
            printer_percenty = (printy * prefs->zoom_percent) / screeny;
            box_width = (BOX_WIDTH * printer_percentx) / 100;
            box_height = (BOX_HEIGHT * printer_percenty) / 100;
            l_margin = (L_MARGIN * printx) / screenx;
            t_margin = (T_MARGIN * printy) / screeny;
            min_spacing = (MIN_SPACING * printer_percentx) / 100;
            round_diam = (ROUND_DIAM * printer_percentx) / 100;
            small_space = round_diam / 4;
            char_height = (CHAR_HEIGHT  * printer_percenty) / 100;

            if (StartDoc(hdc, &di) == SP_ERROR)
                break;

            // Save scroll positions and restore them after printing page(s).
            h_scroll_save = h_scrollpos;
            v_scroll_save = v_scrollpos;
            h_scrollpos = 0;
            v_scrollpos = 0;

            if (n_strips == 1)
            {
                // Not stripping. Loop over pages, X within Y
                for (i = 0, n_page = 1; i < n_pagesy; i++)
                {
                    h_scrollpos = 0;
                    for (j = 0; j < n_pagesx; j++, n_page++)
                    {
                        if (n_page >= prd.nFromPage && n_page <= prd.nToPage)
                        {
                            // Copies come out without collation. 1,1,1,2,2,2,3,3,3...
                            for (copy = 0; copy < num_copies; copy++)
                            {
                                if (StartPage(hdc) <= 0)
                                    break;
                                hFont = CreateFont(char_height, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "Arial");
                                hFontLarge = CreateFont(2 * char_height, 0, 0, 0, FW_BOLD, 0, 0, 0, 0, 0, 0, 0, 0, "Arial");

                                hFontOld = SelectObject(hdc, hFontLarge);
                                TextOut
                                    (
                                    hdc,
                                    l_margin - h_scrollpos,
                                    t_margin - v_scrollpos,
                                    prefs->title,
                                    strlen(prefs->title)
                                    );
                                SelectObject(hdc, hFont);
                                highlight_person = NULL;  // don't print the gray boxes
                                highlight_family = NULL;
                                if (prefs->view_desc)
                                    draw_desc_boxes(hdc, prefs->root_person);
                                if (prefs->view_anc)
                                    draw_anc_boxes(hdc, prefs->root_person);

                                SelectObject(hdc, hFontOld);
                                DeleteObject(hFont);
                                DeleteObject(hFontLarge);
                                EndPage(hdc);
                            }
                        }
                        h_scrollpos += printsizex;
                    }
                    v_scrollpos += printsizey;
                }
            }
            else
            {
                // We are stripping. n_pagesy is guaranteed to be 1, so there is no loop for it.
                // Fill page with strips and copies of strips, breaking the page when it is full.
                for (j = 0, n_page = 1; j < n_pagesx; j++, n_page++)
                {
                    if (n_page >= prd.nFromPage && n_page <= prd.nToPage)
                    {
                        // Copies come out stripped, without collation. 1,1,1,2,2,2,3,3,3...
                        for (copy = 0; copy < num_copies; copy++)
                        {
                            if (v_scrollpos == 0)
                            {
                                // Start a new page
                                if (StartPage(hdc) <= 0)
                                    break;
                                hFont = CreateFont(char_height, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "Arial");
                                hFontLarge = CreateFont(2 * char_height, 0, 0, 0, FW_BOLD, 0, 0, 0, 0, 0, 0, 0, 0, "Arial");
                            }

                            // Draw contents
                            hFontOld = SelectObject(hdc, hFontLarge);
                            TextOut
                                (
                                hdc,
                                l_margin - h_scrollpos,
                                t_margin - v_scrollpos,
                                prefs->title,
                                strlen(prefs->title)
                                );
                            SelectObject(hdc, hFont);
                            highlight_person = NULL;  // don't print the gray boxes
                            highlight_family = NULL;
                            if (prefs->view_desc)
                                draw_desc_boxes(hdc, prefs->root_person);
                            if (prefs->view_anc)
                                draw_anc_boxes(hdc, prefs->root_person);

                            SelectObject(hdc, hFontOld);
                            // Advance to next strip
                            v_scrollpos -= strip_height;

                            // output cut line in grey
                            SetDCPenColor(hdc, RGB(128, 128, 128));
                            hPenOld = SelectObject(hdc, GetStockObject(DC_PEN));
                            MoveToEx(hdc, 0, -v_scrollpos, NULL);
                            LineTo(hdc, printsizex, -v_scrollpos);
                            SelectObject(hdc, hPenOld);

                            if (v_scrollpos - strip_height < -printsizey)
                            {
                                // We can't fit any more strips on the page.
                                DeleteObject(hFont);
                                DeleteObject(hFontLarge);
                                EndPage(hdc);
                                v_scrollpos = 0;
                            }
                        }
                        // Include a small overlap, since we don't know the margin of the final printer
                        // when printing to PDF
                        h_scrollpos += printsizex - l_margin;
                    }
                    else
                    {
                        // Advance to next strip
                        h_scrollpos += printsizex - l_margin;
                    }
                }
            }

            if (v_scrollpos != 0)
            {
                // Finish any page that we haven't finished.
                DeleteObject(hFont);
                DeleteObject(hFontLarge);
                EndPage(hdc);
            }
            EndDoc(hdc);
            DeleteDC(hdc);
            h_scrollpos = h_scroll_save;
            v_scrollpos = v_scroll_save;
            goto update_titlebar;

        case ID_VIEW_ZOOMIN:
            GetClientRect(hWnd, &rc);
            x_move = rc.left + (rc.right - rc.left) / 2;
            y_move = rc.top + (rc.bottom - rc.top) / 2;
        zoom_in:
            if (prefs->root_person != NULL)
            {
                prefs->zoom_percent *= ZOOM_MULTIPLE;
                h_scrollpos = (h_scrollpos + x_move) * ZOOM_MULTIPLE - x_move;
                if (h_scrollpos < 0 || h_scrollwidth - (rc.right - rc.left - 1) < 0)
                    h_scrollpos = 0;
                else if (h_scrollpos > h_scrollwidth - (rc.right - rc.left - 1))
                    h_scrollpos = h_scrollwidth - (rc.right - rc.left - 1);

                v_scrollpos = (v_scrollpos + y_move) * ZOOM_MULTIPLE - y_move;
                if (v_scrollpos < 0 || v_scrollheight - (rc.bottom - rc.top - 1) < 0)
                    v_scrollpos = 0;
                else if (v_scrollpos > v_scrollheight - (rc.bottom - rc.top - 1))
                    v_scrollpos = v_scrollheight - (rc.bottom - rc.top - 1);

                modified = TRUE;
                update_scrollbars(hWnd);
                goto update_titlebar;
            }
            break;

        case ID_VIEW_ZOOMOUT:
            GetClientRect(hWnd, &rc);
            x_move = rc.left + (rc.right - rc.left) / 2;
            y_move = rc.top + (rc.bottom - rc.top) / 2;
        zoom_out:
            if (prefs->root_person != NULL && prefs->zoom_percent > 20)  // stop ill-conditioning when % too small
            {
                prefs->zoom_percent /= ZOOM_MULTIPLE;
                h_scrollpos = (h_scrollpos + x_move) / ZOOM_MULTIPLE - x_move;
                if (h_scrollpos < 0)
                    h_scrollpos = 0;
                else if (h_scrollpos > h_scrollwidth - (rc.right - rc.left - 1))
                    h_scrollpos = h_scrollwidth - (rc.right - rc.left - 1);

                v_scrollpos = (v_scrollpos + y_move) / ZOOM_MULTIPLE - y_move;
                if (v_scrollpos < 0)
                    v_scrollpos = 0;
                else if (v_scrollpos > v_scrollheight - (rc.bottom - rc.top - 1))
                    v_scrollpos = v_scrollheight - (rc.bottom - rc.top - 1);

                modified = TRUE;
                update_scrollbars(hWnd);
                goto update_titlebar;
            }
            break;

        case ID_VIEW_DESCENDANTS:
            hMenu = GetSubMenu(GetMenu(hWnd), 1);
            prefs->view_desc = !prefs->view_desc;
            if (prefs->view_desc)
                CheckMenuItem(hMenu, ID_VIEW_DESCENDANTS, MF_CHECKED);
            else
                CheckMenuItem(hMenu, ID_VIEW_DESCENDANTS, MF_UNCHECKED);
            modified = TRUE;
            goto generate_chart;

        case ID_VIEW_ANCESTORS:
            hMenu = GetSubMenu(GetMenu(hWnd), 1);
            prefs->view_anc = !prefs->view_anc;
            if (prefs->view_anc)
                CheckMenuItem(hMenu, ID_VIEW_ANCESTORS, MF_CHECKED);
            else
                CheckMenuItem(hMenu, ID_VIEW_ANCESTORS, MF_UNCHECKED);
            modified = TRUE;
            goto generate_chart;

        case ID_VIEW_PREFERENCES:
            if (DialogBox(hInst, MAKEINTRESOURCE(IDD_PREFERENCES), hWnd, prefs_dialog) == IDOK)
            {
                modified = TRUE;
                goto generate_chart;
            }
            break;

        case 9999:              // choose a new view
        case 9999 + 1:
        case 9999 + 2:
        case 9999 + 3:
        case 9999 + 4:
        case 9999 + 5:
        case 9999 + 6:
        case 9999 + 7:
        case 9999 + 8:
        case 9999 + 9:
        case 9999 + 10:
        case 9999 + 11:
        case 9999 + 12:
        case 9999 + 13:
        case 9999 + 14:
        case 9999 + 15:
            prefs = &view_prefs[wmId - 9999];
            goto generate_chart;

        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;

        case IDM_EXIT:
            check_before_closing(hWnd);
            clear_all();
            DestroyWindow(hWnd);
            break;

        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
        break;

    case WM_MOUSEWHEEL:
        if (LOWORD(wParam) & MK_CONTROL)
        {
            GetClientRect(hWnd, &rc);       // needed for zooming code
            x_move = GET_X_LPARAM(lParam);  // zoom about the mouse position
            y_move = GET_Y_LPARAM(lParam);
            if ((((int)wParam) >> 16) > 0)   // no HIWORD here as need sign
                goto zoom_in;
            else
                goto zoom_out;
        }
        else
        {
            if ((((int)wParam) >> 16) > 0)
                wParam = SB_LINELEFT;
            else
                wParam = SB_LINERIGHT;
            goto vert_scroll;
        }
        break;

    case WM_LBUTTONDOWN:
        SetCapture(hWnd);
        x_down = GET_X_LPARAM(lParam);
        y_down = GET_Y_LPARAM(lParam);
        h_dragpos = 0;
        break;

    case WM_LBUTTONUP:
        ReleaseCapture();
        break;

    case WM_MOUSEMOVE:
        x_move = GET_X_LPARAM(lParam);
        y_move = GET_Y_LPARAM(lParam);
        if (GetCapture() == hWnd)
        {
            if (highlight_person != NULL)
            {
                int move_dx;

                // Dragging a person box. We allow a change to the offset (not the generation)
                h_dragpos += x_move - x_down;
                move_dx = (h_dragpos + min_spacing / 2) / (box_width + min_spacing);
                if (move_dx != 0)
                {
                    highlight_person->offset += move_dx;    // not recursive
                    InvalidateRect(hWnd, NULL, TRUE);
                    h_dragpos = 0;
                }
            }
            else if (highlight_family != NULL)
            {
                // Dragging a family. We allow branch to be moved in X offset only.
                int move_dx;

                h_dragpos += x_move - x_down;
                move_dx = (h_dragpos + min_spacing / 2) / (box_width + min_spacing);
                if (move_dx != 0)
                {
                    move_person_by_deltax(highlight_family->husband, move_dx);
                    InvalidateRect(hWnd, NULL, TRUE);
                    h_dragpos = 0;
                }
            }
            else
            {
                // Scrolling the content of the window.
                GetClientRect(hWnd, &rc);
                if (h_scrollwidth - (rc.right - rc.left - 1) > 0)  // disabled test
                {
                    h_scrollpos -= x_move - x_down;
                    if (h_scrollpos < 0)
                        h_scrollpos = 0;
                    else if (h_scrollpos > h_scrollwidth - (rc.right - rc.left - 1))
                        h_scrollpos = h_scrollwidth - (rc.right - rc.left - 1);

                    scrollinfo.cbSize = sizeof(SCROLLINFO);
                    scrollinfo.fMask = SIF_POS;
                    scrollinfo.nPos = h_scrollpos;
                    SetScrollInfo(hWnd, SB_HORZ, &scrollinfo, TRUE);
                }

                if (v_scrollheight - (rc.bottom - rc.top - 1) > 0)  // disabled test
                {
                    v_scrollpos -= y_move - y_down;
                    if (v_scrollpos < 0)
                        v_scrollpos = 0;
                    else if (v_scrollpos > v_scrollheight - (rc.bottom - rc.top - 1))
                        v_scrollpos = v_scrollheight - (rc.bottom - rc.top - 1);

                    scrollinfo.cbSize = sizeof(SCROLLINFO);
                    scrollinfo.fMask = SIF_POS;
                    scrollinfo.nPos = v_scrollpos;
                    SetScrollInfo(hWnd, SB_VERT, &scrollinfo, TRUE);
                }
                InvalidateRect(hWnd, NULL, TRUE);
            }
            x_down = x_move;
            y_down = y_move;
        }
        else
        {
            // Not dragging.
            // Find the person (or family) under the mouse and highlight them.
            Person *old_highlight_p = highlight_person;
            Family *old_highlight_f = highlight_family;

            highlight_person = NULL;
            highlight_family = NULL;

            for (i = 0; i <= n_person; i++)
            {
                Person *p = lookup_person[i];

                if (p != NULL)
                {
                    if (p->xbox < x_move && x_move < p->xbox + box_width)
                    {
                        if (p->ybox < y_move && y_move < p->ybox + box_height)
                        {
                            highlight_person = p;
                            break;
                        }
                    }
                }
            }

            if (highlight_person == NULL)       // look for family hits between husb and wife
            {
                for (i = 0; i <= n_family; i++)
                {
                    Family *f = lookup_family[i];

                    if (f != NULL && f->husband != NULL && f->wife  != NULL)
                    {
                        Person *h = f->husband;
                        Person *w = f->wife;
                        FamilyList *fl;

                        // Assume h and w have the same Y-coordinates.
                        if (h->ybox < y_move && y_move < h->ybox + box_height)
                        {
                            // Find the one with all the spouses (it will be on the left)
                            if (h->xbox + box_width < x_move && x_move < w->xbox)
                            {
                                // Go through the husband's spouses (the list is in lildate order)
                                for (fl = h->spouses; fl != NULL; fl = fl->next)
                                {
                                    if (x_move < fl->f->wife->xbox)
                                    {
                                        highlight_family = fl->f;
                                        break;
                                    }
                                }
                            }
                            else if (w->xbox + box_width < x_move && x_move < h->xbox)
                            {
                                // Similarly if the wife is the one on the left
                                for (fl = w->spouses; fl != NULL; fl = fl->next)
                                {
                                    if (x_move < fl->f->husband->xbox)
                                    {
                                        highlight_family = fl->f;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            if (highlight_person != old_highlight_p || highlight_family != old_highlight_f)
                InvalidateRect(hWnd, NULL, TRUE);
        }
        break;

    case WM_LBUTTONDBLCLK:
        if (highlight_person != NULL)
        {
            prefs->root_person = highlight_person;
            modified = TRUE;
            goto generate_chart;
        }
        break;

    case WM_CONTEXTMENU:
        if (highlight_person != NULL)
        {
            Person *p, *p2;
            Family *f;
            int nlink;
            BOOL has_tree;
            FamilyList *s;

            // Load menu and block nonsensical actions
            hMenu = LoadMenu(hInst, MAKEINTRESOURCE(IDR_MENUPOPUP));

            // Block add parent if family not NULL, add sibling if family is NULL
            EnableMenuItem(GetSubMenu(hMenu, 0), ID_EDIT_ADDPARENT,
                           highlight_person->family != NULL ? MF_GRAYED : MF_ENABLED);
            EnableMenuItem(GetSubMenu(hMenu, 0), ID_EDIT_ADDSIBLING,
                           highlight_person->family == NULL ? MF_GRAYED : MF_ENABLED);

            // Only allow delete if a person has exactly one link to the tree.
            // The root person cannot be deleted.
            nlink = 0;
            has_tree = FALSE;
            if (highlight_person->family != NULL)
            {
                has_tree = has_tree || highlight_person->generation < 0;    // in the ancestor chart
                nlink++;
            }
            for (s = highlight_person->spouses; s != NULL; s = s->next)
            {
                nlink++;
                if (s->f->children != NULL)
                {
                    has_tree = has_tree || highlight_person->generation > 0;   // in the descendant chart
                    nlink++;
                }
            }
            EnableMenuItem(GetSubMenu(hMenu, 0), ID_EDIT_DELETE,
                           (nlink != 1 || highlight_person == prefs->root_person) ? MF_GRAYED : MF_ENABLED);

            // Change collapse/expand menu item depending on person's state. Don't allow collapse
            // if the person has no family or ancestors, or is the root person.
            EnableMenuItem(GetSubMenu(hMenu, 0), ID_EDIT_SHOWHIDE, has_tree ? MF_ENABLED : MF_GRAYED);
            if (highlight_person->hidden)
                ModifyMenu(hMenu, ID_EDIT_SHOWHIDE, MF_BYCOMMAND, ID_EDIT_SHOWHIDE, "&Expand branch");

            // Track the menu
            cmd = TrackPopupMenu
                (
                GetSubMenu(hMenu, 0),   // person submenu
                TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
                GET_X_LPARAM(lParam),
                GET_Y_LPARAM(lParam),
                0,
                hWnd,
                NULL
                );
            DestroyMenu(hMenu);

            switch (cmd)
            {
            case ID_EDIT_MAKE_ROOT:
                prefs->root_person = highlight_person;
                modified = TRUE;
                goto generate_chart;

            case ID_EDIT_SHOWHIDE:
                highlight_person->hidden = !highlight_person->hidden;
                modified = TRUE;
                goto generate_chart;

            case ID_EDIT_PERSON:
                cmd = DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_PERSON), hWnd, person_dialog, (LPARAM)highlight_person);
                switch (cmd)
                {
                case IDCANCEL:
                    break;
                case IDOK:
                    modified = TRUE;
                    InvalidateRect(hWnd, NULL, TRUE);
                    break;
                case ID_PERSON_ADDSPOUSE:
                    goto add_spouse;
                case ID_PERSON_ADDPARENT:
                    goto add_parent;
                case ID_PERSON_ADDSIBLING:
                    goto add_sibling;
                }
                break;

            case ID_EDIT_NOTES:
                note_ptr = &highlight_person->notes;
            person_notes_dlg:
                cmd = DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_NOTES), hWnd, notes_dialog, (LPARAM)note_ptr);
                switch (cmd)
                {
                case IDOK:
                case IDCANCEL:
                    break;

                case ID_NOTES_DELETE:
                    note_ptr = remove_note(*note_ptr, &highlight_person->notes);
                    goto person_notes_dlg;
                    break;

                case ID_NOTES_NEXT:
                    note_ptr = &(*note_ptr)->next;
                    goto person_notes_dlg;
                }
                break;

            case ID_EDIT_ADDSPOUSE:
            add_spouse:
                // Preload sex for M or F and the family connections. 
                p = new_person(STATE_NEW_SPOUSE);
                f = new_family();
                if (highlight_person->sex[0] == 'F')
                {
                    strcpy_s(p->sex, 2, "M");
                    f->wife = highlight_person;
                    f->husband = p;
                }
                else
                {
                    strcpy_s(p->sex, 2, "F");
                    f->wife = p;
                    f->husband = highlight_person;
                }
                cmd = DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_PERSON), hWnd, person_dialog, (LPARAM)p);
                if (cmd == IDCANCEL)
                {
                    free(p);
                    free(f);
                    break;
                }
                register_person(p);
                register_family(f);
                p->spouses = new_familylist(f, p->spouses);
                highlight_person->spouses = new_familylist(f, highlight_person->spouses);
                modified = TRUE;
                goto generate_chart;

            case ID_EDIT_ADDPARENT:
            add_parent:
                p = new_person(STATE_NEW_PARENT);
                cmd = DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_PERSON), hWnd, person_dialog, (LPARAM)p);
                if (cmd == IDCANCEL)
                {
                    free(p);
                    break;
                }
                register_person(p);
                f = new_family();
                register_family(f);
                p2 = new_person(STATE_NEW_PARENT);
                register_person(p2);

                if (p->sex[0] == 'F')
                {
                    f->wife = p;
                    f->husband = p2;
                    p2->sex[0] = 'M';
                }
                else
                {
                    f->husband = p;
                    f->wife = p2;
                    p2->sex[0] = 'F';
                }
                p->spouses = new_familylist(f, p->spouses);
                p2->spouses = new_familylist(f, p2->spouses);
                highlight_person->family = f;
                f->children = new_personlist(highlight_person, f->children);

                // make the new parent the root so they are visible
                prefs->root_person = p;
                modified = TRUE;
                goto generate_chart;

            case ID_EDIT_ADDSIBLING:
            add_sibling:
                // Preload surname from existing child.
                p = new_person(STATE_NEW_CHILD);
                strcpy_s(p->surname, MAXSTR, highlight_person->surname);
                cmd = DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_PERSON), hWnd, person_dialog, (LPARAM)p);
                if (cmd == IDCANCEL)
                {
                    free(p);
                    break;
                }
                register_person(p);
                p->family = highlight_person->family;
                p->family->children = new_personlist(p, p->family->children);
                modified = TRUE;
                goto generate_chart;

            case ID_EDIT_DELETE:
                // This person either has one spouse and no children, or is the child of a family.
                // Other cases cannot come here because the option is disabled.
                // The root person cannot be deleted.
                p = highlight_person;
                if (p->spouses != NULL)
                {
                    ASSERT(p->spouses->next == NULL, "More than one spouse");

                    // Remove familylist from the other spouse
                    f = p->spouses->f;
                    if (p == f->husband)
                        remove_familylist(f, &f->wife->spouses);
                    else if (p == f->wife)
                        remove_familylist(f, &f->husband->spouses);
                    else
                        ASSERT(FALSE, "Person not husband or wife");
                    
                    // Free the family
                    lookup_family[f->id] = NULL;
                    free(f);
                }
                else if (highlight_person->family != NULL)
                {
                    // Remove person from the family's list of children
                    f = highlight_person->family;
                    remove_personlist(p, &f->children);
                }

                // Free the person
                lookup_person[p->id] = NULL;
                free(p);
                highlight_person = NULL;
                modified = TRUE;
                goto generate_chart;
            }
        }
        else if (highlight_family != NULL)
        {
            Person *p;

            hMenu = LoadMenu(hInst, MAKEINTRESOURCE(IDR_MENUPOPUP));
            cmd = TrackPopupMenu
            (
                GetSubMenu(hMenu, 1),   // family submenu
                TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
                GET_X_LPARAM(lParam),
                GET_Y_LPARAM(lParam),
                0,
                hWnd,
                NULL
            );
            DestroyMenu(hMenu);

            switch (cmd)
            {
            case ID_EDIT_FAMILY:
                cmd = DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_FAMILY), hWnd, family_dialog, (LPARAM)highlight_family);
                switch (cmd)
                {
                case IDCANCEL:
                    break;
                case IDOK:
                    InvalidateRect(hWnd, NULL, TRUE);
                    modified = TRUE;
                    break;
                case ID_FAMILY_ADDCHILD:
                    goto add_child;
                }
                break;

            case ID_EDIT_NOTES:
                note_ptr = &highlight_family->notes;
            family_notes_dlg:
                cmd = DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_NOTES), hWnd, notes_dialog, (LPARAM)note_ptr);
                switch (cmd)
                {
                case IDOK:
                case IDCANCEL:
                    break;

                case ID_NOTES_DELETE:
                    note_ptr = remove_note(*note_ptr, &highlight_family->notes);
                    goto family_notes_dlg;
                    break;

                case ID_NOTES_NEXT:
                    note_ptr = &(*note_ptr)->next;
                    goto family_notes_dlg;
                }
                break;

            case ID_EDIT_ADDCHILD:
            add_child:
                // Preload surname from husband of family.
                p = new_person(STATE_NEW_CHILD);
                strcpy_s(p->surname, MAXSTR, highlight_family->husband->surname);
                cmd = DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_PERSON), hWnd, person_dialog, (LPARAM)p);
                if (cmd == IDCANCEL)
                {
                    free(p);
                    break;
                }
                register_person(p);
                p->family = highlight_family;
                highlight_family->children = new_personlist(p, highlight_family->children);
                highlight_person = p;  // keep new family on screen
                modified = TRUE;
                goto generate_chart;
            }
        }
        break;

    case WM_PAINT:
        hdc = BeginPaint(hWnd, &ps);
        if (prefs->root_person != NULL)
        {
            box_width = (BOX_WIDTH * prefs->zoom_percent) / 100;
            box_height = (BOX_HEIGHT * prefs->zoom_percent) / 100;
            l_margin = L_MARGIN;
            t_margin = T_MARGIN;
            min_spacing = (MIN_SPACING * prefs->zoom_percent) / 100;
            round_diam = (ROUND_DIAM * prefs->zoom_percent) / 100;
            small_space = round_diam / 4;
            char_height = (CHAR_HEIGHT  * prefs->zoom_percent) / 100;
            hFont = CreateFont(char_height, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "Arial");
            hFontLarge = CreateFont(2 * char_height, 0, 0, 0, FW_BOLD, 0, 0, 0, 0, 0, 0, 0, 0, "Arial");
            hFontOld = SelectObject(hdc, hFont);

            // Draw the page boundaries in light grey.
            SetDCPenColor(hdc, RGB(192, 192, 192));
            hPenOld = SelectObject(hdc, GetStockObject(DC_PEN));
            pagex = (printsizex * screenx) / printx;
            n_pagesx = (h_scrollwidth + pagex - 1) / pagex;
            pagey = (printsizey * screeny) / printy;
            n_pagesy = (v_scrollheight + pagey - 1) / pagey;
            for (i = 1; i <= n_pagesx; i++)
            {
                x_move = i * pagex - h_scrollpos;
                MoveToEx(hdc, x_move, -v_scrollpos, NULL);
                LineTo(hdc, x_move, pagey * n_pagesy - v_scrollpos);
            }
            for (i = 1; i <= n_pagesy; i++)
            {
                y_move = i * pagey - v_scrollpos;
                MoveToEx(hdc, -h_scrollpos, y_move, NULL);
                LineTo(hdc, pagex * n_pagesx - h_scrollpos, y_move);
            }
            SetTextColor(hdc, RGB(192, 192, 192));
            for (i = 0, n_page = 1; i < n_pagesy; i++)
            {
                for (j = 0; j < n_pagesx; j++, n_page++)
                {
                    char buf[16];

                    x_move = j * pagex + small_space - h_scrollpos;
                    y_move = i * pagey + small_space - v_scrollpos;
                    _itoa_s(n_page, buf, 16, 10);
                    TextOut(hdc, x_move, y_move, buf, strlen(buf));
                }
            }
            SetTextColor(hdc, RGB(0, 0, 0));
            SelectObject(hdc, hPenOld);

            // Draw the content.
            SelectObject(hdc, hFontLarge);
            TextOut(hdc, L_MARGIN - h_scrollpos, T_MARGIN - v_scrollpos, prefs->title, strlen(prefs->title));
            SelectObject(hdc, hFont);
            if (prefs->view_desc)
                draw_desc_boxes(hdc, prefs->root_person);
            if (prefs->view_anc)
                draw_anc_boxes(hdc, prefs->root_person);

            SelectObject(hdc, hFontOld);
            DeleteObject(hFont);
            DeleteObject(hFontLarge);
        }
        EndPaint(hWnd, &ps);
        break;

    case WM_HSCROLL:
        // Do the bloody scrolling myself...windows is no help here.
        GetClientRect(hWnd, &rc);
        switch (LOWORD(wParam))
        {
        case SB_LINELEFT:
            h_scrollpos -= 50;
            break;
        case SB_LINERIGHT:
            h_scrollpos += 50;
            break;
        case SB_PAGELEFT:
            h_scrollpos -= rc.right - rc.left;
            break;
        case SB_PAGERIGHT:
            h_scrollpos += rc.right - rc.left;
            break;
        case SB_THUMBPOSITION:
        case SB_THUMBTRACK:
            h_scrollpos = HIWORD(wParam);
            break;
        }

        if (h_scrollpos < 0)
            h_scrollpos = 0;
        else if (h_scrollpos > h_scrollwidth - (rc.right - rc.left - 1))
            h_scrollpos = h_scrollwidth - (rc.right - rc.left - 1);

        scrollinfo.cbSize = sizeof(SCROLLINFO);
        scrollinfo.fMask = SIF_POS;
        scrollinfo.nPos = h_scrollpos;
        SetScrollInfo(hWnd, SB_HORZ, &scrollinfo, TRUE);

        InvalidateRect(hWnd, NULL, TRUE); 
        break;

    case WM_VSCROLL:
    vert_scroll:
        GetClientRect(hWnd, &rc);
        if (v_scrollheight - (rc.bottom - rc.top - 1) < 0)  // disabled test here as we can come from wheel
            break;
        switch (LOWORD(wParam))
        {
        case SB_LINELEFT:
            v_scrollpos -= 50;
            break;
        case SB_LINERIGHT:
            v_scrollpos += 50;
            break;
        case SB_PAGELEFT:
            v_scrollpos -= rc.bottom - rc.top;
            break;
        case SB_PAGERIGHT:
            v_scrollpos += rc.bottom - rc.top;
            break;
        case SB_THUMBPOSITION:
        case SB_THUMBTRACK:
            v_scrollpos = HIWORD(wParam);
            break;
        }

        if (v_scrollpos < 0)
            v_scrollpos = 0;
        else if (v_scrollpos > v_scrollheight - (rc.bottom - rc.top - 1))
            v_scrollpos = v_scrollheight - (rc.bottom - rc.top - 1);

        scrollinfo.cbSize = sizeof(SCROLLINFO);
        scrollinfo.fMask = SIF_POS;
        scrollinfo.nPos = v_scrollpos;
        SetScrollInfo(hWnd, SB_VERT, &scrollinfo, TRUE);

        InvalidateRect(hWnd, NULL, TRUE); 
        break;

    case WM_INITMENUPOPUP:
        hMenu = (HMENU)wParam;
        if (hMenu == GetSubMenu(GetMenu(hWnd), 0))
        {
            EnableMenuItem(hMenu, ID_FILE_CLOSE, prefs->root_person != NULL ? MF_ENABLED : MF_GRAYED);
            EnableMenuItem(hMenu, ID_FILE_SAVE, prefs->root_person != NULL ? MF_ENABLED : MF_GRAYED);
            EnableMenuItem(hMenu, ID_FILE_SAVEAS, prefs->root_person != NULL ? MF_ENABLED : MF_GRAYED);
            EnableMenuItem(hMenu, ID_FILE_EXPORT, prefs->root_person != NULL ? MF_ENABLED : MF_GRAYED);
            EnableMenuItem(hMenu, ID_FILE_PRINT, prefs->root_person != NULL ? MF_ENABLED : MF_GRAYED);
        }
        else if (hMenu == GetSubMenu(GetMenu(hWnd), 1))
        {
            EnableMenuItem(hMenu, ID_VIEW_ZOOMIN, prefs->root_person != NULL ? MF_ENABLED : MF_GRAYED);
            EnableMenuItem(hMenu, ID_VIEW_ZOOMOUT, prefs->root_person != NULL ? MF_ENABLED : MF_GRAYED);
            EnableMenuItem(hMenu, ID_VIEW_DESCENDANTS, prefs->root_person != NULL ? MF_ENABLED : MF_GRAYED);
            EnableMenuItem(hMenu, ID_VIEW_ANCESTORS, prefs->root_person != NULL ? MF_ENABLED : MF_GRAYED);
            CheckMenuItem(hMenu, ID_VIEW_DESCENDANTS, prefs->view_desc ? MF_CHECKED : MF_UNCHECKED);
            CheckMenuItem(hMenu, ID_VIEW_ANCESTORS, prefs->view_anc ? MF_CHECKED : MF_UNCHECKED);

            for (i = MAX_PREFS - 1; i >= 0; i--)
                DeleteMenu(hMenu, 8 + i, MF_BYPOSITION);        // after pos 8 in menu
            for (i = 0; i < n_views; i++)
            {
                ViewPrefs *vp = &view_prefs[i];

                AppendMenu(hMenu, vp == prefs ? MF_CHECKED : MF_UNCHECKED, 9999 + i, vp->title);
            }
        }

        break;

    case WM_SIZE:
        if (prefs->root_person != NULL)
            update_scrollbars(hWnd);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    case WM_CLOSE:
        check_before_closing(hWnd);
        clear_all();
        // fall through
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
