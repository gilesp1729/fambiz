#pragma once

#include "resource.h"

#define MAXSTR      256
#define MAX_PERSON  2048
#define MAX_FAMILY  2048
#define MAX_NOTESIZE 4096

// Max generations in total and an offset to the middle of the generation array
// (must be separately defined as needs to be constant)
#define MAX_GEN     30
#define CENTRE_GEN  15

// Max number of view preference settings
#define MAX_PREFS 16

// Extend this to include all the silly little cases in the GEDCOM event
typedef enum
{
    EV_BIRTH,
    EV_DEATH,
    EV_MARRIAGE,
    EV_DIVORCE,
    EV_BURIAL
} EVENT;

// When creating a new person/family, set this state to tell the dialog routine which button
// we have come from.
typedef enum
{
    STATE_EXISTING = 0,             // An existing person/family in the tree.
    STATE_NEW_FAMILY,               // A new family (created with Add Spouse or Add Parent)
    STATE_NEW_CHILD,                // A new child of a family (Add Child...)
    STATE_NEW_SPOUSE,               // A new spouse of a person (Add Spouse...)
    STATE_NEW_PARENT                // A new parent of a person (Add Parent...)
} CREATESTATE;

// Data structures to represent persons and families.

typedef struct Event
{
    EVENT       type;               // What is this (birth, marriage, etc)
    char        date[MAXSTR];       // date and place if known
    char        place[MAXSTR];
    char        cause[MAXSTR];      // cause (of death)
    struct Event *next;             // points to next event in list
} Event;

typedef struct Note
{
    struct Note *next;
    char        note[MAX_NOTESIZE];
} Note;

typedef struct Attachment
{
    struct Attachment *next;
    char        title[MAXSTR];
    char        filename[MAXSTR];   // Fully-qualified while in the program (not always in files)
    BOOL        is_image;           // TRUE if the file is an image (web-browser-natively viewable)
} Attachment;

typedef struct Person
{
    int         id;                 // person ID number
    char        surname[MAXSTR];    // surname, no spaces, cleaned up for display/print
    char        given[MAXSTR];      // given names (may contain spaces)
    char        sex[2];             // "M", "F" and possibly other values
    char        occupation[MAXSTR]; // any occupation given
    Note        *notes;             // List of notes
    Event       *event;             // List of events (birth, death, etc) with dates/places
    Attachment  *attach;            // List of attachments (photos, documents)
    struct Family *family;          // Child to family link (the person is a child of this family)
    struct FamilyList *spouses;     // List of one or more families in which the person is a spouse
    BOOL        hidden;             // descendants/ancestors are hidden (collapsed) in chart
    CREATESTATE state;              // Indicates this person is being created new and is not in the tree yet.

    // Chart-related calculated data
    int         generation;         // 0 = the root, 1,2,3 = descendant generations below root, -1,-2 = ancestors
    int         accum_width;        // width of all children and their descendants
    int         offset;             // offset from LH edge of box containing the maximum accum_width of all generations
    int         xbox;               // final position of box
    int         ybox;
    BOOL        show_link;          // If TRUE, this person is the root of another view (for HTML only) and should be a link
    int         view_link;          // If above TRUE, this is the view number it should link to
} Person;

typedef struct PersonList
{
    Person      *p;                 // Pointers to person and the next in list
    struct PersonList  *next;
} PersonList;

typedef struct Family
{
    int         id;                 // Family ID number
    Event       *event;             // List of events (marriage, divorce, etc) with dates/places
    Note        *notes;             // List of notes
    Attachment  *attach;            // List of attachments (photos, documents)
    Person      *husband;           // Parents
    Person      *wife;
    PersonList  *children;          // List of children
    CREATESTATE state;              // Indicates this family is being created new and is not in the tree yet.
} Family;

typedef struct FamilyList
{
    Family      *f;                 // Pointers to family and the next in list
    int         family_max_offset;  // While building chart, the max offset of all children to the left
    struct FamilyList  *next;
} FamilyList;

// These are the strings that appear in a GEDCOM file, along with event types that 
// correspond (if applicable) and the display strings for the charts.
typedef struct Code
{
    char        code[10];
    EVENT       event_type;
    char        display[64];
} Code;

// A structure to represent a view.
typedef struct ViewPrefs
{
    char        title[MAXSTR];      // Title of view
    Person      *root_person;       // Root person of view
    int         root_id;            // ID of root person (used only when reading in the GED file)
    BOOL        view_desc;          // TRUE to view descendants
    BOOL        view_anc;           // TRUE to view ancestors
    int         desc_limit;         // How many descendant generations to view (0 = no limit)
    int         anc_limit;          // How many ancestor gens similary
    int         zoom_percent;       // The zoom percentage of the view
}  ViewPrefs;

// Externals
extern HINSTANCE hInst;
extern char curr_filename[];
extern char attach_dir[];
extern BOOL attach_default;
extern Person *lookup_person[];
extern Family *lookup_family[];
extern int n_person;
extern int n_family;
extern Code codes[];
extern ViewPrefs *prefs;
extern ViewPrefs default_prefs;
extern ViewPrefs view_prefs[];
extern int n_views;


// Macros
#define MAX(a, b)   ((a) > (b) ? (a) : (b))

#define STRICT_ASSERTS
#ifdef STRICT_ASSERTS
#define ASSERT(exp, msg)    do { if (!(exp)) { OutputDebugString(msg); DebugBreak(); } } while(0)
#else
#define ASSERT(exp, msg)    do { if (!(exp)) OutputDebugString(msg) } while(0)
#endif


// chart.c
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);

// dialogs.c
LRESULT CALLBACK    person_dialog(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK    family_dialog(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK    notes_dialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK    attachments_dialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK    prefs_dialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK	About(HWND, UINT, WPARAM, LPARAM);

// ged.c
BOOL read_ged(char *filename);
BOOL write_ged(char *filename);
Person *new_person_by_id(int id);
Person *new_person(CREATESTATE state);
void register_person(Person *p);
PersonList *new_personlist(Person *p, PersonList *person_list);
Family *new_family_by_id(int id);
Family *new_family(void);
void register_family(Family *f);
FamilyList *new_familylist(Family *f, FamilyList *family_list);
Event *new_event(EVENT type, Event **event_list);
Note *new_note(Note **note_list);
Attachment *new_attachment(Attachment **att_list);
Person *find_person(int id);
Family *find_family(int id);
Event *find_event(EVENT type, Event **event_list);
void remove_event(EVENT type, Event **event_list);
Note **remove_note(Note *note, Note **note_list);
Attachment **remove_attachment(Attachment *att, Attachment **att_list);
void remove_personlist(Person *p, PersonList **person_list);
void remove_familylist(Family *f, FamilyList **family_list);
