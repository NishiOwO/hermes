/* recognize HTML ISO entities */

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define HASHSIZE 101

struct nlist
{
    struct nlist *next;
    char *name;
    unsigned code;
};

static struct nlist *hashtab[HASHSIZE];

struct entity
{
    char *name;
    unsigned code;
} entities[] =
{
    "quot",     34,
    "apos",     39,
    "iexcl",    161,
    "cent",     162,
    "pound",    163,
    "yen",      165,
    "brvbar",   166,
    "sect",     167,
    "copy",     169,
    "laquo",    171,
    "raquo",    187,
    "not",      172,
    "reg",      174,
    "deg",      176,
    "plusmn",   177,
    "sup2",     178,
    "sup3",     179,
    "micro",    181,
    "para",     182,
    "sup1",     185,
    "middot",   183,
    "frac14",   188,
    "frac12",   189,
    "iquest",   191,
    "frac34",   190,
    "AElig",    198,
    "Aacute",   193,
    "Acirc",    194,
    "Agrave",   192,
    "Aring",    197,
    "Atilde",   195,
    "Auml",     196,
    "Ccedil",   199,
    "ETH",      208,
    "Eacute",   201,
    "Ecirc",    202,
    "Egrave",   200,
    "Euml",     203,
    "Iacute",   205,
    "Icirc",    206,
    "Igrave",   204,
    "Iuml",     207,
    "Ntilde",   209,
    "Oacute",   211,
    "Ocirc",    212,
    "Ograve",   210,
    "Oslash",   216,
    "Otilde",   213,
    "Ouml",     214,
    "THORN",    222,
    "Uacute",   218,
    "Ucirc",    219,
    "Ugrave",   217,
    "Uuml",     220,
    "Yacute",   221,
    "aacute",   225,
    "acirc",    226,
    "aelig",    230,
    "agrave",   224,
    "amp",      38,
    "aring",    229,
    "atilde",   227,
    "auml",     228,
    "ccedil",   231,
    "eacute",   233,
    "ecirc",    234,
    "egrave",   232,
    "eth",      240,
    "euml",     235,
    "tagc",    62,
    "gt",       62,
    "iacute",   237,
    "icirc",    238,
    "igrave",   236,
    "iuml",     239,
    "stago",    60,
    "lt",       60,
    "ntilde",   241,
    "oacute",   243,
    "ocirc",    244,
    "ograve",   242,
    "oslash",   248,
    "otilde",   245,
    "ouml",     246,
    "szlig",    223,
    "thorn",    254,
    "uacute",   250,
    "ucirc",    251,
    "ugrave",   249,
    "uuml",     252,
    "yacute",   253,
    "yuml",     255
};

static unsigned hash(char *s)
{
    unsigned hashval;

    for (hashval = 0; *s != '\0'; s++)
        hashval = *s + 31*hashval;

    return hashval % HASHSIZE;
}

static struct nlist *lookup(char *s)
{
    struct nlist *np;

    for (np = hashtab[hash(s)]; np != NULL; np = np->next)
        if (strcmp(s, np->name) == 0)
            return np;
    return NULL;
}

static struct nlist *install(char *name, unsigned code)
{
    struct nlist *np;
    unsigned hashval;

    if ((np = lookup(name)) == NULL)
    {
        np = (struct nlist *)malloc(sizeof(*np));

        if (np == NULL || (np->name = strdup(name)) == NULL)
            return NULL;

        hashval = hash(name);
        np->next = hashtab[hashval];
        hashtab[hashval] = np;
    }

    np->code = code;
    return np;
}

int  entity(char *name, int *len)
{
    int i, c;
    char *p, buf[64];
    struct nlist *np;

    for (i = 2, p = buf; i < 65; ++i)
    {
        c = *name++;

        if (c == ';')
        {
            *p = '\0';
            *len = i;
            np = lookup(buf);

            if (np)
                return np->code;

            break;
        }

        *p++ = c;
    }

    return 0;   /* signifies unknown entity name */
}

void InitEntities(void)
{
    struct entity *ep;
    
    ep = entities;

    for(;;)
    {
        install(ep->name, ep->code);

        if (strcmp(ep->name, "yuml") == 0)
            break;

        ++ep;        
    }
}
