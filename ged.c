#include "stdafx.h"
#include "Fambiz.h"
#include <stdio.h>

// Files with these extensions are viewble in a web browser.
#define IMAGE_TYPES "jpg png mp4"

// New person and fam structures.

// Create a new person with the given ID. 
Person *new_person_by_id(int id)
{
    Person *p = calloc(1, sizeof(Person));

    p->id = id;
    lookup_person[id] = p;
    if (id > n_person)
        n_person = id;      // Note: ID's start at 1, so n_person is the last one found
    return p;
}

// Create a new person (with the next available ID) but don't link it in yet.
// It must be either registered or simply freed.
Person *new_person(CREATESTATE state)
{
    Person *p = calloc(1, sizeof(Person));

    p->id = n_person + 1;
    p->state = state;
    return p;
}

// Put a new person on the lookup array.
void register_person(Person *p)
{
    lookup_person[p->id] = p;
    p->state = STATE_EXISTING;
    if (p->id > n_person)
        n_person = p->id;
}

// Create a person list struct and attach it to the tail. Return the head.
PersonList *new_personlist(Person *p, PersonList *person_list)
{
    PersonList *pl = calloc(1, sizeof(PersonList));

    pl->p = p;
    if (person_list == NULL)
    {
        pl->next = NULL;
        return pl;
    }
    else
    {
        PersonList *tail;

        // Attach new pl to the tail (ensures file order is kept)
        for (tail = person_list; tail->next != NULL; tail = tail->next)
            ;
        tail->next = pl;
    }
    return person_list;
}

// Family calls analogous to the above person calls.
Family *new_family_by_id(int id)
{
    Family *f = calloc(1, sizeof(Family));

    f->id = id;
    lookup_family[id] = f;
    if (id > n_family)
        n_family = id;      // Note: ID's start at 1, so n_family is the last one found
    return f;
}

Family *new_family(void)
{
    Family *f = calloc(1, sizeof(Family));

    f->id = n_family + 1;
    f->state = STATE_NEW_FAMILY;
    return f;
}

void register_family(Family *f)
{
    lookup_family[f->id] = f;
    f->state = STATE_EXISTING;
    if (f->id > n_family)
        n_family = f->id;
}

FamilyList *new_familylist(Family *f, FamilyList *family_list)
{
    FamilyList *fl = calloc(1, sizeof(FamilyList));

    fl->f = f;
    if (family_list == NULL)
    {
        fl->next = NULL;
        return fl;
    }
    else
    {
        FamilyList *tail;

        // Attach new pl to the tail (ensures file order is kept)
        for (tail = family_list; tail->next != NULL; tail = tail->next)
            ;
        tail->next = fl;
    }
    return family_list;
}

// Create a new event and add it to the tail of the given list.
Event *new_event(EVENT type, Event **event_list)
{
    Event *ev = calloc(1, sizeof(Event));
    Event *tail;

    ev->type = type;
    ev->next = NULL;
    if (*event_list == NULL)
    {
        *event_list = ev;
    }
    else
    {
        for (tail = *event_list; tail->next != NULL; tail = tail->next)
            ;
        tail->next = ev;
    }
    return ev;
}

// Create a new note and add it to the tail of the given list.
Note *new_note(Note **note_list)
{
    Note *n = calloc(1, sizeof(Note));
    Note *tail;

    n->next = NULL;
    if (*note_list == NULL)
    {
        *note_list = n;
    }
    else
    {
        for (tail = *note_list; tail->next != NULL; tail = tail->next)
            ;
        tail->next = n;
    }
    return n;
}

// Create a new attachment and add it to the tail of the given list.
Attachment *new_attachment(Attachment **att_list)
{
    Attachment *a = calloc(1, sizeof(Attachment));
    Attachment *tail;

    a->next = NULL;
    if (*att_list == NULL)
    {
        *att_list = a;
    }
    else
    {
        for (tail = *att_list; tail->next != NULL; tail = tail->next)
            ;
        tail->next = a;
    }
    return a;
}

// Find an existing person (family, etc) and if not found, create it and add it to the lookup arrays.
Person *find_person(int id)
{
    Person *p = lookup_person[id];

    if (p != NULL)
        return p;
    else
        return new_person_by_id(id);
}

Family *find_family(int id)
{
    Family *f = lookup_family[id];

    if (f != NULL)
        return f;
    else
        return new_family_by_id(id);
}

// Find an event in the list, or create one and add it to the tail of the list.
// Return the event found or created, and possibly modify event_list (for the first one)
Event *find_event(EVENT type, Event **event_list)
{
    Event *ev;

    for (ev = *event_list; ev != NULL; ev = ev->next)
    {
        if (ev->type == type)
            return ev;
    }
    return new_event(type, event_list);
}

// Remove an event of the given type from the list, if it exists. 
// It's OK if it doesn't.
void remove_event(EVENT type, Event **event_list)
{
    Event *ev;
    Event *prev = NULL;

    for (ev = *event_list; ev != NULL; prev = ev, ev = ev->next)
    {
        if (ev->type == type)
        {
            if (prev == NULL)   // first in list
                *event_list = ev->next;
            else
                prev->next = ev->next;

            free(ev);
            return;
        }
    }
}

// Remove a note from the list, if it exists. Return a pointer to the "next" pointer that points to 
// the next note after the one deleted. If note doesn't exist returns NULL.
Note **remove_note(Note *note, Note **note_list)
{
    Note *n;
    Note *prev = NULL;

    for (n = *note_list; n != NULL; prev = n, n = n->next)
    {
        if (n == note)
        {
            if (prev == NULL)   // first in list
            {
                *note_list = n->next;
                free(n);
                return note_list;
            }
            else
            {
                prev->next = n->next;
                free(n);
                return &prev->next;
            }
        }
    }
    return NULL;
}

// Remove ann attachment from the list, if it exists. Return a pointer to the "next" pointer that points to 
// the next one after the one deleted. If attachment doesn't exist returns NULL.
Attachment **remove_attachment(Attachment *att, Attachment **att_list)
{
    Attachment *a;
    Attachment *prev = NULL;

    for (a = *att_list; a != NULL; prev = a, a = a->next)
    {
        if (a == att)
        {
            if (prev == NULL)   // first in list
            {
                *att_list = a->next;
                free(a);
                return att_list;
            }
            else
            {
                prev->next = a->next;
                free(a);
                return &prev->next;
            }
        }
    }
    return NULL;
}

// Remove a person from a personlist. ASSERT if it's not there.
void remove_personlist(Person *p, PersonList **person_list)
{
    PersonList *pl;
    PersonList *prev = NULL;

    for (pl = *person_list; pl != NULL; prev = pl, pl = pl->next)
    {
        if (pl->p == p)
        {
            if (prev == NULL)   // first in list
                *person_list = pl->next;
            else
                prev->next = pl->next;

            free(pl);
            return;
        }
    }
    ASSERT(FALSE, "Person not found in list");
}

// Remove a family from a familylist. ASSERT if it's not there.
void remove_familylist(Family *f, FamilyList **family_list)
{
    FamilyList *fl;
    FamilyList *prev = NULL;

    for (fl = *family_list; fl != NULL; prev = fl, fl = fl->next)
    {
        if (fl->f == f)
        {
            if (prev == NULL)   // first in list
                *family_list = fl->next;
            else
                prev->next = fl->next;

            free(fl);
            return;
        }
    }
    ASSERT(FALSE, "Family not found in list");
}

// Skip forward in a GEDCOM file to the next record at a given level.
// We assume we've just read a whole line. On return, the level number has
// been read in but nothing else. Returns -1 if EOF encountered, or a level
// number equal to or less than the number asked for.
static int
skip_ged(FILE *ged, int level)
{
    char buf[MAX_NOTESIZE];
    int chr;
    
    while (1)
    {
        chr = fgetc(ged);
        if (chr == EOF)
            return -1;
        if (chr - '0' <= level)
            return chr - '0';
        if (fgets(buf, MAX_NOTESIZE, ged) == NULL)
            return -1;
    }
}

// Read and write a GEDCOM file to/from a PersonList tree.
BOOL
read_ged(char *filename)
{
    FILE *ged;
    char buf[MAX_NOTESIZE], *ref, *tag, *slosh, *dot;
    int id;
    char *ctxt = NULL;
    Event *ev;

    // Clear lookup arrays to NULL pointers.
    prefs->root_person = NULL;
    n_person = 0;
    n_family = 0;
    n_views = 0;
    memset(lookup_person, 0, MAX_PERSON * sizeof(Person *));
    memset(lookup_family, 0, MAX_FAMILY * sizeof(Family *));

    // Clear attachment directory (use the first one found, or else the basename set by caller)
    attach_dir[0] = '\0';
    attach_default = FALSE;

    fopen_s(&ged, filename, "rt");
    if (ged == NULL)
        return FALSE;

    fgets(buf, MAX_NOTESIZE, ged);
    tag = strtok_s(buf, "\n", &ctxt);
    if (strstr(tag, "0 HEAD") == NULL)     // Read and skip header. Skip any BOM's in the file (we only read ansi)
        goto eof_error;
    if (skip_ged(ged, 0) < 0)
        goto eof_error;

    while (1)
    {
        fgets(buf, MAX_NOTESIZE, ged);
        ref = strtok_s(buf, " \n", &ctxt);
        if (ref[0] == '@')                      // Digest INDI (0 @Innn@ INDI) or FAM (0 @Fnnn@ FAM) record. 
        {
            id = atoi(&ref[2]);
            tag = strtok_s(NULL, " \n", &ctxt);
            if (strcmp(tag, "INDI") == 0)
            {
                Person *p = find_person(id);
                int i, lev;

                // IF we've seen a _VIEW tag(s), set up the root person when we encounter them.
                for (i = 0; i < n_views; i++)
                {
                    ViewPrefs *vp = &view_prefs[i];

                    if (vp->root_id != 0 && vp->root_id == id)
                        vp->root_person = p;
                }

                skip_ged(ged, 1);       // absorb the first level number
                while (1)
                {
                    fgets(buf, MAX_NOTESIZE, ged);
                    tag = strtok_s(buf, " \n", &ctxt);

                    if (strcmp(tag, "SEX") == 0)
                    {
                        ref = strtok_s(NULL, " \n", &ctxt);
                        if (ref != NULL)
                            strcpy_s(p->sex, 2, ref);
                    }
                    else if (strcmp(tag, "NAME") == 0)
                    {
                        ref = strtok_s(NULL, "/\n", &ctxt);  // Note: can contain spaces. Slashes separate given and surname.
                        if (ref != NULL)
                        {
                            int namelen;

                            strcpy_s(p->given, MAXSTR, ref);
                            namelen = strlen(p->given) - 1;
                            if (p->given[namelen] == ' ')
                                p->given[namelen] = '\0';   // strip trailing space

                            // Strip slashes from name. 
                            ref = strtok_s(NULL, "/\n", &ctxt);
                            if (ref != NULL)
                                strcpy_s(p->surname, MAXSTR, ref);
                        }
                    }
                    else if (strcmp(tag, "OCCU") == 0)
                    {
                        ref = strtok_s(NULL, "\n", &ctxt);  // Note: can contain spaces
                        if (ref != NULL)
                            strcpy_s(p->occupation, MAXSTR, ref);
                    }
                    else if (strcmp(tag, "_HIDDEN") == 0)
                    {
                        p->hidden = TRUE;
                    }
                    else if (strcmp(tag, "DEAT") == 0)
                    {
                        ev = new_event(EV_DEATH, &p->event);
                        goto i_event_common;              // Yucky goto but saves a lot of mucking about.
                    }
                    else if (strcmp(tag, "BURI") == 0)
                    {
                        ev = new_event(EV_BURIAL, &p->event);
                        goto i_event_common; 
                    }
                    else if (strcmp(tag, "BIRT") == 0)
                    {
                        ev = new_event(EV_BIRTH, &p->event);
                    i_event_common:
                        lev = skip_ged(ged, 2);
                        if (lev == 2)       // handle events with no level-2 stuff
                        {
                            while (1)
                            {
                                fgets(buf, MAX_NOTESIZE, ged);
                                tag = strtok_s(buf, " \n", &ctxt);
                                if (strcmp(tag, "DATE") == 0)
                                {
                                    ref = strtok_s(NULL, "\n", &ctxt);  // Note: can contain spaces
                                    if (ref != NULL)
                                        strcpy_s(ev->date, MAXSTR, ref);
                                }
                                else if (strcmp(tag, "PLAC") == 0)
                                {
                                    ref = strtok_s(NULL, "\n", &ctxt);
                                    if (ref != NULL)
                                        strcpy_s(ev->place, MAXSTR, ref);
                                }
                                else if (strcmp(tag, "CAUS") == 0)
                                {
                                    ref = strtok_s(NULL, "\n", &ctxt);
                                    if (ref != NULL)
                                        strcpy_s(ev->cause, MAXSTR, ref);
                                }
                                lev = skip_ged(ged, 2);
                                if (lev < 2)
                                    break;
                            }
                        }
                        if (lev == 1)            // don't skip again at level 1
                            continue;
                        else if (lev < 1)
                            break;
                    }
                    else if (strcmp(tag, "NOTE") == 0)
                    {
                        Note *n = new_note(&p->notes);

                        ref = strtok_s(NULL, "\n", &ctxt);  // Note: can contain spaces, quotes and XML tags. TODO: strip these
                        if (ref != NULL)
                            strcpy_s(n->note, MAX_NOTESIZE, ref);

                        lev = skip_ged(ged, 2);
                        if (lev == 2)       // handle notes with no level-2 stuff
                        {
                            while (1)
                            {
                                fgets(buf, MAX_NOTESIZE, ged);
                                tag = strtok_s(buf, " \n", &ctxt);
                                if (strcmp(tag, "CONT") == 0)
                                {
                                    strcat_s(n->note, MAX_NOTESIZE, "\r\n");   // Use CRLF inside notes for the edit control
                                    ref = strtok_s(NULL, "\n", &ctxt);
                                    if (ref != NULL)
                                        strcat_s(n->note, MAX_NOTESIZE, ref);
                                }
                                else if (strcmp(tag, "CONC") == 0)
                                {
                                    ref = strtok_s(NULL, "\n", &ctxt);
                                    if (ref != NULL)
                                        strcat_s(n->note, MAX_NOTESIZE, ref);
                                }

                                lev = skip_ged(ged, 2);
                                if (lev < 2)
                                    break;
                            }
                        }
                        if (lev == 1)            // don't skip again at level 1
                            continue;
                        else if (lev < 1)
                            break;
                    }
                    else if (strcmp(tag, "OBJE") == 0)
                    {
                        Attachment *a = new_attachment(&p->attach);

                        lev = skip_ged(ged, 2);
                        if (lev == 2)
                        {
                            while (1)
                            {
                                fgets(buf, MAXSTR, ged);
                                tag = strtok_s(buf, " \n", &ctxt);
                                if (strcmp(tag, "TITL") == 0)
                                {
                                    ref = strtok_s(NULL, "\n", &ctxt);
                                    if (ref != NULL)
                                        strcpy_s(a->title, MAXSTR, ref);
                                }
                                else if (strcmp(tag, "FILE") == 0)
                                {
                                    ref = strtok_s(NULL, "\n", &ctxt);
                                    if (ref != NULL)
                                        strcpy_s(a->filename, MAXSTR, ref);

                                    if (attach_dir[0] == '\0')
                                    {
                                        // If there is a directory in the filename, use that. But if not,
                                        // we must assume the basename.
                                        slosh = strrchr(a->filename, '\\');
                                        if (slosh != NULL)
                                        {
                                            strcpy_s(attach_dir, MAXSTR, a->filename);
                                            slosh = strrchr(attach_dir, '\\');
                                            *(slosh + 1) = '\0';
                                        }
                                        else
                                        {
                                            strcpy_s(attach_dir, MAXSTR, filename);
                                            dot = strrchr(attach_dir, '.');
                                            *dot = '\\';
                                            *(dot + 1) = '\0';
                                            attach_default = TRUE;
                                        }
                                    }

                                    if (attach_default)
                                    {
                                        char basename[MAXSTR];

                                        // Prepend the attachment directory to the filename.
                                        strcpy_s(basename, MAXSTR, a->filename);
                                        strcpy_s(a->filename, MAXSTR, attach_dir);
                                        strcat_s(a->filename, MAXSTR, basename);
                                    }

                                    dot = strrchr(a->filename, '.');
                                    a->is_image = strstr(IMAGE_TYPES, dot + 1) != NULL;
                                }

                                lev = skip_ged(ged, 2);
                                if (lev < 2)
                                    break;
                            }
                        }
                        if (lev == 1)            // don't skip again at level 1
                            continue;
                        else if (lev < 1)
                            break;
                    }

#if 0 // Do these linkages when processing the families.
                    else if (strcmp(tag, "FAMS") == 0)
                    {
                    }
                    else if (strcmp(tag, "FAMC") == 0)
                    {
                    }
#endif                    
                    // todo BURI, BAPM, OCCU, etc. that can occur at level 1.
                    if (skip_ged(ged, 1) < 1)  
                        break;
                }
            }
            else if (strcmp(tag, "FAM") == 0)   // The families come after the persons they refer to. Is this always true?
            {
                Family *f = find_family(id);
                Person *p;
                int lev;

                skip_ged(ged, 1);       // absorb the first level number
                while (1)
                {
                    fgets(buf, MAX_NOTESIZE, ged);
                    tag = strtok_s(buf, " \n", &ctxt);

                    if (strcmp(tag, "HUSB") == 0)
                    {
                        ref = strtok_s(NULL, "\n", &ctxt);
                        id = atoi(&ref[2]);
                        p = find_person(id);
                        f->husband = p;
                        p->spouses = new_familylist(f, p->spouses);
                    }
                    else if (strcmp(tag, "WIFE") == 0)
                    {
                        ref = strtok_s(NULL, "\n", &ctxt);
                        id = atoi(&ref[2]);
                        p = find_person(id);
                        f->wife = p;
                        p->spouses = new_familylist(f, p->spouses);
                    }
                    else if (strcmp(tag, "CHIL") == 0)
                    {
                        ref = strtok_s(NULL, "\n", &ctxt);
                        id = atoi(&ref[2]);
                        p = find_person(id);
                        f->children = new_personlist(p, f->children);
                        p->family = f;
                    }
                    else if (strcmp(tag, "MARR") == 0)
                    {
                        ev = new_event(EV_MARRIAGE, &f->event);
                        goto f_event_common;
                    }
                    else if (strcmp(tag, "DIV") == 0)
                    {
                        ev = new_event(EV_DIVORCE, &f->event);
                    f_event_common:
                        lev = skip_ged(ged, 2);
                        if (lev == 2)       // handle events with no level-2 stuff
                        {
                            while (1)
                            {
                                fgets(buf, MAX_NOTESIZE, ged);
                                tag = strtok_s(buf, " \n", &ctxt);
                                if (strcmp(tag, "DATE") == 0)
                                {
                                    ref = strtok_s(NULL, "\n", &ctxt);  // Note: can contain spaces
                                    if (ref != NULL)
                                        strcpy_s(ev->date, MAXSTR, ref);
                                }
                                else if (strcmp(tag, "PLAC") == 0)
                                {
                                    ref = strtok_s(NULL, "\n", &ctxt);
                                    if (ref != NULL)
                                        strcpy_s(ev->place, MAXSTR, ref);
                                }
                                lev = skip_ged(ged, 2);
                                if (lev < 2)
                                    break;
                            }
                            if (lev == 1)            // don't skip again at level 1
                                continue;
                            else if (lev < 1)
                                break;
                        }
                    }
                    else if (strcmp(tag, "NOTE") == 0)
                    {
                        Note *n = new_note(&f->notes);

                        ref = strtok_s(NULL, "\n", &ctxt);  // Note: can contain spaces,quotes and XML tags. TODO: strip these
                        if (ref != NULL)
                            strcpy_s(n->note, MAX_NOTESIZE, ref);

                        lev = skip_ged(ged, 2);
                        if (lev == 2)       // handle notes with no level-2 stuff
                        {
                            while (1)
                            {
                                fgets(buf, MAX_NOTESIZE, ged);
                                tag = strtok_s(buf, " \n", &ctxt);
                                if (strcmp(tag, "CONT") == 0)
                                {
                                    strcat_s(n->note, MAX_NOTESIZE, "\r\n");   // Use CRLF inside notes for the edit control
                                    ref = strtok_s(NULL, "\n", &ctxt);
                                    if (ref != NULL)
                                        strcat_s(n->note, MAX_NOTESIZE, ref);
                                }
                                else if (strcmp(tag, "CONC") == 0)
                                {
                                    ref = strtok_s(NULL, "\n", &ctxt);
                                    if (ref != NULL)
                                        strcat_s(n->note, MAX_NOTESIZE, ref);
                                }
                            }
                            lev = skip_ged(ged, 2);
                            if (lev < 2)
                                break;
                        }
                        if (lev == 1)            // don't skip again at level 1
                            continue;
                        else if (lev < 1)
                            break;
                    }
                    else if (strcmp(tag, "OBJE") == 0)
                    {
                        Attachment *a = new_attachment(&p->attach);

                        lev = skip_ged(ged, 2);
                        if (lev == 2)
                        {
                            while (1)
                            {
                                fgets(buf, MAXSTR, ged);
                                tag = strtok_s(buf, " \n", &ctxt);
                                if (strcmp(tag, "TITL") == 0)
                                {
                                    ref = strtok_s(NULL, "\n", &ctxt);
                                    if (ref != NULL)
                                        strcpy_s(a->title, MAXSTR, ref);
                                }
                                else if (strcmp(tag, "FILE") == 0)
                                {
                                    ref = strtok_s(NULL, "\n", &ctxt);
                                    if (ref != NULL)
                                        strcpy_s(a->filename, MAXSTR, ref);

                                    if (attach_dir[0] == '\0')
                                    {
                                        // If there is a directory in the filename, use that. But if not,
                                        // we must assume the basename.
                                        slosh = strrchr(a->filename, '\\');
                                        if (slosh != NULL)
                                        {
                                            strcpy_s(attach_dir, MAXSTR, a->filename);
                                            slosh = strrchr(attach_dir, '\\');
                                            *(slosh + 1) = '\0';
                                        }
                                        else
                                        {
                                            strcpy_s(attach_dir, MAXSTR, filename);
                                            dot = strrchr(attach_dir, '.');
                                            *dot = '\\';
                                            *(dot + 1) = '\0';
                                            attach_default = TRUE;
                                        }
                                    }

                                    if (attach_default)
                                    {
                                        char basename[MAXSTR];

                                        // Prepend the attachment directory to the filename.
                                        strcpy_s(basename, MAXSTR, a->filename);
                                        strcpy_s(a->filename, MAXSTR, attach_dir);
                                        strcat_s(a->filename, MAXSTR, basename);
                                    }

                                    dot = strrchr(a->filename, '.');
                                    a->is_image = strstr(IMAGE_TYPES, dot + 1) != NULL;
                                }

                                lev = skip_ged(ged, 2);
                                if (lev < 2)
                                    break;
                            }
                        }
                        if (lev == 1)            // don't skip again at level 1
                            continue;
                        else if (lev < 1)
                            break;
                    }

                    // TODO other family tags
                    if (skip_ged(ged, 1) < 1)
                        break;
                }
            }
        }
        else if (strcmp(ref, "_VIEW") == 0)     // This is a Fambiz file with view parameters, possibly multiple
        {
            ViewPrefs *vp = &view_prefs[n_views++];

            ref = strtok_s(NULL, " \n", &ctxt);
            if (ref != NULL)
                vp->root_id = atoi(ref);
            ref = strtok_s(NULL, " \n", &ctxt);
            if (ref != NULL)
                vp->view_desc = atoi(ref);
            ref = strtok_s(NULL, " \n", &ctxt);
            if (ref != NULL)
                vp->view_anc = atoi(ref);
            ref = strtok_s(NULL, " \n", &ctxt);
            if (ref != NULL)
                vp->desc_limit = atoi(ref);
            ref = strtok_s(NULL, " \n", &ctxt);
            if (ref != NULL)
                vp->anc_limit = atoi(ref);
            ref = strtok_s(NULL, " \n", &ctxt);
            if (ref != NULL)
                vp->zoom_percent = atoi(ref);
            ref = strtok_s(NULL, "\n", &ctxt);    // title may contain spaces
            if (ref != NULL)
                strcpy_s(vp->title, MAXSTR, ref);

            if (skip_ged(ged, 0) < 0)
                break;
        }
        else if (strcmp(ref, "TRLR") == 0)      // Trailer has been read, we're finished.
        {
            break;
        }
        else if (skip_ged(ged, 0) < 0)          // It's unrecognised - skip over it
        {
            goto eof_error;
        }
    }

    fclose(ged);
    return TRUE;

eof_error:
    fclose(ged);
    return FALSE;
}

// Write the tree to a GEDCOM file. Add internal tags to indicate who the root person is,
// what is being viewed, etc. so that a view can be reconstructed on reopening.
BOOL
write_ged(char *filename)
{
    FILE *ged;
    char buf[MAXSTR];
    char *ctxt = NULL;
    char *line;
    Event *ev;
    Note *n;
    Attachment *a;
    int i;

    fopen_s(&ged, filename, "wt");
    if (ged == NULL)
        return FALSE;

    // Write out the header
    GetDateFormat(LOCALE_SYSTEM_DEFAULT, 0, NULL, "dd MMM yyyy", buf, MAX_NOTESIZE);
    fprintf_s(ged, "0 HEAD\n");
    fprintf_s(ged, "1 GEDC\n");
    fprintf_s(ged, "2 VERS 5.5.1\n");
    fprintf_s(ged, "2 FORM LINEAGE-LINKED\n");
    fprintf_s(ged, "1 DATE %s\n", buf);
    fprintf_s(ged, "1 CHAR ASCII\n");
    fprintf_s(ged, "1 LANG English\n");
    fprintf_s(ged, "1 SOUR FAMBIZ\n");
    fprintf_s(ged, "2 NAME Family Business\n");
    fprintf_s(ged, "1 FILE %s\n", filename);

    // Write out the view parameters.
    for (i = 0; i < n_views; i++)
    {
        ViewPrefs *vp = &view_prefs[i];

        fprintf_s(ged, "0 _VIEW %d %d %d %d %d %d %s\n", 
                  vp->root_person->id, 
                  vp->view_desc, vp->view_anc, 
                  vp->desc_limit, prefs->anc_limit, 
                  vp->zoom_percent, vp->title);
    }

    for (i = 0; i <= n_person; i++)
    {
        Person *p = lookup_person[i];
        FamilyList *sl;

        if (p == NULL)
            continue;

        fprintf_s(ged, "0 @I%d@ INDI\n", p->id);

        if (p->given != NULL && p->given[0] != '\0')
        {
            if (p->surname != NULL && p->surname[0] != '\0')
                fprintf_s(ged, "1 NAME %s /%s/\n", p->given, p->surname);
            else
                fprintf_s(ged, "1 NAME %s\n", p->given);
        }
        if (p->sex != NULL && p->sex[0] != '\0')
            fprintf_s(ged, "1 SEX %s\n", p->sex);
        if (p->occupation != NULL && p->occupation[0] != '\0')
            fprintf_s(ged, "1 OCCU %s\n", p->occupation);
        for (ev = p->event; ev != NULL; ev = ev->next)
        {
            fprintf_s(ged, "1 %s\n", codes[ev->type].code);
            if (ev->date != NULL && ev->date[0] != '\0')
                fprintf_s(ged, "2 DATE %s\n", ev->date);
            if (ev->place != NULL && ev->place[0] != '\0')
                fprintf_s(ged, "2 PLAC %s\n", ev->place);
            if (ev->cause != NULL && ev->cause[0] != '\0')
                fprintf_s(ged, "2 CAUS %s\n", ev->cause);
        }

        for (n = p->notes; n != NULL; n = n->next)
        {
            if (n->note != NULL && n->note[0] != '\0')
            {
                // Don't worry about breaking lines with CONC. Other programs handle them just fine.
                line = strtok_s(n->note, "\r\n", &ctxt);   // skip \r\n
                if (line != NULL)
                    fprintf_s(ged, "1 NOTE %s\n", line);
                while (line != NULL)
                {
                    line = strtok_s(NULL, "\r\n", &ctxt);
                    if (line != NULL)
                        fprintf_s(ged, "2 CONT %s\n", line);
                }
            }
        }

        for (a = p->attach; a != NULL; a = a->next)
        {
            if (a->filename != NULL && a->filename[0] != '\0')
            {
                fprintf_s(ged, "1 OBJE\n");
                fprintf_s(ged, "2 TITL %s\n", a->title);
                if (attach_default)
                {
                    char *slosh = strrchr(a->filename, '\\');

                    // Write out filename unqualified.
                    fprintf_s(ged, "2 FILE %s\n", slosh+1);
                }
                else
                {
                    fprintf_s(ged, "2 FILE %s\n", a->filename);
                }
            }
        }

        if (p->family != NULL)
            fprintf_s(ged, "1 FAMC @F%d@\n", p->family->id);
        for (sl = p->spouses; sl != NULL; sl = sl->next)
            fprintf_s(ged, "1 FAMS @F%d@\n", sl->f->id);
        if (p->hidden)
            fprintf_s(ged, "1 _HIDDEN\n");
    }

    for (i = 0; i <= n_family; i++)
    {
        Family *f = lookup_family[i];
        PersonList *cl;

        if (f == NULL)
            continue;

        fprintf_s(ged, "0 @F%d@ FAM\n", f->id);
        if (f->husband != NULL)
            fprintf_s(ged, "1 HUSB @I%d@\n", f->husband->id);
        if (f->wife != NULL)
            fprintf_s(ged, "1 WIFE @I%d@\n", f->wife->id);
        for (cl = f->children; cl != NULL; cl = cl->next)
            fprintf(ged, "1 CHIL @I%d@\n", cl->p->id);
        for (ev = f->event; ev != NULL; ev = ev->next)
        {
            fprintf_s(ged, "1 %s\n", codes[ev->type].code);
            if (ev->date != NULL && ev->date[0] != '\0')
                fprintf_s(ged, "2 DATE %s\n", ev->date);
            if (ev->place != NULL && ev->place[0] != '\0')
                fprintf_s(ged, "2 PLAC %s\n", ev->place);
        }
        for (n = f->notes; n != NULL; n = n->next)
        {
            if (n->note != NULL && n->note[0] != '\0')
            {
                line = strtok_s(n->note, "\n", &ctxt);
                if (line != NULL)
                    fprintf_s(ged, "1 NOTE %s\n", line);
                while (line != NULL)
                {
                    line = strtok_s(NULL, "\n", &ctxt);
                    if (line != NULL)
                        fprintf_s(ged, "2 CONT %s\n", line);
                }
            }
        }
        for (a = f->attach; a != NULL; a = a->next)
        {
            if (a->filename != NULL && a->filename[0] != '\0')
            {
                fprintf_s(ged, "1 OBJE\n");
                fprintf_s(ged, "2 TITL %s\n", a->title);
                if (attach_default)
                {
                    char *slosh = strrchr(a->filename, '\\');

                    // Write out filename unqualified.
                    fprintf_s(ged, "2 FILE %s\n", slosh + 1);
                }
                else
                {
                    fprintf_s(ged, "2 FILE %s\n", a->filename);
                }
            }
        }
    }

    fprintf_s(ged, "0 TRLR\n");
    fclose(ged);

    return TRUE;
}


