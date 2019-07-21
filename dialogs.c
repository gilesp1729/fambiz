#include "stdafx.h"
#include "Fambiz.h"
#include <CommCtrl.h>
#include <CommDlg.h>
#include <windowsx.h>
#include <stdio.h>

// Dialog boxes for editing persons, families etc.

LRESULT CALLBACK person_dialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    static Person *p;
    Event *ev;
    char buf[MAXSTR];

    switch (message)
    {
    case WM_INITDIALOG:
        p = (Person *)lParam;
        sprintf_s(buf, MAXSTR, "ID %d", p->id);
        SetDlgItemText(hDlg, IDC_STATIC_ID, buf);
        SetDlgItemText(hDlg, IDC_EDIT_GIVEN, p->given);
        SetDlgItemText(hDlg, IDC_EDIT_SURNAME, p->surname);
        SetDlgItemText(hDlg, IDC_EDIT_SEX, p->sex);
        SetDlgItemText(hDlg, IDC_EDIT_OCCUPATION, p->occupation);
        for (ev = p->event; ev != NULL; ev = ev->next)
        {
            if (ev->type == EV_BIRTH)
            {
                SetDlgItemText(hDlg, IDC_EDIT_BIRTH_DATE, ev->date);
                SetDlgItemText(hDlg, IDC_EDIT_BIRTH_PLACE, ev->place);
            }
            else if (ev->type == EV_DEATH)
            {
                SetDlgItemText(hDlg, IDC_EDIT_DEATH_DATE, ev->date);
                SetDlgItemText(hDlg, IDC_EDIT_DEATH_PLACE, ev->place);
                SetDlgItemText(hDlg, IDC_EDIT_DEATH_CAUSE, ev->cause);
            }
        }
        return 1;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
        case IDCANCEL:
            EndDialog(hDlg, LOWORD(wParam));
            return 1;
        }
        break;
    }

    return 0;
}

LRESULT CALLBACK family_dialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    static Family *f;
    Event *ev;
    char buf[MAXSTR];

    switch (message)
    {
    case WM_INITDIALOG:
        f = (Family *)lParam;
        sprintf_s(buf, MAXSTR, "ID %d", f->id);
        SetDlgItemText(hDlg, IDC_STATIC_ID, buf);
        return 1;

    case WM_COMMAND:
        switch(LOWORD(wParam))
        {
        case IDOK:
        case IDCANCEL:
            EndDialog(hDlg, LOWORD(wParam));
            return 1;
        }
        break;
    }

    return 0;
}

// Message handler for about box.
LRESULT CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return 1;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return 1;
        }
        break;
    }
    return 0;
}


