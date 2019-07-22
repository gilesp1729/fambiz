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
    char buf[MAXSTR], date[MAXSTR], place[MAXSTR], cause[MAXSTR];
    int nd, np, nc, checked;

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
        EnableWindow(GetDlgItem(hDlg, IDC_EDIT_DEATH_DATE), FALSE);
        EnableWindow(GetDlgItem(hDlg, IDC_EDIT_DEATH_PLACE), FALSE);
        EnableWindow(GetDlgItem(hDlg, IDC_EDIT_DEATH_CAUSE), FALSE);
        EnableWindow(GetDlgItem(hDlg, IDC_STATIC_DEATH_DATE), FALSE);
        EnableWindow(GetDlgItem(hDlg, IDC_STATIC_DEATH_PLACE), FALSE);
        EnableWindow(GetDlgItem(hDlg, IDC_STATIC_DEATH_CAUSE), FALSE);

        for (ev = p->event; ev != NULL; ev = ev->next)
        {
            if (ev->type == EV_BIRTH)
            {
                SetDlgItemText(hDlg, IDC_EDIT_BIRTH_DATE, ev->date);
                SetDlgItemText(hDlg, IDC_EDIT_BIRTH_PLACE, ev->place);
            }
            else if (ev->type == EV_DEATH)
            {
                CheckDlgButton(hDlg, IDC_CHECK_DECEASED, BST_CHECKED);
                EnableWindow(GetDlgItem(hDlg, IDC_EDIT_DEATH_DATE), TRUE);
                EnableWindow(GetDlgItem(hDlg, IDC_EDIT_DEATH_PLACE), TRUE);
                EnableWindow(GetDlgItem(hDlg, IDC_EDIT_DEATH_CAUSE), TRUE);
                EnableWindow(GetDlgItem(hDlg, IDC_STATIC_DEATH_DATE), TRUE);
                EnableWindow(GetDlgItem(hDlg, IDC_STATIC_DEATH_PLACE), TRUE);
                EnableWindow(GetDlgItem(hDlg, IDC_STATIC_DEATH_CAUSE), TRUE);
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
        case ID_PERSON_ADDSPOUSE:
        case ID_PERSON_ADDPARENT:
            GetDlgItemText(hDlg, IDC_EDIT_GIVEN, p->given, MAXSTR);
            GetDlgItemText(hDlg, IDC_EDIT_SURNAME, p->surname, MAXSTR);
            p->sex[1] = '\0';    // sex is only one letter
            GetDlgItemText(hDlg, IDC_EDIT_SEX, p->sex, 2);
            GetDlgItemText(hDlg, IDC_EDIT_OCCUPATION, p->occupation, MAXSTR);

            // Find an existing event, or create a new one.
            GetDlgItemText(hDlg, IDC_EDIT_BIRTH_DATE, date, MAXSTR);
            GetDlgItemText(hDlg, IDC_EDIT_BIRTH_PLACE, place, MAXSTR);
            ev = find_event(EV_BIRTH, &p->event);
            strcpy_s(ev->date, MAXSTR, date);
            strcpy_s(ev->place, MAXSTR, place);

            nd = GetDlgItemText(hDlg, IDC_EDIT_DEATH_DATE, date, MAXSTR);
            np = GetDlgItemText(hDlg, IDC_EDIT_DEATH_PLACE, place, MAXSTR);
            nc = GetDlgItemText(hDlg, IDC_EDIT_DEATH_CAUSE, cause, MAXSTR);
            if (nd != 0 || np != 0 || nc != 0 || IsDlgButtonChecked(hDlg, IDC_CHECK_DECEASED))
            {
                // If there is data for death, and the box is ticked, add/edit an event record. Otherwise, remove it.
                ev = find_event(EV_DEATH, &p->event);
                strcpy_s(ev->date, MAXSTR, date);
                strcpy_s(ev->place, MAXSTR, place);
                strcpy_s(ev->cause, MAXSTR, cause);
            }
            else
            {
                remove_event(EV_DEATH, &p->event);
            }
            // fall through
        case IDCANCEL:
            EndDialog(hDlg, LOWORD(wParam));
            return 1;

        case ID_PERSON_NOTES:
            DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_NOTES), hDlg, notes_dialog, (LPARAM)&p->notes);
            break;

        case IDC_CHECK_DECEASED:
            checked = IsDlgButtonChecked(hDlg, IDC_CHECK_DECEASED);
            EnableWindow(GetDlgItem(hDlg, IDC_EDIT_DEATH_DATE), checked);
            EnableWindow(GetDlgItem(hDlg, IDC_EDIT_DEATH_PLACE), checked);
            EnableWindow(GetDlgItem(hDlg, IDC_EDIT_DEATH_CAUSE), checked);
            EnableWindow(GetDlgItem(hDlg, IDC_STATIC_DEATH_DATE), checked);
            EnableWindow(GetDlgItem(hDlg, IDC_STATIC_DEATH_PLACE), checked);
            EnableWindow(GetDlgItem(hDlg, IDC_STATIC_DEATH_CAUSE), checked);
        }
        break;
    }

    return 0;
}

LRESULT CALLBACK family_dialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    static Family *f;
    Event *ev;
    char buf[MAXSTR], date[MAXSTR], place[MAXSTR];
    int nd, np, checked;

    switch (message)
    {
    case WM_INITDIALOG:
        f = (Family *)lParam;
        sprintf_s(buf, MAXSTR, "ID %d", f->id);
        SetDlgItemText(hDlg, IDC_STATIC_ID, buf);
        sprintf_s(buf, MAXSTR, "%s %s", f->husband->given, f->husband->surname);
        SetDlgItemText(hDlg, IDC_STATIC_HUSBAND, buf);
        sprintf_s(buf, MAXSTR, "%s %s", f->wife->given, f->wife->surname);
        SetDlgItemText(hDlg, IDC_STATIC_WIFE, buf);
        EnableWindow(GetDlgItem(hDlg, IDC_EDIT_MARR_DATE), FALSE);
        EnableWindow(GetDlgItem(hDlg, IDC_EDIT_MARR_PLACE), FALSE);
        EnableWindow(GetDlgItem(hDlg, IDC_EDIT_DIV_DATE), FALSE);
        EnableWindow(GetDlgItem(hDlg, IDC_EDIT_DIV_PLACE), FALSE);
        EnableWindow(GetDlgItem(hDlg, IDC_STATIC_MARR_DATE), FALSE);
        EnableWindow(GetDlgItem(hDlg, IDC_STATIC_MARR_PLACE), FALSE);
        EnableWindow(GetDlgItem(hDlg, IDC_STATIC_DIV_DATE), FALSE);
        EnableWindow(GetDlgItem(hDlg, IDC_STATIC_DIV_PLACE), FALSE);

        for (ev = f->event; ev != NULL; ev = ev->next)
        {
            if (ev->type == EV_MARRIAGE)
            {
                CheckDlgButton(hDlg, IDC_CHECK_MARR, BST_CHECKED);
                EnableWindow(GetDlgItem(hDlg, IDC_EDIT_MARR_DATE), TRUE);
                EnableWindow(GetDlgItem(hDlg, IDC_EDIT_MARR_PLACE), TRUE);
                EnableWindow(GetDlgItem(hDlg, IDC_STATIC_MARR_DATE), TRUE);
                EnableWindow(GetDlgItem(hDlg, IDC_STATIC_MARR_PLACE), TRUE);
                SetDlgItemText(hDlg, IDC_EDIT_MARR_DATE, ev->date);
                SetDlgItemText(hDlg, IDC_EDIT_MARR_PLACE, ev->place);
            }
            else if (ev->type == EV_DIVORCE)
            {
                CheckDlgButton(hDlg, IDC_CHECK_DIV, BST_CHECKED);
                EnableWindow(GetDlgItem(hDlg, IDC_EDIT_DIV_DATE), TRUE);
                EnableWindow(GetDlgItem(hDlg, IDC_EDIT_DIV_PLACE), TRUE);
                EnableWindow(GetDlgItem(hDlg, IDC_STATIC_DIV_DATE), TRUE);
                EnableWindow(GetDlgItem(hDlg, IDC_STATIC_DIV_PLACE), TRUE);
                SetDlgItemText(hDlg, IDC_EDIT_DIV_DATE, ev->date);
                SetDlgItemText(hDlg, IDC_EDIT_DIV_PLACE, ev->place);
            }
        }
        return 1;

    case WM_COMMAND:
        switch(LOWORD(wParam))
        {
        case IDOK:
        case ID_FAMILY_ADDCHILD:
            nd = GetDlgItemText(hDlg, IDC_EDIT_MARR_DATE, date, MAXSTR);
            np = GetDlgItemText(hDlg, IDC_EDIT_MARR_PLACE, place, MAXSTR);
            if (nd != 0 || np != 0 || IsDlgButtonChecked(hDlg, IDC_CHECK_MARR))
            {
                // If there is data for a marriage, and the box is ticked, add/edit an event record. Otherwise, remove it.
                ev = find_event(EV_MARRIAGE, &f->event);
                strcpy_s(ev->date, MAXSTR, date);
                strcpy_s(ev->place, MAXSTR, place);
            }
            else
            {
                remove_event(EV_MARRIAGE, &f->event);
            }

            nd = GetDlgItemText(hDlg, IDC_EDIT_DIV_DATE, date, MAXSTR);
            np = GetDlgItemText(hDlg, IDC_EDIT_DIV_PLACE, place, MAXSTR);
            if (nd != 0 && np != 0 || IsDlgButtonChecked(hDlg, IDC_CHECK_DIV))
            {
                ev = find_event(EV_DIVORCE, &f->event);
                strcpy_s(ev->date, MAXSTR, date);
                strcpy_s(ev->place, MAXSTR, place);
            }
            else
            {
                remove_event(EV_DIVORCE, &f->event);
            }
            // fall through
        case IDCANCEL:
            EndDialog(hDlg, LOWORD(wParam));
            return 1;

        case ID_FAMILY_NOTES:
            DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_NOTES), hDlg, notes_dialog, (LPARAM)&f->notes);
            break;

        case IDC_CHECK_MARR:
            checked = IsDlgButtonChecked(hDlg, IDC_CHECK_MARR);
            EnableWindow(GetDlgItem(hDlg, IDC_EDIT_MARR_DATE), checked);
            EnableWindow(GetDlgItem(hDlg, IDC_EDIT_MARR_PLACE), checked);
            EnableWindow(GetDlgItem(hDlg, IDC_STATIC_MARR_DATE), checked);
            EnableWindow(GetDlgItem(hDlg, IDC_STATIC_MARR_PLACE), checked);
            break;

        case IDC_CHECK_DIV:
            checked = IsDlgButtonChecked(hDlg, IDC_CHECK_DIV);
            EnableWindow(GetDlgItem(hDlg, IDC_EDIT_DIV_DATE), checked);
            EnableWindow(GetDlgItem(hDlg, IDC_EDIT_DIV_PLACE), checked);
            EnableWindow(GetDlgItem(hDlg, IDC_STATIC_DIV_DATE), checked);
            EnableWindow(GetDlgItem(hDlg, IDC_STATIC_DIV_PLACE), checked);
            break;
        }
        break;
    }

    return 0;
}

LRESULT CALLBACK notes_dialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    static Note **np;

    switch (message)
    {
    case WM_INITDIALOG:
        np = (Note **)lParam;
        return 0;

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


