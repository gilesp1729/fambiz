#include "stdafx.h"
#include "Fambiz.h"
#include <CommCtrl.h>
#include <CommDlg.h>
#include <windowsx.h>
#include <stdio.h>

// Graphics stuff for charts.

#define BOX_WIDTH       90
#define BOX_HEIGHT      55
#define MIN_SPACING     25
#define ROUND_DIAM      10
#define CHAR_HEIGHT     8
#define ZOOM_MULTIPLE   1.2

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
int zoom_percent = 200;
int box_width;
int box_height;
int min_spacing;
int round_diam;
int small_space;
int char_height;


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

    p->generation = gen;

    // Get widths of p and spouse.
    p->accum_width = 1;
    for (fl = p->spouses; fl != NULL; fl = fl->next)
    {
        s = find_spouse(p, fl->f);
        s->generation = gen;
        p->accum_width++;
    }
        
    // Accumulate widths of children and their descendants depth-first.
    child_width = 0;
    for (fl = p->spouses; fl != NULL; fl = fl->next)
    {
        f = fl->f;
        for (cl = f->children; cl != NULL; cl = cl->next)
        {
            c = cl->p;
            determine_desc_widths(c, gen + 1);
            child_width += c->accum_width;
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

    // Accumulate offsets of children and their descendants depth-first.
    for (fl = p->spouses; fl != NULL; fl = fl->next)
    {
        fl->family_max_offset = max_offset - 1;
        f = fl->f;
        for (cl = f->children; cl != NULL; cl = cl->next)
        {
            c = cl->p;
            determine_desc_offsets(c, gen + 1);
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

    p->generation = gen;
    p->accum_width = 1;

    if (gen < anc_generations)
        anc_generations = gen;
    if (f == NULL)
        return;

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

    if (parent_width > p->accum_width)
        p->accum_width = parent_width;
}

// Determine offsets of ancestors of person p.
void
determine_anc_offsets(Person *p, int gen)
{
    Family *f = p->family;
    int last_offset;

    if (f != NULL)
    {
        if (f->husband != NULL)
            determine_anc_offsets(f->husband, gen - 1);
        if (f->wife != NULL)
            determine_anc_offsets(f->wife, gen - 1);
    }

    // Get offset of p based on stuff above.
    if (gen == 0 && view_desc)
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
    int x_box = p->offset * (box_width + min_spacing) + (min_spacing / 2) - h_scrollpos;
    int y_box = (p->generation - anc_generations) * (box_height + min_spacing) + (min_spacing / 2) - v_scrollpos;
    int x_text, y_text;
    Event *ev;
    char buf[MAXSTR];

    if (p == highlight_person)
        SelectObject(hdc, GetStockObject(LTGRAY_BRUSH));
    else
        SelectObject(hdc, GetStockObject(NULL_BRUSH));

    y_text = y_box + small_space;
    if (p->sex[0] == 'M')
    {
        Rectangle(hdc, x_box, y_box, x_box + box_width, y_box + box_height);
        if (p == root_person)
            Rectangle(hdc, x_box + 2, y_box + 2, x_box + box_width - 2, y_box + box_height - 2);
        x_text = x_box + small_space;
    }
    else
    {
        RoundRect(hdc, x_box, y_box, x_box + box_width, y_box + box_height, round_diam, round_diam);
        if (p == root_person)
            RoundRect(hdc, x_box + 2, y_box + 2, x_box + box_width - 2, y_box + box_height - 2, round_diam, round_diam);
        x_text = x_box + 2 * small_space;
    }

    // Wrap text at the surname if it won't fit.
    wrap_text_out(hdc, x_text, &y_text, p->name, strlen(p->name));
    for (ev = p->event; ev != NULL; ev = ev->next)
    {
        sprintf_s(buf, MAXSTR, "%s %s", codes[ev->type].display, ev->date);
        wrap_text_out(hdc, x_text, &y_text, buf, strlen(buf));
        if (ev->place != NULL && ev->place[0] != '\0')
            wrap_text_out(hdc, x_text, &y_text, ev->place, strlen(ev->place));
    }
#if 1 //def DEBUG_CHART
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

        // draw any marriage/divorce dates.
        y_event = s->ybox + box_height;
        for (ev = f->event; ev != NULL; ev = ev->next)
        {
            char buf[64];

            sprintf_s(buf, 64, "%s %s", codes[ev->type].display, ev->date);
            TextOut(hdc, s->xbox + small_space, y_event, buf, strlen(buf));
            y_event += char_height;
        }

        // connect marriage double lines to children.
        if (f->children != NULL)
        {
            //x_line = (s->xbox + prev->xbox + box_width) / 2;
            x_line = s->xbox - min_spacing / 2;                     // TODO this could go the other way for long marriage lines, also do in ancestors
            MoveToEx(hdc, x_line, y_line, NULL);
            y_line = p->ybox + box_height + min_spacing / 2;
            LineTo(hdc, x_line, y_line);
        }
        prev = s;

        // draw children of the family
        for (cl = f->children; cl != NULL; cl = cl->next)
        {
            c = cl->p;
            draw_desc_boxes(hdc, c);
            MoveToEx(hdc, x_line, y_line,NULL);
            LineTo(hdc, c->xbox + box_width / 2, y_line);
            LineTo(hdc, c->xbox + box_width / 2, c->ybox);
        }
    }
}

// Draw boxes and connecting lines for ancestors of person p.
void
draw_anc_boxes(HDC hdc, Person *p)
{
    Family *f = p->family;
    int x_line, y_line;
    Person *h, *w;

    draw_box(hdc, p);
    if (f == NULL)
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

        y_line = h->ybox + box_height / 2;
        MoveToEx(hdc, w->xbox, y_line, NULL);
        LineTo(hdc, h->xbox + box_width - 1, y_line);
        y_line += small_space;
        MoveToEx(hdc, w->xbox, y_line, NULL);
        LineTo(hdc, h->xbox + box_width - 1, y_line);

        x_line = (h->xbox + box_width + w->xbox) / 2;
        MoveToEx(hdc, x_line, y_line, NULL);
        y_line = p->ybox - min_spacing / 2;
        LineTo(hdc, x_line, y_line);
        LineTo(hdc, p->xbox + box_width / 2, y_line);
        LineTo(hdc, p->xbox + box_width / 2, p->ybox);
    }
}

void
update_scrollbars(HWND hWnd, int max_offset, int num_generations)
{
    RECT rc;
    SCROLLINFO hscrollinfo, vscrollinfo;

    GetClientRect(hWnd, &rc);

    h_scrollwidth = (max_offset + 1) * ((BOX_WIDTH + MIN_SPACING) * zoom_percent) / 100;
    if (h_scrollpos < 0 || h_scrollwidth - (rc.right - rc.left - 1) < 0)
        h_scrollpos = 0;
    else if (h_scrollpos > h_scrollwidth - (rc.right - rc.left - 1) < 0)
        h_scrollpos = h_scrollwidth - (rc.right - rc.left - 1);

    v_scrollheight = (num_generations + 1) * ((BOX_HEIGHT + MIN_SPACING) * zoom_percent) / 100;
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
    int wmId, wmEvent, i, j;
    PAINTSTRUCT ps;
    HDC hdc;
    HMENU hMenu;
    HFONT hFont, hFontOld;
    HPEN hPenOld;
    HBRUSH hBrushOld;
    OPENFILENAME ofn;
    static PRINTDLG prd;
    static int printx, printy, screenx, screeny, printsizex, printsizey;
    int h_scroll_save, v_scroll_save;
    int pagex, n_pagesx, pagey, n_pagesy, n_page;
    DOCINFO di;
    int printer_percentx, printer_percenty;
    SCROLLINFO scrollinfo;
    RECT rc;
    int cmd;
    int x_move, y_move;
    static int h_dragpos;
    static int x_down, y_down;
    int desc_accum_width, anc_accum_width;

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

        root_person = NULL;
        break;

    case WM_COMMAND:
        wmId = LOWORD(wParam);
        wmEvent = HIWORD(wParam);
        // Parse the menu selections:
        switch (wmId)
        {
        case ID_FILE_OPEN:
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
            root_person = lookup_person[1];         // TODO temporary, until saving/restoring done

        generate_chart:
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
                }
            }

            // Generate widths. Take care of root_person->accum_width being overwritten!
            if (view_desc)
                determine_desc_widths(root_person, 0);
            desc_accum_width = root_person->accum_width;
            if (view_anc)
                determine_anc_widths(root_person, 0);
            anc_accum_width = root_person->accum_width;

            if (view_desc && view_anc)
            {
                // Update gen offsets to accommodate ancestors wider than descendants,
                // or the converse.
                if (desc_accum_width > anc_accum_width)
                {
                    root_person->accum_width = desc_accum_width;
                    determine_desc_offsets(root_person, 0);
                    for (i = -1; i >= -CENTRE_GEN; i--)
                        gen_offset[i] = (desc_accum_width - anc_accum_width) / 2;
                    root_person->accum_width = anc_accum_width;
                    determine_anc_offsets(root_person, 0);
                }
                else if (anc_accum_width > desc_accum_width)
                {
                    root_person->accum_width = anc_accum_width;
                    determine_anc_offsets(root_person, 0);
                    for (i = 0; i < CENTRE_GEN; i++)    // start at 0 as root offset not stored
                        gen_offset[i] = (anc_accum_width - desc_accum_width) / 2;
                    root_person->accum_width = desc_accum_width;
                    determine_desc_offsets(root_person, 0);
                }
            }
            else
            {
                // Generate all offsets. Replace overwritten accum_widths.
                root_person->accum_width = desc_accum_width;
                if (view_desc)
                    determine_desc_offsets(root_person, 0);
                root_person->accum_width = anc_accum_width;
                if (view_anc)
                    determine_anc_offsets(root_person, 0);
            }

            // Set up scroll bars to reflect total width and height and update view.
            h_scrollpos = 0;
            v_scrollpos = 0;
            update_scrollbars(hWnd, max_offset, desc_generations - anc_generations);  // anc_generations is negative
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
            break;

        case ID_FILE_NEW:
        case ID_FILE_CLOSE:
            // TODO check for changes and save, then free everything
            root_person = NULL;
            curr_filename[0] = '\0';
            InvalidateRect(hWnd, NULL, TRUE);
            break;

        case ID_FILE_PAGESETUP:
            prd.Flags = PD_PRINTSETUP | PD_RETURNDC;
            if (!PrintDlg(&prd))
                break;

            // recalculate page sizes and redraw page boundaries
            printx = GetDeviceCaps(prd.hDC, LOGPIXELSX);
            printy = GetDeviceCaps(prd.hDC, LOGPIXELSY);
            printsizex = GetDeviceCaps(prd.hDC, HORZRES);
            printsizey = GetDeviceCaps(prd.hDC, VERTRES);
            DeleteDC(prd.hDC);
            if (root_person != NULL)
                InvalidateRect(hWnd, NULL, TRUE);
            break;

        case ID_FILE_PRINT:
            prd.Flags = PD_RETURNDC;
            prd.nCopies = 1;
            prd.nFromPage = 1;
            prd.nToPage = 0xFFFF;
            prd.nMinPage = 1;
            prd.nMaxPage = 0xFFFF;
            if (!PrintDlg(&prd))
                break;

            hdc = prd.hDC;
            memset(&di, 0, sizeof(DOCINFO));
            di.cbSize = sizeof(DOCINFO);
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

            printer_percentx = (printx * zoom_percent) / screenx;
            printer_percenty = (printy * zoom_percent) / screeny;
            box_width = (BOX_WIDTH * printer_percentx) / 100;
            box_height = (BOX_HEIGHT * printer_percenty) / 100;
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

            // Loop over pages
            for (i = 0, n_page = 1; i < n_pagesy; i++)
            {
                h_scrollpos = 0;
                for (j = 0; j < n_pagesx; j++, n_page++)
                {
                    if (n_page >= prd.nFromPage && n_page <= prd.nToPage)
                    {
                        if (StartPage(hdc) <= 0)
                            break;
                        hFont = CreateFont(char_height, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "Arial");
                        hFontOld = SelectObject(hdc, hFont);
                        hBrushOld = GetStockObject(NULL_BRUSH);

                        highlight_person = NULL;  // don't print the gray boxes
                        highlight_family = NULL;
                        if (view_desc)
                            draw_desc_boxes(hdc, root_person);
                        if (view_anc)
                            draw_anc_boxes(hdc, root_person);

                        SelectObject(hdc, hBrushOld);
                        SelectObject(hdc, hFontOld);
                        DeleteObject(hFont);
                        EndPage(hdc);
                    }
                    h_scrollpos += printsizex;
                }
                v_scrollpos += printsizey;
            }
            EndDoc(hdc);
            DeleteDC(hdc);
            h_scrollpos = h_scroll_save;
            v_scrollpos = v_scroll_save;
            break;

        case ID_VIEW_ZOOMIN:
            GetClientRect(hWnd, &rc);
            x_move = rc.left + (rc.right - rc.left) / 2;
            y_move = rc.top + (rc.bottom - rc.top) / 2;
        zoom_in:
            if (root_person != NULL)
            {
                zoom_percent *= ZOOM_MULTIPLE;
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

                update_scrollbars(hWnd, max_offset, desc_generations - anc_generations);
            }
            break;

        case ID_VIEW_ZOOMOUT:
            GetClientRect(hWnd, &rc);
            x_move = rc.left + (rc.right - rc.left) / 2;
            y_move = rc.top + (rc.bottom - rc.top) / 2;
        zoom_out:
            if (root_person != NULL)
            {
                zoom_percent /= ZOOM_MULTIPLE;
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

                update_scrollbars(hWnd, max_offset, desc_generations - anc_generations);
            }
            break;

        case ID_VIEW_DESCENDANTS:
            hMenu = GetSubMenu(GetMenu(hWnd), 1);
            view_desc = !view_desc;
            if (view_desc)
                CheckMenuItem(hMenu, ID_VIEW_DESCENDANTS, MF_CHECKED);
            else
                CheckMenuItem(hMenu, ID_VIEW_DESCENDANTS, MF_UNCHECKED);
            goto generate_chart;

        case ID_VIEW_ANCESTORS:
            hMenu = GetSubMenu(GetMenu(hWnd), 1);
            view_anc = !view_anc;
            if (view_anc)
                CheckMenuItem(hMenu, ID_VIEW_ANCESTORS, MF_CHECKED);
            else
                CheckMenuItem(hMenu, ID_VIEW_ANCESTORS, MF_UNCHECKED);
            goto generate_chart;

        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;

        case IDM_EXIT:
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

                        if (h->ybox < y_move && y_move < h->ybox + box_height)
                        {
                            if 
                            (
                                h->xbox + box_width < x_move && x_move < w->xbox
                                ||
                                w->xbox + box_width < x_move && x_move < h->xbox
                            )
                            {
                                highlight_family = f;
                                break;
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
            root_person = highlight_person;
            goto generate_chart;
        }
        break;

    case WM_CONTEXTMENU:
        if (highlight_person != NULL)
        {
            hMenu = LoadMenu(hInst, MAKEINTRESOURCE(IDR_MENUPOPUP));
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
                root_person = highlight_person;
                goto generate_chart;
                break;



            }
        }
        else if (highlight_family != NULL)
        {
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



            }
        }
        break;

    case WM_PAINT:
        hdc = BeginPaint(hWnd, &ps);
        if (root_person != NULL)
        {
            box_width = (BOX_WIDTH * zoom_percent) / 100;
            box_height = (BOX_HEIGHT * zoom_percent) / 100;
            min_spacing = (MIN_SPACING * zoom_percent) / 100;
            round_diam = (ROUND_DIAM * zoom_percent) / 100;
            small_space = round_diam / 4;
            char_height = (CHAR_HEIGHT  * zoom_percent) / 100;
            hFont = CreateFont(char_height, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "Arial");
            hFontOld = SelectObject(hdc, hFont);
            hBrushOld = GetStockObject(NULL_BRUSH);
            screenx = GetDeviceCaps(hdc, LOGPIXELSX);
            screeny = GetDeviceCaps(hdc, LOGPIXELSY);

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
            if (view_desc)
                draw_desc_boxes(hdc, root_person);
            if (view_anc)
                draw_anc_boxes(hdc, root_person);

            SelectObject(hdc, hBrushOld);
            SelectObject(hdc, hFontOld);
            DeleteObject(hFont);
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
            EnableMenuItem(hMenu, ID_FILE_CLOSE, root_person != NULL ? MF_ENABLED : MF_GRAYED);
            EnableMenuItem(hMenu, ID_FILE_PRINT, root_person != NULL ? MF_ENABLED : MF_GRAYED);
        }
        else if (hMenu == GetSubMenu(GetMenu(hWnd), 1))
        {
            EnableMenuItem(hMenu, ID_VIEW_ZOOMIN, root_person != NULL ? MF_ENABLED : MF_GRAYED);
            EnableMenuItem(hMenu, ID_VIEW_ZOOMOUT, root_person != NULL ? MF_ENABLED : MF_GRAYED);
            EnableMenuItem(hMenu, ID_VIEW_DESCENDANTS, root_person != NULL ? MF_ENABLED : MF_GRAYED);
            EnableMenuItem(hMenu, ID_VIEW_ANCESTORS, root_person != NULL ? MF_ENABLED : MF_GRAYED);
        }

        break;

    case WM_SIZE:
        if (root_person != NULL)
            update_scrollbars(hWnd, max_offset, desc_generations - anc_generations);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
