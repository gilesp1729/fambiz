#include "stdafx.h"
#include "Fambiz.h"
#include <stdio.h>

char *months[12] =
{
    "Jan",
    "Feb",
    "Mar",
    "Apr",
    "May",
    "Jun",
    "Jul",
    "Aug",
    "Sep",
    "Oct",
    "Nov",
    "Dec"
};

// Julian day calculation. We store the reduced JDN (JDN-2299159) or Lilian day number, valid from 1582 on.
// See Wikipedia, Julian day. The following formula is given for Y, M (1-12), D (1-31), all divisions integer. No time of day is used.
// JDN = (1461 * (Y + 4800 + (M - 14)/12))/4 +(367 * (M - 2 - 12 * ((M - 14)/12)))/12 - (3 * ((Y + 4900 + (M - 14)/12)/100))/4 + D - 32075
// Lilian day = JDN - 2299159

int julian(int Y, int M, int D)
{
    return (1461 * (Y + 4800 + (M - 14) / 12)) / 4 + (367 * (M - 2 - 12 * ((M - 14) / 12))) / 12 - (3 * ((Y + 4900 + (M - 14) / 12) / 100)) / 4 + D - 32075;
}

int lilian(int Y, int M, int D)
{
    return julian(Y, M, D) - 2299159;
}

// Return a month number (1-12) by matching the first 3 letters of the month name.
int match_month(char *name)
{
    int m;

    for (m = 0; m < 12; m++)
    {
        if (_strnicmp(name, months[m], 3) == 0)
            return m + 1;
    }
    return 0;   // when there is no match
}

// Parse the date using some reasonable guess at its format, and return a Lilian day number for it.
// US formats may be encountered when the month number comes before the day number.
//
// The formats are:
//  1970
//  Mar 1970        (punctuation like / or - is OK too)
//  10 Mar 1970
//  Mar 10, 1970
//  3/1970
//  10/3/1970
//  3/10/1970 (US form of the 10th of March)
//
// Year-first is not supported as it is unlikely to ever be encountered (even though I like it)
// The year must have 4 digits.
// Stuff after the year number is ignored.

int parse_date_lilian(char *input_date, BOOL US_format)
{
    char date[MAXSTR];
    char *ctxt = NULL;
    char *tok1, *tok2, *tok3;
    int n, n2, n3, lil;
    int Y, M = 1, D = 1;     // initialise in case they aren't found

    strcpy_s(date, MAXSTR, input_date);     // copy it so strtok doesn't screw up original
    tok1 = strtok_s(date, " ,./-", &ctxt);
    if (tok1 == NULL)
        return 0;
    if (isalpha(tok1[0]))
    {
        // First token is a month name. Format is Mar 1970 or Mar 10, 1970.
        M = match_month(tok1);
        tok2 = strtok_s(NULL, " ,./-", &ctxt);
        if (tok2 == NULL)
            return 0;
        n = atoi(tok2);
        if (n > 1000)
        {
            // It's a year. Stop here.
            Y = n;
        }
        else
        {
            // It's a day number. Look for the year.
            D = n;
            tok3 = strtok_s(NULL, " ,./-", &ctxt);
            if (tok3 == NULL)
                return 0;
            n2 = atoi(tok3);
            if (n2 < 1000)
                return 0;       // Not a year. Give up and return 0.
            Y = n2;
        }
    }
    else
    {
        n = atoi(tok1);
        if (n > 1000)
        {
            // It's a year on its own. Stop here.
            Y = n;
        }
        else
        {
            tok2 = strtok_s(NULL, " ,./-", &ctxt);
            if (tok2 == NULL)
                return 0;

            // check for month name here
            if (isalpha(tok2[0]))
            {
                M = match_month(tok2);
                D = n;
                tok3 = strtok_s(NULL, " ,./-", &ctxt);
                n3 = atoi(tok3);
                if (n3 < 1000)
                    return 0;       // Not a year. Give up and return 0.
                Y = n3;
            }
            else
            {
                n2 = atoi(tok2);
                if (n2 > 1000)
                {
                    // It's a numeric month and year (3/1970)
                    M = n;
                    Y = n2;
                }
                else
                {
                    // It's a day and month (or month and day if US format) and we expect a year to follow.
                    if (US_format)
                    {
                        M = n;
                        D = n2;
                    }
                    else
                    {
                        D = n;
                        M = n2;
                    }
                    tok3 = strtok_s(NULL, " ,./-", &ctxt);
                    if (tok3 == NULL)
                        return 0;
                    n3 = atoi(tok3);
                    if (n3 < 1000)
                        return 0;       // Not a year. Give up and return 0.
                    Y = n3;
                }
            }
        }
    }

    lil = lilian(Y, M, D);

#ifdef DEBUG_JULIAN
    {
        char buf[MAXSTR];
        sprintf_s(buf, MAXSTR, "%s --> DMY %d %d %d --> %d\r\n", input_date, D, M, Y, lil);
        OutputDebugString(buf);
    }
#endif
    return lil;
}