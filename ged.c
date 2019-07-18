#include "stdafx.h"
#include "Fambiz.h"
#include <stdio.h>

// New person and fam structures.
Person *new_person(int id)
{
    Person *p = calloc(1, sizeof(Person));

    p->id = id;
    lookup_person[id] = p;
    if (id > n_person)
        n_person = id;      // Note: ID's start at 1, so n_person is the last one found
    return p;
}

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

Family *new_family(int id)
{
    Family *f = calloc(1, sizeof(Family));

    f->id = id;
    lookup_family[id] = f;
    if (id > n_family)
        n_family = id;      // Note: ID's start at 1, so n_family is the last one found
    return f;
}

FamilyList *new_familylist(Family *f, FamilyList *family_list)
{
    FamilyList *fl = calloc(1, sizeof(FamilyList));

    fl->f = f;
    fl->next = family_list;
    return fl;
}

Event *new_event(EVENT type, Event *event_list)
{
    Event *ev = calloc(1, sizeof(Event));

    ev->type = type;
    ev->next = event_list;
    return ev;
}

// Find an existing person (family, etc) and if not found, create it.
Person *find_person(int id)
{
    Person *p = lookup_person[id];

    if (p != NULL)
        return p;
    else
        return new_person(id);
}

Family *find_family(int id)
{
    Family *f = lookup_family[id];

    if (f != NULL)
        return f;
    else
        return new_family(id);
}

// Skip forward in a GEDCOM file to the next record at a given level.
// We assume we've just read a whole line. On return, the level number has
// been read in but nothing else. Returns -1 if EOF encountered, or a level
// number equal to or less than the number asked for.
static int
skip_ged(FILE *ged, int level)
{
    char buf[MAXSTR];
    int chr;
    
    while (1)
    {
        chr = fgetc(ged);
        if (chr == EOF)
            return -1;
        if (chr - '0' <= level)
            return chr - '0';
        if (fgets(buf, MAXSTR, ged) == NULL)
            return -1;
    }
}

// Read and write a GEDCOM file to/from a PersonList tree.
BOOL
read_ged(char *filename)
{
    FILE *ged;
    char buf[MAXSTR], *ref, *tag;
    int id;
    char *ctxt = NULL;

    // Clear lookup arrays to NULL pointers.
    n_person = 0;
    n_family = 0;
    memset(lookup_person, 0, MAX_PERSON * sizeof(Person *));
    memset(lookup_family, 0, MAX_FAMILY * sizeof(Family *));

    fopen_s(&ged, filename, "rt");
    if (ged == NULL)
        return FALSE;

    fgets(buf, MAXSTR, ged);
    tag = strtok_s(buf, "\n", &ctxt);
    if (strstr(tag, "0 HEAD") == NULL)     // Read and skip header. Skip any BOM's in the file (we only read ansi)
        goto eof_error;
    if (skip_ged(ged, 0) < 0)
        goto eof_error;

    while (1)
    {
        fgets(buf, MAXSTR, ged);
        ref = strtok_s(buf, " \n", &ctxt);
        if (ref[0] == '@')                      // Digest INDI (0 @Innn@ INDI) or FAM (0 @Fnnn@ FAM) record. 
        {
            id = atoi(&ref[2]);
            tag = strtok_s(NULL, " \n", &ctxt);
            if (strcmp(tag, "INDI") == 0)
            {
                Person *p = find_person(id);
                int lev;

                skip_ged(ged, 1);       // absorb the first level number
                while (1)
                {
                    fgets(buf, MAXSTR, ged);
                    tag = strtok_s(buf, " \n", &ctxt);

                    if (strcmp(tag, "SEX") == 0)
                    {
                        ref = strtok_s(NULL, " \n", &ctxt);
                        if (ref != NULL)
                            strcpy_s(p->sex, 2, ref);
                    }
                    else if (strcmp(tag, "NAME") == 0)
                    {
                        int i, namelen;

                        ref = strtok_s(NULL, "\n", &ctxt);  // Note: can contain spaces
                        if (ref != NULL)
                        {
                            strcpy_s(p->rawname, MAXSTR, ref);
                            strcpy_s(p->name, MAXSTR, ref);

                            // Strip slashes from name. They convey no useful information.
                            namelen = strlen(p->name);
                            for (i = 0; i < namelen; i++)
                            {
                                if (p->name[i] == '/')
                                    p->name[i] = ' ';
                            }
                            while (p->name[--i] == ' ')     // Strip resulting trailing spaces.
                                ;
                            p->name[i + 1] = '\0';
                        }
                    }
                    else if (strcmp(tag, "DEAT") == 0)
                    {
                        p->event = new_event(EV_DEATH, p->event);
                        goto i_event_common;              // Yucky goto but saves a lot of mucking about.
                    }
                    else if (strcmp(tag, "BIRT") == 0)
                    {
                        p->event = new_event(EV_BIRTH, p->event);
                    i_event_common:
                        if (skip_ged(ged, 2) < 2)       // handle events with no level-2 stuff
                            break;
                        while (1)
                        {
                            fgets(buf, MAXSTR, ged);
                            tag = strtok_s(buf, " \n", &ctxt);
                            if (strcmp(tag, "DATE") == 0)
                            {
                                ref = strtok_s(NULL, "\n", &ctxt);  // Note: can contain spaces
                                if (ref != NULL)
                                    strcpy_s(p->event->date, MAXSTR, ref);
                            }
                            else if (strcmp(tag, "PLAC") == 0)
                            {
                                ref = strtok_s(NULL, "\n", &ctxt);
                                if (ref != NULL)
                                    strcpy_s(p->event->place, MAXSTR, ref);
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
                    fgets(buf, MAXSTR, ged);
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
                        f->event = new_event(EV_MARRIAGE, f->event);
                        goto f_event_common;
                    }
                    else if (strcmp(tag, "DIV") == 0)
                    {
                        f->event = new_event(EV_DIVORCE, f->event);
                    f_event_common:
                        if (skip_ged(ged, 2) < 2)       // handle events with no level-2 stuff
                            break;
                        while (1)
                        {
                            fgets(buf, MAXSTR, ged);
                            tag = strtok_s(buf, " \n", &ctxt);
                            if (strcmp(tag, "DATE") == 0)
                            {
                                ref = strtok_s(NULL, "\n", &ctxt);  // Note: can contain spaces
                                if (ref != NULL)
                                    strcpy_s(f->event->date, MAXSTR, ref);
                            }
                            else if (strcmp(tag, "PLAC") == 0)
                            {
                                ref = strtok_s(NULL, "\n", &ctxt);
                                if (ref != NULL)
                                    strcpy_s(f->event->place, MAXSTR, ref);
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
                    // TODO other family tags
                    if (skip_ged(ged, 1) < 1)
                        break;
                }
            }
        }
        else if (strcmp(ref, "TRLR") == 0)      // Trailer has been read, we're finished.
            break;
        else if (skip_ged(ged, 0) < 0)          // It's unrecognised - skip over it
            goto eof_error;
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
    Event *ev;
    int i;

    fopen_s(&ged, filename, "wt");
    if (ged == NULL)
        return FALSE;

    GetDateFormat(LOCALE_SYSTEM_DEFAULT, 0, NULL, "dd MMM yyyy", buf, MAXSTR);
    fprintf_s(ged, "0 HEAD\n");
    fprintf_s(ged, "1 GEDC\n");
    fprintf_s(ged, "2 VERS 5.5.1\n");
    fprintf_s(ged, "2 FORM LINEAGE-LINKED\n");
    fprintf_s(ged, "1 DATE %s\n", buf);
    fprintf_s(ged, "1 CHAR ANSI\n");
    fprintf_s(ged, "1 LANG English\n");
    fprintf_s(ged, "1 SOUR FAMBIZ\n");
    fprintf_s(ged, "2 NAME Family Business\n");
    fprintf_s(ged, "1 FILE %s\n", filename);

    for (i = 0; i <= n_person; i++)
    {
        Person *p = lookup_person[i];
        FamilyList *sl;

        if (p == NULL)
            continue;

        fprintf_s(ged, "0 @I%d@ INDI\n", p->id);
        if (p->rawname != NULL && p->rawname[0] != '\0')
            fprintf_s(ged, "1 NAME %s\n", p->rawname);      // TODO build this from new entries with slashes round surname
        if (p->sex != NULL && p->sex[0] != '\0')
            fprintf_s(ged, "1 SEX %s\n", p->sex);
        for (ev = p->event; ev != NULL; ev = ev->next)
        {
            fprintf_s(ged, "1 %s\n", codes[ev->type].code);
            if (ev->date != NULL && ev->date[0] != '\0')
                fprintf_s(ged, "2 DATE %s\n", ev->date);
            if (ev->place != NULL && ev->place[0] != '\0')
                fprintf_s(ged, "2 PLAC %s\n", ev->place);
        }
        if (p->family != NULL)
            fprintf_s(ged, "1 FAMC @F%d@\n", p->family->id);
        for (sl = p->spouses; sl != NULL; sl = sl->next)
            fprintf_s(ged, "1 FAMS @F%d@\n", sl->f->id);
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
    }

    fprintf_s(ged, "0 TRLR\n");
    fclose(ged);

    return TRUE;
}


