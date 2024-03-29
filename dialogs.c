#include "stdafx.h"
#include "Fambiz.h"
#include <CommCtrl.h>
#include <CommDlg.h>
#include <windowsx.h>
#include <shellapi.h>
#include <stdio.h>


// Dialog boxes for editing persons, families etc.

LRESULT CALLBACK person_dialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    static Person *p;
    Event *ev;
    Note **note_ptr;
    Attachment **att_ptr;
    char buf[MAXSTR], date[MAXSTR], place[MAXSTR], cause[MAXSTR];
    int nd, np, nc, checked, cmd;

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
        EnableWindow(GetDlgItem(hDlg, IDC_EDIT_BURIAL_PLACE), FALSE);
        EnableWindow(GetDlgItem(hDlg, IDC_EDIT_DEATH_CAUSE), FALSE);
        EnableWindow(GetDlgItem(hDlg, IDC_STATIC_DEATH_DATE), FALSE);
        EnableWindow(GetDlgItem(hDlg, IDC_STATIC_DEATH_PLACE), FALSE);
        EnableWindow(GetDlgItem(hDlg, IDC_STATIC_DEATH_CAUSE), FALSE);
        EnableWindow(GetDlgItem(hDlg, IDC_STATIC_BURIAL_PLACE), FALSE);

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
            else if (ev->type == EV_BURIAL)
            {
                EnableWindow(GetDlgItem(hDlg, IDC_EDIT_BURIAL_PLACE), TRUE);
                EnableWindow(GetDlgItem(hDlg, IDC_STATIC_BURIAL_PLACE), TRUE);
                SetDlgItemText(hDlg, IDC_EDIT_BURIAL_PLACE, ev->place);
            }
        }

        // cope with dead people that have no burial event
        if (IsDlgButtonChecked(hDlg, IDC_CHECK_DECEASED))
        {
            EnableWindow(GetDlgItem(hDlg, IDC_EDIT_BURIAL_PLACE), TRUE);
            EnableWindow(GetDlgItem(hDlg, IDC_STATIC_BURIAL_PLACE), TRUE);
        }

        // modify title and enabled buttons, depending on who we are editing here
        switch (p->state)
        {
        case STATE_EXISTING:
            break;              // all OK leave it alone
        case STATE_NEW_CHILD:
            SetWindowText(hDlg, "Edit New Child");
            EnableWindow(GetDlgItem(hDlg, ID_PERSON_ADDPARENT), FALSE);
            EnableWindow(GetDlgItem(hDlg, ID_PERSON_ADDSPOUSE), FALSE);
            break;
        case STATE_NEW_PARENT:
            SetWindowText(hDlg, "Edit New Parent");
            EnableWindow(GetDlgItem(hDlg, ID_PERSON_ADDPARENT), FALSE);
            EnableWindow(GetDlgItem(hDlg, ID_PERSON_ADDSPOUSE), FALSE);
            EnableWindow(GetDlgItem(hDlg, ID_PERSON_ADDSIBLING), FALSE);
            break;
        case STATE_NEW_SPOUSE:
            SetWindowText(hDlg, "Edit New Spouse");
            EnableWindow(GetDlgItem(hDlg, ID_PERSON_ADDPARENT), FALSE);
            EnableWindow(GetDlgItem(hDlg, ID_PERSON_ADDSPOUSE), FALSE);
            EnableWindow(GetDlgItem(hDlg, ID_PERSON_ADDSIBLING), FALSE);
            break;
        }

        // block nonsensical actions
        if (p->family != NULL)
            EnableWindow(GetDlgItem(hDlg, ID_PERSON_ADDPARENT), FALSE);
        else
            EnableWindow(GetDlgItem(hDlg, ID_PERSON_ADDSIBLING), FALSE);

        return 1;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
        case ID_PERSON_ADDSPOUSE:
        case ID_PERSON_ADDPARENT:
        case ID_PERSON_ADDSIBLING:
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
            if (ev->date[0] != '\0')
                p->lildate = parse_date_lilian(ev->date, FALSE);
            else
                p->lildate = 0;

            nd = GetDlgItemText(hDlg, IDC_EDIT_DEATH_DATE, date, MAXSTR);
            np = GetDlgItemText(hDlg, IDC_EDIT_DEATH_PLACE, place, MAXSTR);
            nc = GetDlgItemText(hDlg, IDC_EDIT_DEATH_CAUSE, cause, MAXSTR);
            if (nd != 0 || np != 0 || nc != 0 || IsDlgButtonChecked(hDlg, IDC_CHECK_DECEASED))
            {
                // If there is data for death, or the box is ticked, add/edit an event record. Otherwise, remove it.
                ev = find_event(EV_DEATH, &p->event);
                strcpy_s(ev->date, MAXSTR, date);
                strcpy_s(ev->place, MAXSTR, place);
                strcpy_s(ev->cause, MAXSTR, cause);
            }
            else
            {
                remove_event(EV_DEATH, &p->event);
            }

            np = GetDlgItemText(hDlg, IDC_EDIT_BURIAL_PLACE, place, MAXSTR);
            if (np != 0)    // don't check button. We don't want a burial event if there is no place given.
            {
                ev = find_event(EV_BURIAL, &p->event);
                strcpy_s(ev->place, MAXSTR, place);
            }
            else
            {
                remove_event(EV_BURIAL, &p->event);
            }
            // fall through
        case IDCANCEL:
            EndDialog(hDlg, LOWORD(wParam));
            return 1;

        case ID_PERSON_NOTES:
            note_ptr = &p->notes;
        person_notes_dlg:
            cmd = DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_NOTES), hDlg, notes_dialog, (LPARAM)note_ptr);
            switch (cmd)
            {
            case IDOK:
            case IDCANCEL:
                break;
            
            case ID_NOTES_DELETE:
                note_ptr = remove_note(*note_ptr, &p->notes);
                goto person_notes_dlg;
                break;

            case ID_NOTES_NEXT:
                note_ptr = &(*note_ptr)->next;
                goto person_notes_dlg;
            }
            break;

        case ID_PERSON_ATTACHMENTS:
            att_ptr = &p->attach;
        person_att_dlg:
            cmd = DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_ATTACHMENTS), hDlg, attachments_dialog, (LPARAM)att_ptr);
            switch (cmd)
            {
            case IDOK:
            case IDCANCEL:
                break;

            case ID_ATT_DELETE:
                att_ptr = remove_attachment(*att_ptr, &p->attach);
                goto person_att_dlg;
                break;

            case ID_ATT_NEXT:
                att_ptr = &(*att_ptr)->next;
                goto person_att_dlg;
            }
            break;

        case IDC_CHECK_DECEASED:
            checked = IsDlgButtonChecked(hDlg, IDC_CHECK_DECEASED);
            EnableWindow(GetDlgItem(hDlg, IDC_EDIT_DEATH_DATE), checked);
            EnableWindow(GetDlgItem(hDlg, IDC_EDIT_DEATH_PLACE), checked);
            EnableWindow(GetDlgItem(hDlg, IDC_EDIT_DEATH_CAUSE), checked);
            EnableWindow(GetDlgItem(hDlg, IDC_EDIT_BURIAL_PLACE), checked);
            EnableWindow(GetDlgItem(hDlg, IDC_STATIC_DEATH_DATE), checked);
            EnableWindow(GetDlgItem(hDlg, IDC_STATIC_DEATH_PLACE), checked);
            EnableWindow(GetDlgItem(hDlg, IDC_STATIC_DEATH_CAUSE), checked);
            EnableWindow(GetDlgItem(hDlg, IDC_STATIC_BURIAL_PLACE), checked);
        }
        break;
    }

    return 0;
}

LRESULT CALLBACK family_dialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    static Family *f;
    Event *ev;
    Note **note_ptr;
    Attachment **att_ptr;
    char buf[MAXSTR], date[MAXSTR], place[MAXSTR], mtype[MAXSTR];
    int nd, np, nt, checked, cmd;

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
        EnableWindow(GetDlgItem(hDlg, IDC_EDIT_MARR_TYPE), FALSE);
        EnableWindow(GetDlgItem(hDlg, IDC_EDIT_DIV_DATE), FALSE);
        EnableWindow(GetDlgItem(hDlg, IDC_EDIT_DIV_PLACE), FALSE);
        EnableWindow(GetDlgItem(hDlg, IDC_EDIT_DIV_TYPE), FALSE);
        EnableWindow(GetDlgItem(hDlg, IDC_STATIC_MARR_DATE), FALSE);
        EnableWindow(GetDlgItem(hDlg, IDC_STATIC_MARR_PLACE), FALSE);
        EnableWindow(GetDlgItem(hDlg, IDC_STATIC_MARR_TYPE), FALSE);
        EnableWindow(GetDlgItem(hDlg, IDC_STATIC_DIV_DATE), FALSE);
        EnableWindow(GetDlgItem(hDlg, IDC_STATIC_DIV_PLACE), FALSE);
        EnableWindow(GetDlgItem(hDlg, IDC_STATIC_DIV_TYPE), FALSE);

        for (ev = f->event; ev != NULL; ev = ev->next)
        {
            if (ev->type == EV_MARRIAGE)
            {
                CheckDlgButton(hDlg, IDC_CHECK_MARR, BST_CHECKED);
                EnableWindow(GetDlgItem(hDlg, IDC_EDIT_MARR_DATE), TRUE);
                EnableWindow(GetDlgItem(hDlg, IDC_EDIT_MARR_PLACE), TRUE);
                EnableWindow(GetDlgItem(hDlg, IDC_EDIT_MARR_TYPE), TRUE);
                EnableWindow(GetDlgItem(hDlg, IDC_STATIC_MARR_DATE), TRUE);
                EnableWindow(GetDlgItem(hDlg, IDC_STATIC_MARR_PLACE), TRUE);
                EnableWindow(GetDlgItem(hDlg, IDC_STATIC_MARR_TYPE), TRUE);
                SetDlgItemText(hDlg, IDC_EDIT_MARR_DATE, ev->date);
                SetDlgItemText(hDlg, IDC_EDIT_MARR_PLACE, ev->place);
                SetDlgItemText(hDlg, IDC_EDIT_MARR_TYPE, ev->mtype);
            }
            else if (ev->type == EV_DIVORCE)
            {
                CheckDlgButton(hDlg, IDC_CHECK_DIV, BST_CHECKED);
                EnableWindow(GetDlgItem(hDlg, IDC_EDIT_DIV_DATE), TRUE);
                EnableWindow(GetDlgItem(hDlg, IDC_EDIT_DIV_PLACE), TRUE);
                EnableWindow(GetDlgItem(hDlg, IDC_EDIT_DIV_TYPE), TRUE);
                EnableWindow(GetDlgItem(hDlg, IDC_STATIC_DIV_DATE), TRUE);
                EnableWindow(GetDlgItem(hDlg, IDC_STATIC_DIV_PLACE), TRUE);
                EnableWindow(GetDlgItem(hDlg, IDC_STATIC_DIV_TYPE), TRUE);
                SetDlgItemText(hDlg, IDC_EDIT_DIV_DATE, ev->date);
                SetDlgItemText(hDlg, IDC_EDIT_DIV_PLACE, ev->place);
                SetDlgItemText(hDlg, IDC_EDIT_DIV_TYPE, ev->mtype);
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
            nt = GetDlgItemText(hDlg, IDC_EDIT_MARR_TYPE, mtype, MAXSTR);
            if (nd != 0 || np != 0 || nt != 0 || IsDlgButtonChecked(hDlg, IDC_CHECK_MARR))
            {
                // If there is data for a marriage, or the box is ticked, add/edit an event record. Otherwise, remove it.
                ev = find_event(EV_MARRIAGE, &f->event);
                strcpy_s(ev->date, MAXSTR, date);
                strcpy_s(ev->place, MAXSTR, place);
                strcpy_s(ev->mtype, MAXSTR, mtype);
                f->lildate = parse_date_lilian(ev->date, FALSE);
            }
            else
            {
                remove_event(EV_MARRIAGE, &f->event);
                f->lildate = 0;
            }

            nd = GetDlgItemText(hDlg, IDC_EDIT_DIV_DATE, date, MAXSTR);
            np = GetDlgItemText(hDlg, IDC_EDIT_DIV_PLACE, place, MAXSTR);
            nt = GetDlgItemText(hDlg, IDC_EDIT_DIV_TYPE, mtype, MAXSTR);
            if (nd != 0 && np != 0 || nt != 0 || IsDlgButtonChecked(hDlg, IDC_CHECK_DIV))
            {
                ev = find_event(EV_DIVORCE, &f->event);
                strcpy_s(ev->date, MAXSTR, date);
                strcpy_s(ev->place, MAXSTR, place);
                strcpy_s(ev->mtype, MAXSTR, mtype);
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
            note_ptr = &f->notes;
        family_notes_dlg:
            cmd = DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_NOTES), hDlg, notes_dialog, (LPARAM)note_ptr);
            switch (cmd)
            {
            case IDOK:
            case IDCANCEL:
                break;

            case ID_NOTES_DELETE:
                note_ptr = remove_note(*note_ptr, &f->notes);
                goto family_notes_dlg;
                break;

            case ID_NOTES_NEXT:
                note_ptr = &(*note_ptr)->next;
                goto family_notes_dlg;
            }
            break;

        case ID_PERSON_ATTACHMENTS:
            att_ptr = &f->attach;
        family_att_dlg:
            cmd = DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_ATTACHMENTS), hDlg, attachments_dialog, (LPARAM)att_ptr);
            switch (cmd)
            {
            case IDOK:
            case IDCANCEL:
                break;

            case ID_ATT_DELETE:
                att_ptr = remove_attachment(*att_ptr, &f->attach);
                goto family_att_dlg;

            case ID_ATT_NEXT:
                att_ptr = &(*att_ptr)->next;
                goto family_att_dlg;
            }
            break;

        case IDC_CHECK_MARR:
            checked = IsDlgButtonChecked(hDlg, IDC_CHECK_MARR);
            EnableWindow(GetDlgItem(hDlg, IDC_EDIT_MARR_DATE), checked);
            EnableWindow(GetDlgItem(hDlg, IDC_EDIT_MARR_PLACE), checked);
            EnableWindow(GetDlgItem(hDlg, IDC_EDIT_MARR_TYPE), checked);
            EnableWindow(GetDlgItem(hDlg, IDC_STATIC_MARR_DATE), checked);
            EnableWindow(GetDlgItem(hDlg, IDC_STATIC_MARR_PLACE), checked);
            EnableWindow(GetDlgItem(hDlg, IDC_STATIC_MARR_TYPE), checked);
            break;

        case IDC_CHECK_DIV:
            checked = IsDlgButtonChecked(hDlg, IDC_CHECK_DIV);
            EnableWindow(GetDlgItem(hDlg, IDC_EDIT_DIV_DATE), checked);
            EnableWindow(GetDlgItem(hDlg, IDC_EDIT_DIV_PLACE), checked);
            EnableWindow(GetDlgItem(hDlg, IDC_EDIT_DIV_TYPE), checked);
            EnableWindow(GetDlgItem(hDlg, IDC_STATIC_DIV_DATE), checked);
            EnableWindow(GetDlgItem(hDlg, IDC_STATIC_DIV_PLACE), checked);
            EnableWindow(GetDlgItem(hDlg, IDC_STATIC_DIV_TYPE), checked);
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
        if (*np != NULL)
            SetDlgItemText(hDlg, IDC_NOTES_TEXT, (*np)->note);
        else
            EnableWindow(GetDlgItem(hDlg, ID_NOTES_NEXT), FALSE);
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            // Store the note text at the given pointer or attach a new note to the tail.
            if (*np == NULL)
                *np = calloc(1, sizeof(Note));
            GetDlgItemText(hDlg, IDC_NOTES_TEXT, (*np)->note, MAX_NOTESIZE);
            // fall through
        case IDCANCEL:
        case ID_NOTES_NEXT:
        case ID_NOTES_DELETE:
            // These are handled outside
            EndDialog(hDlg, LOWORD(wParam));
            return 1;
        }
        break;
    }
    return 0;
}

LRESULT CALLBACK attachments_dialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    static Attachment **ap;
    char att_filename[MAXSTR];
    static BOOL filename_entered;
    OPENFILENAME ofn;

    switch (message)
    {
    case WM_INITDIALOG:
        ap = (Attachment **)lParam;
        if (*ap != NULL)
        {
            SetDlgItemText(hDlg, IDC_ATT_TITLE, (*ap)->title);
            SetDlgItemText(hDlg, IDC_ATT_FILENAME, (*ap)->filename);
        }
        else
        {
            EnableWindow(GetDlgItem(hDlg, ID_ATT_NEXT), FALSE);
        }
        filename_entered = FALSE;
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            // Store the attachment at the given pointer or attach a new one to the tail.
            if (*ap == NULL)
                *ap = calloc(1, sizeof(Attachment));

            GetDlgItemText(hDlg, IDC_ATT_TITLE, (*ap)->title, MAXSTR);

            // If a new filename has been entered from another directory, copy it into
            // the attachements directory. Point the attachment entry at this new copy.
            if (filename_entered)
            {
                char *slosh;
                char att_dir[MAXSTR];

                GetDlgItemText(hDlg, IDC_ATT_FILENAME, att_filename, MAXSTR);
                strcpy_s(att_dir, MAXSTR, att_filename);
                slosh = strrchr(att_dir, '\\');
                *(slosh + 1) = '\0';

                // check if dir of att_filename is the same as attach_dir
                if (strcmp(att_dir, attach_dir) == 0)
                {
                    strcpy_s((*ap)->filename, MAXSTR, att_filename);
                }
                else
                {
                    char *slosh = strrchr(att_filename, '\\');

                    // Copy the file to attach_dir. Make sure it exists first.
                    strcpy_s((*ap)->filename, MAXSTR, attach_dir);
                    strcat_s((*ap)->filename, MAXSTR, slosh + 1);
                    CreateDirectory(attach_dir, NULL);
                    CopyFile(att_filename, (*ap)->filename, FALSE);
                }
            }

            // fall through
        case IDCANCEL:
        case ID_ATT_NEXT:
        case ID_ATT_DELETE:
            // These are handled outside
            EndDialog(hDlg, LOWORD(wParam));
            return 1;

        case ID_ATT_VIEW:
            ShellExecute(hDlg, NULL, (*ap)->filename, NULL, NULL, SW_SHOWNORMAL);
            break;

        case IDC_ATT_FILENAME:
            if (HIWORD(wParam) == EN_CHANGE)
                filename_entered = TRUE;
            break;

        case ID_ATT_BROWSE:
            att_filename[0] = '\0';
            memset(&ofn, 0, sizeof(OPENFILENAME));
            ofn.lStructSize = sizeof(OPENFILENAME);
            ofn.hwndOwner = hDlg;
            ofn.lpstrFilter = "All Files\0*.*\0\0";
            ofn.nFilterIndex = 1;
            ofn.lpstrDefExt = "";
            ofn.lpstrFile = att_filename;
            ofn.nMaxFile = 256;
            ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST;
            if (GetOpenFileName(&ofn))
            {
                SetDlgItemText(hDlg, IDC_ATT_FILENAME, att_filename);
                filename_entered = TRUE;
            }
            break;
        }
        break;
    }
    return 0;
}

// Load prefs settings to the dialog, and save them from the dialog.
void load_prefs(HWND hDlg, ViewPrefs *prefs)
{
    char buf[MAXSTR];
    int i, indx, root_index;

    SetDlgItemText(hDlg, IDC_PREFS_TITLE, prefs->title);
    SetDlgItemInt(hDlg, IDC_PREFS_DESCLIMIT, prefs->desc_limit, FALSE);
    SetDlgItemInt(hDlg, IDC_PREFS_ANCLIMIT, prefs->anc_limit, FALSE);
    root_index = 0;
    for (i = 1; i <= n_person; i++)
    {
        Person *p = lookup_person[i];
        Event *ev;

        if (p != NULL)
        {
            ev = find_event(EV_BIRTH, &p->event);
            sprintf_s(buf, MAXSTR, "%d: %s %s (%s)", p->id, p->given, p->surname, ev->date);
            indx = SendDlgItemMessage(hDlg, IDC_COMBO_PERSONS, CB_ADDSTRING, 0, (LPARAM)buf);
            if (p == prefs->root_person)
                root_index = indx;
        }
    }
    SendDlgItemMessage(hDlg, IDC_COMBO_PERSONS, CB_SETCURSEL, root_index, 0);
    SetDlgItemInt(hDlg, IDC_PREFS_ZOOM, prefs->zoom_percent, FALSE);
    CheckDlgButton(hDlg, IDC_PREFS_VIEW_DESC, prefs->view_desc ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_PREFS_VIEW_ANC, prefs->view_anc ? BST_CHECKED : BST_UNCHECKED);

    SetDlgItemText(hDlg, IDC_STATIC_DEVICENAME, prefs->dm_devicename);
    SetDlgItemText(hDlg, IDC_STATIC_FORMNAME, prefs->dm_formname);
    SetDlgItemText(hDlg, IDC_STATIC_ORIENTATION, prefs->dm_orientation == DMORIENT_PORTRAIT ? "Portrait" : "Landscape");
    CheckDlgButton(hDlg, IDC_PREFS_STRIPPING, prefs->stripping ? BST_CHECKED : BST_UNCHECKED);
    SetDlgItemInt(hDlg, IDC_PREFS_STRIPHEIGHT, prefs->strip_height, FALSE);
}

void save_prefs(HWND hDlg, ViewPrefs *prefs)
{
    char buf[MAXSTR];
    int i, indx;

    GetDlgItemText(hDlg, IDC_PREFS_TITLE, prefs->title, MAXSTR);
    prefs->desc_limit = GetDlgItemInt(hDlg, IDC_PREFS_DESCLIMIT, NULL, FALSE);
    prefs->anc_limit = GetDlgItemInt(hDlg, IDC_PREFS_ANCLIMIT, NULL, FALSE);
    indx = SendDlgItemMessage(hDlg, IDC_COMBO_PERSONS, CB_GETCURSEL, 0, 0);
    SendDlgItemMessage(hDlg, IDC_COMBO_PERSONS, CB_GETLBTEXT, indx, (LPARAM)buf);
    i = atoi(buf);
    prefs->root_person = lookup_person[i];
    prefs->zoom_percent = GetDlgItemInt(hDlg, IDC_PREFS_ZOOM, NULL, FALSE);
    prefs->view_desc = IsDlgButtonChecked(hDlg, IDC_PREFS_VIEW_DESC);
    prefs->view_anc = IsDlgButtonChecked(hDlg, IDC_PREFS_VIEW_ANC);
    prefs->stripping = IsDlgButtonChecked(hDlg, IDC_PREFS_STRIPPING);
    prefs->strip_height = GetDlgItemInt(hDlg, IDC_PREFS_STRIPHEIGHT, NULL, FALSE);
}

LRESULT CALLBACK prefs_dialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    int i;
    static int view_index;
    char buf[MAXSTR];
    static BOOL text_changed;

    switch (message)
    {
    case WM_INITDIALOG:
        view_index = 0;
        for (i = 0; i < n_views; i++)
        {
            ViewPrefs *vp = &view_prefs[i];

            SendDlgItemMessage(hDlg, IDC_COMBO_VIEW, CB_INSERTSTRING, i, (LPARAM)vp->title);
            if (prefs == vp)
                view_index = i;
        }
        SendDlgItemMessage(hDlg, IDC_COMBO_VIEW, CB_SETCURSEL, view_index, 0);
        SetDlgItemInt(hDlg, IDC_STATIC_VIEW_INDEX, view_index, FALSE);
        load_prefs(hDlg, prefs);
        text_changed = FALSE;
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            save_prefs(hDlg, prefs);
            // fall through
        case IDCANCEL:
            EndDialog(hDlg, LOWORD(wParam));
            return 1;

        case IDC_PREFS_DELETE:
            if (n_views == 1)   // deleteing last view, just reset it all to defaults
            {
                view_index = 0;  // must be already 0
                *prefs = default_prefs;
                SendDlgItemMessage(hDlg, IDC_COMBO_VIEW, CB_RESETCONTENT, 0, 0);
            }
            else
            {
                SendDlgItemMessage(hDlg, IDC_COMBO_VIEW, CB_DELETESTRING, view_index, 0);
                for (i = view_index + 1; i < n_views; i++)  // shuffle rest down
                    view_prefs[i - 1] = view_prefs[i];
                n_views--;
                if (view_index == n_views)
                {
                    view_index--;
                    prefs = &view_prefs[view_index];
                }
                SendDlgItemMessage(hDlg, IDC_COMBO_VIEW, CB_SETCURSEL, view_index, 0);
            }
            load_prefs(hDlg, prefs);
            break;

        case IDC_COMBO_VIEW:
            switch (HIWORD(wParam))
            {
            case CBN_SELCHANGE:
                save_prefs(hDlg, prefs);
                view_index = SendDlgItemMessage(hDlg, IDC_COMBO_VIEW, CB_GETCURSEL, 0, 0);
                SetDlgItemInt(hDlg, IDC_STATIC_VIEW_INDEX, view_index, FALSE);
                prefs = &view_prefs[view_index];
                load_prefs(hDlg, prefs);
                break;

            case CBN_EDITCHANGE:
                text_changed = TRUE;
                break;

            case CBN_KILLFOCUS:
                if (text_changed && n_views < MAX_PREFS - 1)
                {
                    text_changed = FALSE;
                    SendDlgItemMessage(hDlg, IDC_COMBO_VIEW, WM_GETTEXT, MAXSTR, (LPARAM)buf);
                    if (prefs->title[0] != '\0')  // just overwrite if file has no views in it, otherwise new
                    {
                        view_index = n_views;
                        prefs = &view_prefs[n_views++];
                        *prefs = default_prefs;
                    }
                    strcpy_s(prefs->title, MAXSTR, buf);
                    SendDlgItemMessage(hDlg, IDC_COMBO_VIEW, CB_ADDSTRING, 0, (LPARAM)prefs->title);
                    SetDlgItemInt(hDlg, IDC_STATIC_VIEW_INDEX, view_index, FALSE);
                    load_prefs(hDlg, prefs);
                }
                break;
            }
            break;
        }
        break;
    }
    return 0;
}

// Message handler for about box.
LRESULT CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    char buf[MAXSTR];

    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        sprintf_s(buf, MAXSTR, "Family Business, Version 1.0, Build %s", __DATE__);
        SetDlgItemText(hDlg, IDC_STATIC_VERSION, buf);
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


