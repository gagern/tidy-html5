/*
 tidy.c - HTML TidyLib command line driver

 Copyright (c) 1998-2008 World Wide Web Consortium
 (Massachusetts Institute of Technology, European Research
 Consortium for Informatics and Mathematics, Keio University).
 All Rights Reserved.

 */

#include "tidy.h"
#include "language.h"
#if !defined(NDEBUG) && defined(_MSC_VER)
#include "sprtf.h"
#endif

#ifndef SPRTF
#define SPRTF printf
#endif

static FILE* errout = NULL;  /* set to stderr */
/* static FILE* txtout = NULL; */  /* set to stdout */

/**
 **  Indicates whether or not two filenames are the same.
 */
static Bool samefile( ctmbstr filename1, ctmbstr filename2 )
{
#if FILENAMES_CASE_SENSITIVE
    return ( strcmp( filename1, filename2 ) == 0 );
#else
    return ( strcasecmp( filename1, filename2 ) == 0 );
#endif
}


/**
 **  Exits with an error in the event of an out of memory condition.
 */
static void outOfMemory(void)
{
    fprintf(stderr,"%s", tidyLocalizedString(TC_STRING_OUT_OF_MEMORY));
    exit(1);
}


/**
 **  Used by `print2Columns` and `print3Columns` to manage whitespace.
 */
static const char *cutToWhiteSpace(const char *s, uint offset, char *sbuf)
{
    if (!s)
    {
        sbuf[0] = '\0';
        return NULL;
    }
    else if (strlen(s) <= offset)
    {
        strcpy(sbuf,s);
        sbuf[offset] = '\0';
        return NULL;
    }
    else
    {
		uint j, l, n;
		/* scan forward looking for newline */
		j = 0;
		while(j < offset && s[j] != '\n')
			++j;
		if ( j == offset ) {
			/* scan backward looking for first space */
			j = offset;
			while(j && s[j] != ' ')
				--j;
			l = j;
			n = j+1;
			/* no white space */
			if (j==0)
			{
				l = offset;
				n = offset;
			}
		} else
		{
			l = j;
			n = j+1;
		}
		strncpy(sbuf,s,l);
		sbuf[l] = '\0';
		return s+n;
    }
}

/**
 **  Outputs one column of text.
 */
static void print1Column( const char* fmt, uint l1, const char *c1 )
{
	const char *pc1=c1;
	char *c1buf = (char *)malloc(l1+1);
	if (!c1buf) outOfMemory();
	
	do
	{
		pc1 = cutToWhiteSpace(pc1, l1, c1buf);
		printf(fmt, c1buf[0] !='\0' ? c1buf : "");
	} while (pc1);
	free(c1buf);
}

/**
 **  Outputs two columns of text.
 */
static void print2Columns( const char* fmt, uint l1, uint l2,
                          const char *c1, const char *c2 )
{
    const char *pc1=c1, *pc2=c2;
    char *c1buf = (char *)malloc(l1+1);
    char *c2buf = (char *)malloc(l2+1);
    if (!c1buf) outOfMemory();
    if (!c2buf) outOfMemory();

    do
    {
        pc1 = cutToWhiteSpace(pc1, l1, c1buf);
        pc2 = cutToWhiteSpace(pc2, l2, c2buf);
        printf(fmt,
               c1buf[0]!='\0'?c1buf:"",
               c2buf[0]!='\0'?c2buf:"");
    } while (pc1 || pc2);
    free(c1buf);
    free(c2buf);
}

/**
 **  Outputs three columns of text.
 */
static void print3Columns( const char* fmt, uint l1, uint l2, uint l3,
                          const char *c1, const char *c2, const char *c3 )
{
    const char *pc1=c1, *pc2=c2, *pc3=c3;
    char *c1buf = (char *)malloc(l1+1);
    char *c2buf = (char *)malloc(l2+1);
    char *c3buf = (char *)malloc(l3+1);
    if (!c1buf) outOfMemory();
    if (!c2buf) outOfMemory();
    if (!c3buf) outOfMemory();

    do
    {
        pc1 = cutToWhiteSpace(pc1, l1, c1buf);
        pc2 = cutToWhiteSpace(pc2, l2, c2buf);
        pc3 = cutToWhiteSpace(pc3, l3, c3buf);
        printf(fmt,
               c1buf[0]!='\0'?c1buf:"",
               c2buf[0]!='\0'?c2buf:"",
               c3buf[0]!='\0'?c3buf:"");
    } while (pc1 || pc2 || pc3);
    free(c1buf);
    free(c2buf);
    free(c3buf);
}

/**
 **  Format strings and decorations used in output.
 */
//static const char helpfmt[] = " %-19.19s %-58.58s\n";
static const char helpfmt[] = " %-25.25s %-52.52s\n";
static const char helpul[]  = "-----------------------------------------------------------------";
static const char fmt[]     = "%-27.27s %-9.9s  %-40.40s\n";
static const char valfmt[]  = "%-27.27s %-9.9s %-1.1s%-39.39s\n";
static const char ul[]      = "=================================================================";

/**
 **  This enum is used to categorize the options for help output.
 */
typedef enum
{
    CmdOptFileManip,
    CmdOptCatFIRST = CmdOptFileManip,
    CmdOptProcDir,
    CmdOptCharEnc,
    CmdOptMisc,
    CmdOptCatLAST
} CmdOptCategory;

/**
 **  This array contains headings that will be used in help ouput.
 */
static const struct {
    ctmbstr mnemonic;  /**< Used in XML as a class. */
    uint key;          /**< Key to fetch the localized string. */
} cmdopt_catname[] = {
    { "file-manip", TC_STRING_FILE_MANIP },
    { "process-directives", TC_STRING_PROCESS_DIRECTIVES },
    { "char-encoding", TC_STRING_CHAR_ENCODING },
    { "misc", TC_STRING_MISC }
};

/**
 **  The struct and subsequent array keep the help output structured
 **  because we _also_ output all of this stuff as as XML.
 */
typedef struct {
    CmdOptCategory cat; /**< Category */
    ctmbstr name1;      /**< Name */
    uint key;           /**< Key to fetch the localized description. */
	uint subKey;        /**< Secondary substitution key. */
    ctmbstr eqconfig;   /**< Equivalent configuration option */
    ctmbstr name2;      /**< Name */
    ctmbstr name3;      /**< Name */
} CmdOptDesc;

/* All instances of %s will be substituted with localized string
   specified by the subKey field. */
static const CmdOptDesc cmdopt_defs[] =  {
    { CmdOptFileManip, "-output <%s>",     TC_OPT_OUTPUT,   TC_LABEL_FILE, "output-file: <%s>", "-o <%s>" },
    { CmdOptFileManip, "-config <%s>",     TC_OPT_CONFIG,   TC_LABEL_FILE, NULL },
    { CmdOptFileManip, "-file <%s>",       TC_OPT_FILE,     TC_LABEL_FILE, "error-file: <%s>", "-f <%s>" },
    { CmdOptFileManip, "-modify",          TC_OPT_MODIFY,   0,             "write-back: yes", "-m" },
    { CmdOptProcDir,   "-indent",          TC_OPT_INDENT,   0,             "indent: auto", "-i" },
    { CmdOptProcDir,   "-wrap <%s>",       TC_OPT_WRAP,     TC_LABEL_COL,  "wrap: <%s>", "-w <%s>" },
    { CmdOptProcDir,   "-upper",           TC_OPT_UPPER,    0,             "uppercase-tags: yes", "-u" },
    { CmdOptProcDir,   "-clean",           TC_OPT_CLEAN,    0,             "clean: yes", "-c" },
    { CmdOptProcDir,   "-bare",            TC_OPT_BARE,     0,             "bare: yes", "-b" },
    { CmdOptProcDir,   "-gdoc",            TC_OPT_GDOC,     0,             "gdoc: yes", "-g" },
    { CmdOptProcDir,   "-numeric",         TC_OPT_NUMERIC,  0,             "numeric-entities: yes", "-n" },
    { CmdOptProcDir,   "-errors",          TC_OPT_ERRORS,   0,             "markup: no", "-e" },
    { CmdOptProcDir,   "-quiet",           TC_OPT_QUIET,    0,             "quiet: yes", "-q" },
    { CmdOptProcDir,   "-omit",            TC_OPT_OMIT,     0,             "omit-optional-tags: yes" },
    { CmdOptProcDir,   "-xml",             TC_OPT_XML,      0,             "input-xml: yes" },
    { CmdOptProcDir,   "-asxml",           TC_OPT_ASXML,    0,             "output-xhtml: yes", "-asxhtml" },
    { CmdOptProcDir,   "-ashtml",          TC_OPT_ASHTML,   0,             "output-html: yes" },
#if SUPPORT_ACCESSIBILITY_CHECKS
    { CmdOptProcDir,   "-access <%s>",     TC_OPT_ACCESS,   TC_LABEL_LEVL, "accessibility-check: <%s>" },
#endif
    { CmdOptCharEnc,   "-raw",             TC_OPT_RAW,      0,             NULL },
    { CmdOptCharEnc,   "-ascii",           TC_OPT_ASCII,    0,             NULL },
    { CmdOptCharEnc,   "-latin0",          TC_OPT_LATIN0,   0,             NULL },
    { CmdOptCharEnc,   "-latin1",          TC_OPT_LATIN1,   0,             NULL },
#ifndef NO_NATIVE_ISO2022_SUPPORT
    { CmdOptCharEnc,   "-iso2022",         TC_OPT_ISO2022,  0,             NULL },
#endif
    { CmdOptCharEnc,   "-utf8",            TC_OPT_UTF8,     0,             NULL },
    { CmdOptCharEnc,   "-mac",             TC_OPT_MAC,      0,             NULL },
    { CmdOptCharEnc,   "-win1252",         TC_OPT_WIN1252,  0,             NULL },
    { CmdOptCharEnc,   "-ibm858",          TC_OPT_IBM858,   0,             NULL },
#if SUPPORT_UTF16_ENCODINGS
    { CmdOptCharEnc,   "-utf16le",         TC_OPT_UTF16LE,  0,             NULL },
    { CmdOptCharEnc,   "-utf16be",         TC_OPT_UTF16BE,  0,             NULL },
    { CmdOptCharEnc,   "-utf16",           TC_OPT_UTF16,    0,             NULL },
#endif
#if SUPPORT_ASIAN_ENCODINGS /* #431953 - RJ */
    { CmdOptCharEnc,   "-big5",            TC_OPT_BIG5,     0,             NULL },
    { CmdOptCharEnc,   "-shiftjis",        TC_OPT_SHIFTJIS, 0,             NULL },
    { CmdOptCharEnc,   "-language <%s>",   TC_OPT_LANGUAGE, TC_LABEL_LANG, "language: <%s>" },
#endif
    { CmdOptMisc,      "-version",         TC_OPT_VERSION,  0,             NULL,  "-v" },
    { CmdOptMisc,      "-help",            TC_OPT_HELP,     0,             NULL,  "-h", "-?" },
    { CmdOptMisc,      "-xml-help",        TC_OPT_XMLHELP,  0,             NULL },
    { CmdOptMisc,      "-help-config",     TC_OPT_HELPCFG,  0,             NULL },
    { CmdOptMisc,      "-xml-config",      TC_OPT_XMLCFG,   0,             NULL },
    { CmdOptMisc,      "-show-config",     TC_OPT_SHOWCFG,  0,             NULL },
	{ CmdOptMisc,      "-help-option <%s>",TC_OPT_HELPOPT,  TC_LABEL_OPT,  NULL },
    { CmdOptMisc,      NULL,               0,               0,             NULL }
};

/**
**  Create a new string with a format and arguments.
*/
static ctmbstr stringWithFormat( const ctmbstr fmt, ... )
{
    va_list argList = {};
    char *result = NULL;
    int len = 0;

    va_start(argList, fmt);
    len = vsnprintf( result, 0, fmt, argList );
    va_end(argList);

    if (!(result = malloc((len + 1) * sizeof(char))))
        outOfMemory();

    va_start(argList, fmt);
    len = vsnprintf( result, len + 1, fmt, argList);
    va_end(argList);

    return result;
}

/**
**  Option names aren't localized, but the  sample fields
**  are, for example <file> should be <archivo> in Spanish.
*/
static void localize_option_names( CmdOptDesc *pos)
{
    ctmbstr fileString = tidyLocalizedString(pos->subKey);
    pos->name1 = stringWithFormat(pos->name1, fileString);
    if ( pos->name2 )
        pos->name2 = stringWithFormat(pos->name2, fileString);
    if ( pos->name3 )
        pos->name3 = stringWithFormat(pos->name3, fileString);
    if ( pos->eqconfig )
        pos->eqconfig = stringWithFormat(pos->eqconfig, fileString);
}

/**
**  Retrieve the options' names from the structure as a single
**  string.
*/
static tmbstr get_option_names( const CmdOptDesc* pos )
{
    tmbstr name;
    uint len;
    CmdOptDesc localPos = *pos;

    localize_option_names( &localPos );

    len = strlen(localPos.name1);
    if (localPos.name2)
        len += 2+strlen(localPos.name2);
    if (localPos.name3)
        len += 2+strlen(localPos.name3);

    name = (tmbstr)malloc(len+1);
    if (!name) outOfMemory();
    strcpy(name, localPos.name1);
    if (localPos.name2)
    {
        strcat(name, ", ");
        strcat(name, localPos.name2);
    }
    if (localPos.name3)
    {
        strcat(name, ", ");
        strcat(name, localPos.name3);
    }
    return name;
}

/**
**  Escape a name for XML output.
*/
static tmbstr get_escaped_name( ctmbstr name )
{
    tmbstr escpName;
    char aux[2];
    uint len = 0;
    ctmbstr c;
    for(c=name; *c!='\0'; ++c)
        switch(*c)
    {
        case '<':
        case '>':
            len += 4;
            break;
        case '"':
            len += 6;
            break;
        default:
            len += 1;
            break;
    }

    escpName = (tmbstr)malloc(len+1);
    if (!escpName) outOfMemory();
    escpName[0] = '\0';

    aux[1] = '\0';
    for(c=name; *c!='\0'; ++c)
        switch(*c)
    {
        case '<':
            strcat(escpName, "&lt;");
            break;
        case '>':
            strcat(escpName, "&gt;");
            break;
        case '"':
            strcat(escpName, "&quot;");
            break;
        default:
            aux[0] = *c;
            strcat(escpName, aux);
            break;
    }

    return escpName;
}

/**
**  Outputs a complete help option (text)
*/
static void print_help_option( void )
{
    CmdOptCategory cat = CmdOptCatFIRST;
    const CmdOptDesc* pos = cmdopt_defs;

    for( cat=CmdOptCatFIRST; cat!=CmdOptCatLAST; ++cat)
    {
        ctmbstr name = tidyLocalizedString(cmdopt_catname[cat].key);
        size_t len =  strlen(name);
        printf("%s\n", name );
        printf("%*.*s\n", (int)len, (int)len, helpul );
        for( pos=cmdopt_defs; pos->name1; ++pos)
        {
            tmbstr name;
            if (pos->cat != cat)
                continue;
            name = get_option_names( pos );
            print2Columns( helpfmt, 25, 52, name, tidyLocalizedString( pos->key ) );
            free(name);
        }
        printf("\n");
    }
}

/**
**  Outputs an XML element for an option.
*/
static void print_xml_help_option_element( ctmbstr element, ctmbstr name )
{
    tmbstr escpName;
    if (!name)
        return;
    printf("  <%s>%s</%s>\n", element, escpName = get_escaped_name(name),
           element);
    free(escpName);
}

/**
**  Outputs a complete help option (XML)
*/
static void print_xml_help_option( void )
{
    const CmdOptDesc* pos = cmdopt_defs;

    for( pos=cmdopt_defs; pos->name1; ++pos)
    {
        printf(" <option class=\"%s\">\n", cmdopt_catname[pos->cat].mnemonic );
        print_xml_help_option_element("name", pos->name1);
        print_xml_help_option_element("name", pos->name2);
        print_xml_help_option_element("name", pos->name3);
        print_xml_help_option_element("description", tidyLocalizedString( pos->key ) );
        if (pos->eqconfig)
            print_xml_help_option_element("eqconfig", pos->eqconfig);
        else
            printf("  <eqconfig />\n");
        printf(" </option>\n");
    }
}

/**
**  Provides the -xml-help service.
*/
static void xml_help( void )
{
    printf( "<?xml version=\"1.0\"?>\n"
           "<cmdline version=\"%s\">\n", tidyLibraryVersion());
    print_xml_help_option();
    printf( "</cmdline>\n" );
}

/**
**  Returns the final name of the tidy executable.
*/
static ctmbstr get_final_name( ctmbstr prog )
{
    ctmbstr name = prog;
    int c;
    size_t i, len = strlen(prog);
    for (i = 0; i < len; i++) {
        c = prog[i];
        if ((( c == '/' ) || ( c == '\\' )) && prog[i+1])
            name = &prog[i+1];
    }
    return name;
}

/**
**  Handles the -help service.
*/
static void help( ctmbstr prog )
{
	printf( tidyLocalizedString(TC_TXT_HELP_1), get_final_name(prog),tidyLibraryVersion() );

#ifdef PLATFORM_NAME
	printf( tidyLocalizedString(TC_TXT_HELP_2A), PLATFORM_NAME );
#else
	printf( tidyLocalizedString(TC_TXT_HELP_2B) );
#endif
    printf( "\n");

    print_help_option();

	printf( "%s", tidyLocalizedString(TC_TXT_HELP_3) );
}

/**
**  Utility to determine if an option is an AutoBool.
*/
static Bool isAutoBool( TidyOption topt )
{
    TidyIterator pos;
    ctmbstr def;

    if ( tidyOptGetType( topt ) != TidyInteger)
        return no;

    pos = tidyOptGetPickList( topt );
    while ( pos )
    {
        def = tidyOptGetNextPick( topt, &pos );
        if (0==strcmp(def,"yes"))
            return yes;
    }
    return no;
}

/**
**  Returns the configuration category name for the
**  specified configuration category id. This will be
**  used as an XML class attribute value.
*/
static ctmbstr ConfigCategoryName( TidyConfigCategory id )
{
    switch( id )
    {
        case TidyMarkup:
            return tidyLocalizedString( TC_CAT_MARKUP );
        case TidyDiagnostics:
			return tidyLocalizedString( TC_CAT_DIAGNOSTICS );
        case TidyPrettyPrint:
			return tidyLocalizedString( TC_CAT_PRETTYPRINT );
        case TidyEncoding:
			return tidyLocalizedString( TC_CAT_ENCODING );
        case TidyMiscellaneous:
			return tidyLocalizedString( TC_CAT_MISC );
    }
    fprintf(stderr, tidyLocalizedString(TC_STRING_FATAL_ERROR), (int)id);
    assert(0);
    abort();
    return "never_here"; /* only for the compiler warning */
}

/**
** Structure maintain a description of an option.
*/
typedef struct {
    ctmbstr name;  /**< Name */
    ctmbstr cat;   /**< Category */
    ctmbstr type;  /**< "String, ... */
    ctmbstr vals;  /**< Potential values. If NULL, use an external function */
    ctmbstr def;   /**< default */
    tmbchar tempdefs[80]; /**< storage for default such as integer */
    Bool haveVals; /**< if yes, vals is valid */
} OptionDesc;

typedef void (*OptionFunc)( TidyDoc, TidyOption, OptionDesc * );


/**
** Create OptionDesc "d" related to "opt" 
*/
static
void GetOption( TidyDoc tdoc, TidyOption topt, OptionDesc *d )
{
    TidyOptionId optId = tidyOptGetId( topt );
    TidyOptionType optTyp = tidyOptGetType( topt );

    d->name = tidyOptGetName( topt );
    d->cat = ConfigCategoryName( tidyOptGetCategory( topt ) );
    d->vals = NULL;
    d->def = NULL;
    d->haveVals = yes;

    /* Handle special cases first.
     */
    switch ( optId )
    {
        case TidyDuplicateAttrs:
        case TidySortAttributes:
        case TidyNewline:
        case TidyAccessibilityCheckLevel:
            d->type = "enum";
            d->vals = NULL;
            d->def =
            optId==TidyNewline ?
            "<em>Platform dependent</em>"
            :tidyOptGetCurrPick( tdoc, optId );
            break;

        case TidyDoctype:
            d->type = "DocType";
            d->vals = NULL;
        {
            ctmbstr sdef = NULL;
            sdef = tidyOptGetCurrPick( tdoc, TidyDoctypeMode );
            if ( !sdef || *sdef == '*' )
                sdef = tidyOptGetValue( tdoc, TidyDoctype );
            d->def = sdef;
        }
            break;

        case TidyInlineTags:
        case TidyBlockTags:
        case TidyEmptyTags:
        case TidyPreTags:
            d->type = "Tag names";
            d->vals = "tagX, tagY, ...";
            d->def = NULL;
            break;

        case TidyCharEncoding:
        case TidyInCharEncoding:
        case TidyOutCharEncoding:
            d->type = "Encoding";
            d->def = tidyOptGetEncName( tdoc, optId );
            if (!d->def)
                d->def = "?";
            d->vals = NULL;
            break;

            /* General case will handle remaining */
        default:
            switch ( optTyp )
        {
            case TidyBoolean:
                d->type = "Boolean";
                d->vals = "y/n, yes/no, t/f, true/false, 1/0";
                d->def = tidyOptGetCurrPick( tdoc, optId );
                break;

            case TidyInteger:
                if (isAutoBool(topt))
                {
                    d->type = "AutoBool";
                    d->vals = "auto, y/n, yes/no, t/f, true/false, 1/0";
                    d->def = tidyOptGetCurrPick( tdoc, optId );
                }
                else
                {
                    uint idef;
                    d->type = "Integer";
                    if ( optId == TidyWrapLen )
                        d->vals = "0 (no wrapping), 1, 2, ...";
                    else
                        d->vals = "0, 1, 2, ...";

                    idef = tidyOptGetInt( tdoc, optId );
                    sprintf(d->tempdefs, "%u", idef);
                    d->def = d->tempdefs;
                }
                break;

            case TidyString:
                d->type = "String";
                d->vals = NULL;
                d->haveVals = no;
                d->def = tidyOptGetValue( tdoc, optId );
                break;
        }
    }
}

/**
** Array holding all options. Contains a trailing sentinel. 
*/
typedef struct {
    TidyOption topt[N_TIDY_OPTIONS];
} AllOption_t;

/**
**  A simple option comparator.
**/
static int cmpOpt(const void* e1_, const void *e2_)
{
    const TidyOption* e1 = (const TidyOption*)e1_;
    const TidyOption* e2 = (const TidyOption*)e2_;
    return strcmp(tidyOptGetName(*e1), tidyOptGetName(*e2));
}

/**
**  Returns options sorted.
**/
static void getSortedOption( TidyDoc tdoc, AllOption_t *tOption )
{
    TidyIterator pos = tidyGetOptionList( tdoc );
    uint i = 0;

    while ( pos )
    {
        TidyOption topt = tidyGetNextOption( tdoc, &pos );
        tOption->topt[i] = topt;
        ++i;
    }
    tOption->topt[i] = NULL; /* sentinel */

    qsort(tOption->topt,
          /* Do not sort the sentinel: hence `-1' */
          sizeof(tOption->topt)/sizeof(tOption->topt[0])-1,
          sizeof(tOption->topt[0]),
          cmpOpt);
}

/**
**  An iterator for the sorted options.
**/
static void ForEachSortedOption( TidyDoc tdoc, OptionFunc OptionPrint )
{
    AllOption_t tOption;
    const TidyOption *topt;

    getSortedOption( tdoc, &tOption );
    for( topt = tOption.topt; *topt; ++topt)
    {
        OptionDesc d;

        GetOption( tdoc, *topt, &d );
        (*OptionPrint)( tdoc, *topt, &d );
    }
}

/**
**  An iterator for the unsorted options.
**/
static void ForEachOption( TidyDoc tdoc, OptionFunc OptionPrint )
{
    TidyIterator pos = tidyGetOptionList( tdoc );

    while ( pos )
    {
        TidyOption topt = tidyGetNextOption( tdoc, &pos );
        OptionDesc d;

        GetOption( tdoc, topt, &d );
        (*OptionPrint)( tdoc, topt, &d );
    }
}

/**
**  Prints an option's allowed value as specified in its pick list.
**/
static void PrintAllowedValuesFromPick( TidyOption topt )
{
    TidyIterator pos = tidyOptGetPickList( topt );
    Bool first = yes;
    ctmbstr def;
    while ( pos )
    {
        if (first)
            first = no;
        else
            printf(", ");
        def = tidyOptGetNextPick( topt, &pos );
        printf("%s", def);
    }
}

/**
**  Prints an option's allowed values.
**/
static void PrintAllowedValues( TidyOption topt, const OptionDesc *d )
{
    if (d->vals)
        printf( "%s", d->vals );
    else
        PrintAllowedValuesFromPick( topt );
}

/**
**  Prints for XML an option's <description>.
**/
static void printXMLDescription( TidyDoc tdoc, TidyOption topt )
{
    ctmbstr doc = tidyOptGetDoc( tdoc, topt );

    if (doc)
        printf("  <description>%s</description>\n", doc);
    else
    {
        printf("  <description />\n");
        fprintf(stderr, "Warning: option `%s' is not documented.\n",
                tidyOptGetName( topt ));
    }
}

/**
**  Prints for XML an option's <seealso>.
**/
static void printXMLCrossRef( TidyDoc tdoc, TidyOption topt )
{
    TidyOption optLinked;
    TidyIterator pos = tidyOptGetDocLinksList(tdoc, topt);
    while( pos )
    {
        optLinked = tidyOptGetNextDocLinks(tdoc, &pos );
        printf("  <seealso>%s</seealso>\n",tidyOptGetName(optLinked));
    }
}

/**
**  Prints for XML an option.
**/
static void printXMLOption( TidyDoc tdoc, TidyOption topt, OptionDesc *d )
{
    if ( tidyOptIsReadOnly(topt) )
        return;

    printf( " <option class=\"%s\">\n", d->cat );
    printf  ("  <name>%s</name>\n",d->name);
    printf  ("  <type>%s</type>\n",d->type);
    if (d->def)
        printf("  <default>%s</default>\n",d->def);
    else
        printf("  <default />\n");
    if (d->haveVals)
    {
        printf("  <example>");
        PrintAllowedValues( topt, d );
        printf("</example>\n");
    }
    else
    {
        printf("  <example />\n");
    }
    printXMLDescription( tdoc, topt );
    printXMLCrossRef( tdoc, topt );
    printf( " </option>\n" );
}

/**
**  Handles the -xml-config service.
**/
static void XMLoptionhelp( TidyDoc tdoc )
{
    printf( "<?xml version=\"1.0\"?>\n"
           "<config version=\"%s\">\n", tidyLibraryVersion());
    ForEachOption( tdoc, printXMLOption );
    printf( "</config>\n" );
}

/**
**  Retrieves allowed values from an option's pick list.
*/
static tmbstr GetAllowedValuesFromPick( TidyOption topt )
{
    TidyIterator pos;
    Bool first;
    ctmbstr def;
    uint len = 0;
    tmbstr val;

    pos = tidyOptGetPickList( topt );
    first = yes;
    while ( pos )
    {
        if (first)
            first = no;
        else
            len += 2;
        def = tidyOptGetNextPick( topt, &pos );
        len += strlen(def);
    }
    val = (tmbstr)malloc(len+1);
    if (!val) outOfMemory();
    val[0] = '\0';
    pos = tidyOptGetPickList( topt );
    first = yes;
    while ( pos )
    {
        if (first)
            first = no;
        else
            strcat(val, ", ");
        def = tidyOptGetNextPick( topt, &pos );
        strcat(val, def);
    }
    return val;
}

/**
**  Retrieves allowed values for an option.
*/
static tmbstr GetAllowedValues( TidyOption topt, const OptionDesc *d )
{
    if (d->vals)
    {
        tmbstr val = (tmbstr)malloc(1+strlen(d->vals));
        if (!val) outOfMemory();
        strcpy(val, d->vals);
        return val;
    }
    else
        return GetAllowedValuesFromPick( topt );
}

/**
**  Prints a single option.
*/
static void printOption( TidyDoc ARG_UNUSED(tdoc), TidyOption topt,
                 OptionDesc *d )
{
    if ( tidyOptIsReadOnly(topt) )
        return;

    if ( *d->name || *d->type )
    {
        ctmbstr pval = d->vals;
        tmbstr val = NULL;
        if (!d->haveVals)
        {
            pval = "-";
        }
        else if (pval == NULL)
        {
            val = GetAllowedValues( topt, d);
            pval = val;
        }
        print3Columns( fmt, 27, 9, 40, d->name, d->type, pval );
        if (val)
            free(val);
    }
}

/**
**  Handles the -help-config service.
*/
static void optionhelp( TidyDoc tdoc )
{
	printf( "%s", tidyLocalizedString( TC_TXT_HELP_CONFIG ) );

	printf( fmt,
		   tidyLocalizedString( TC_TXT_HELP_CONFIG_NAME ),
		   tidyLocalizedString( TC_TXT_HELP_CONFIG_TYPE ),
		   tidyLocalizedString( TC_TXT_HELP_CONFIG_ALLW ) );

	printf( fmt, ul, ul, ul );

    ForEachSortedOption( tdoc, printOption );
}

/**
 **  Simple string replacement - lifted from admin@binarytides.com
 */
tmbstr replace_string(ctmbstr needle , ctmbstr replace , ctmbstr haystack)
{
	tmbstr p = NULL;
	tmbstr old = NULL;
	tmbstr result = NULL ;
	int c = 0;
	int search_size = strlen(needle);
	
	/* Count how many occurences */
	for(p = strstr(haystack , needle) ; p != NULL ; p = strstr(p + search_size , needle))
	{
		c++;
	}
	
	/* Final size */
	c = ( strlen(replace) - search_size )*c + strlen(haystack) + 1;
	
	/* New subject with new size */
	if (!( result = malloc( c ) ))
		outOfMemory();
	
	/* Set it to blank */
	strcpy(result , "");
	
	/* The start position */
	old = (tmbstr)haystack;
	
	for(p = strstr(haystack , needle) ; p != NULL ; p = strstr(p + search_size , needle))
	{
		/* move ahead and copy some text from original subject, from a certain position */
		strncpy(result + strlen(result) , old , p - old);
		
		/* move ahead and copy the replacement text */
		strcpy(result + strlen(result) , replace);
		
		/* The new start position after this search match */
		old = p + search_size;
	}
	
	/* Copy the part after the last search match */
	strcpy(result + strlen(result) , old);
	
	return result;
}
	
/**
 **  Option descriptions are HTML formatted, but we
 **  want to display them in a console.
 */
static tmbstr get_prepared_content( ctmbstr content )
{
	tmbstr prepared = NULL;
	tmbstr replacement = "";
	int i = 0;
	
	/* Our generators allow <code>, <em>, <strong>, <br/>, and <p>,
	   but <br/> will be taken care of specially. */
	ctmbstr tags_open[] = { "<code>", "<em>", "<strong>", "<p>", NULL };
	ctmbstr tags_close[] = { "</code>", "</em>", "</strong>", "</p>", NULL };
	
	prepared = replace_string( "<br/>", "\n\n", content );
	
	/* Use the string substitution method to consider the possibility
	   of using ANSI escape sequences for styling. Not all terminals
	   support ANSI colors, so for fun we'll demo with Mac OS X. */
	
#if defined(MAC_OS_X) && 0
	replacement = "\x1b[36m";
#endif
	i = 0;
	while (tags_open[i]) {
		prepared = replace_string(tags_open[i], replacement, prepared);
		++i;
	};

#if defined(MAC_OS_X) && 0
	replacement = "\x1b[0m";
#endif
	i = 0;
	while (tags_close[i]) {
		prepared = replace_string(tags_close[i], replacement, prepared);
		++i;
	};

	/* Add back proper angled brackets. */
	prepared = replace_string("&lt;", "<", prepared);
	prepared = replace_string("&gt;", ">", prepared);

	return prepared;
}


/**
**  Handles the -help-option service.
*/
static void optionDescribe( TidyDoc tdoc, char *tag )
{
    tmbstr result = NULL;
    TidyOptionId topt;

    topt = tidyOptGetIdForName( tag );

    if (topt < N_TIDY_OPTIONS)
    {
		result = get_prepared_content( tidyOptGetDoc( tdoc, tidyGetOption( tdoc, topt ) ) );
    }
	else
    {
		result = (tmbstr)tidyLocalizedString(TC_STRING_UNKNOWN_OPTION_B);
    }

	printf( "\n" );
	printf( "`--%s`\n\n", tag );
	print1Column( "%-68.68s\n", 68, result );
    printf( "\n" );
}

/**
*  Prints the option value for a given option.
*/
static void printOptionValues( TidyDoc ARG_UNUSED(tdoc), TidyOption topt,
                       OptionDesc *d )
{
    TidyOptionId optId = tidyOptGetId( topt );
    ctmbstr ro = tidyOptIsReadOnly( topt ) ? "*" : "" ;

    switch ( optId )
    {
        case TidyInlineTags:
        case TidyBlockTags:
        case TidyEmptyTags:
        case TidyPreTags:
        {
            TidyIterator pos = tidyOptGetDeclTagList( tdoc );
            while ( pos )
            {
                d->def = tidyOptGetNextDeclTag(tdoc, optId, &pos);
                if ( pos )
                {
                    if ( *d->name )
                        printf( valfmt, d->name, d->type, ro, d->def );
                    else
                        printf( fmt, d->name, d->type, d->def );
                    d->name = "";
                    d->type = "";
                }
            }
        }
            break;
        case TidyNewline:
            d->def = tidyOptGetCurrPick( tdoc, optId );
            break;
        default:
            break;
    }

    /* fix for http://tidy.sf.net/bug/873921 */
    if ( *d->name || *d->type || (d->def && *d->def) )
    {
        if ( ! d->def )
            d->def = "";
        if ( *d->name )
            printf( valfmt, d->name, d->type, ro, d->def );
        else
            printf( fmt, d->name, d->type, d->def );
    }
}

/**
**  Handles the -show-config service.
*/
static void optionvalues( TidyDoc tdoc )
{
    printf( "\nConfiguration File Settings:\n\n" );
    printf( fmt, "Name", "Type", "Current Value" );
    printf( fmt, ul, ul, ul );

    ForEachSortedOption( tdoc, printOptionValues );

    printf( "\n\nValues marked with an *asterisk are calculated \n"
           "internally by HTML Tidy\n\n" );
}

/**
**  Handles the -version service.
*/
static void version( void )
{
#ifdef PLATFORM_NAME
	printf( tidyLocalizedString( TC_STRING_VERS_A ), PLATFORM_NAME, tidyLibraryVersion() );
#else
	printf( tidyLocalizedString( TC_STRING_VERS_A ), tidyLibraryVersion() );
#endif
}

/**
**  Provides the `unknown option` output.
*/
static void unknownOption( uint c )
{
	fprintf( errout, tidyLocalizedString( TC_STRING_UNKNOWN_OPTION ), (char)c );
}

/**
**  Handles pretty-printing callbacks.
**/
void progressTester( TidyDoc tdoc, uint srcLine, uint srcCol, uint dstLine)
{
    //   fprintf(stderr, "srcLine = %u, srcCol = %u, dstLine = %u\n", srcLine, srcCol, dstLine);
}


/**
**  MAIN --  let's do something here.
*/
int main( int argc, char** argv )
{
    ctmbstr prog = argv[0];
    ctmbstr cfgfil = NULL, errfil = NULL, htmlfil = NULL;
    TidyDoc tdoc = tidyCreate();
    int status = 0;

    uint contentErrors = 0;
    uint contentWarnings = 0;
    uint accessWarnings = 0;

    errout = stderr;  /* initialize to stderr */

    tidySetPrettyPrinterCallback(tdoc, progressTester);

#if !defined(NDEBUG) && defined(_MSC_VER)
    set_log_file((char *)"temptidy.txt", 0);
    // add_append_log(1);
#endif

#ifdef TIDY_CONFIG_FILE
    if ( tidyFileExists( tdoc, TIDY_CONFIG_FILE) )
    {
        status = tidyLoadConfig( tdoc, TIDY_CONFIG_FILE );
        if ( status != 0 )
			fprintf(errout, tidyLocalizedString( TC_MAIN_ERROR_LOAD_CONFIG ), TIDY_CONFIG_FILE, status);
    }
#endif /* TIDY_CONFIG_FILE */

    /* look for env var "HTML_TIDY" */
    /* then for ~/.tidyrc (on platforms defining $HOME) */

    if ( (cfgfil = getenv("HTML_TIDY")) != NULL )
    {
        status = tidyLoadConfig( tdoc, cfgfil );
        if ( status != 0 )
            fprintf(errout, tidyLocalizedString( TC_MAIN_ERROR_LOAD_CONFIG ), cfgfil, status);
    }
#ifdef TIDY_USER_CONFIG_FILE
    else if ( tidyFileExists( tdoc, TIDY_USER_CONFIG_FILE) )
    {
        status = tidyLoadConfig( tdoc, TIDY_USER_CONFIG_FILE );
        if ( status != 0 )
            fprintf(errout, tidyLocalizedString( TC_MAIN_ERROR_LOAD_CONFIG ), TIDY_USER_CONFIG_FILE, status);
    }
#endif /* TIDY_USER_CONFIG_FILE */

    /* read command line */
    while ( argc > 0 )
    {
        if (argc > 1 && argv[1][0] == '-')
        {
            /* support -foo and --foo */
            ctmbstr arg = argv[1] + 1;

            if ( strcasecmp(arg, "xml") == 0)
                tidyOptSetBool( tdoc, TidyXmlTags, yes );

            else if ( strcasecmp(arg,   "asxml") == 0 ||
                     strcasecmp(arg, "asxhtml") == 0 )
            {
                tidyOptSetBool( tdoc, TidyXhtmlOut, yes );
            }
            else if ( strcasecmp(arg,   "ashtml") == 0 )
                tidyOptSetBool( tdoc, TidyHtmlOut, yes );

            else if ( strcasecmp(arg, "indent") == 0 )
            {
                tidyOptSetInt( tdoc, TidyIndentContent, TidyAutoState );
                if ( tidyOptGetInt(tdoc, TidyIndentSpaces) == 0 )
                    tidyOptResetToDefault( tdoc, TidyIndentSpaces );
            }
            else if ( strcasecmp(arg, "omit") == 0 )
                tidyOptSetBool( tdoc, TidyOmitOptionalTags, yes );

            else if ( strcasecmp(arg, "upper") == 0 )
                tidyOptSetBool( tdoc, TidyUpperCaseTags, yes );

            else if ( strcasecmp(arg, "clean") == 0 )
                tidyOptSetBool( tdoc, TidyMakeClean, yes );

            else if ( strcasecmp(arg, "gdoc") == 0 )
                tidyOptSetBool( tdoc, TidyGDocClean, yes );

            else if ( strcasecmp(arg, "bare") == 0 )
                tidyOptSetBool( tdoc, TidyMakeBare, yes );

            else if ( strcasecmp(arg, "raw") == 0      ||
                     strcasecmp(arg, "ascii") == 0    ||
                     strcasecmp(arg, "latin0") == 0   ||
                     strcasecmp(arg, "latin1") == 0   ||
                     strcasecmp(arg, "utf8") == 0     ||
#ifndef NO_NATIVE_ISO2022_SUPPORT
                     strcasecmp(arg, "iso2022") == 0  ||
#endif
#if SUPPORT_UTF16_ENCODINGS
                     strcasecmp(arg, "utf16le") == 0  ||
                     strcasecmp(arg, "utf16be") == 0  ||
                     strcasecmp(arg, "utf16") == 0    ||
#endif
#if SUPPORT_ASIAN_ENCODINGS
                     strcasecmp(arg, "shiftjis") == 0 ||
                     strcasecmp(arg, "big5") == 0     ||
#endif
                     strcasecmp(arg, "mac") == 0      ||
                     strcasecmp(arg, "win1252") == 0  ||
                     strcasecmp(arg, "ibm858") == 0 )
            {
                tidySetCharEncoding( tdoc, arg );
            }
            else if ( strcasecmp(arg, "numeric") == 0 )
                tidyOptSetBool( tdoc, TidyNumEntities, yes );

            else if ( strcasecmp(arg, "modify") == 0 ||
                     strcasecmp(arg, "change") == 0 ||  /* obsolete */
                     strcasecmp(arg, "update") == 0 )   /* obsolete */
            {
                tidyOptSetBool( tdoc, TidyWriteBack, yes );
            }
            else if ( strcasecmp(arg, "errors") == 0 )
                tidyOptSetBool( tdoc, TidyShowMarkup, no );

            else if ( strcasecmp(arg, "quiet") == 0 )
                tidyOptSetBool( tdoc, TidyQuiet, yes );

            else if ( strcasecmp(arg, "help") == 0 ||
                     strcasecmp(arg, "-help") == 0 ||
                     strcasecmp(arg,    "h") == 0 || *arg == '?' )
            {
                help( prog );
                tidyRelease( tdoc );
                return 0; /* success */
            }
            else if ( strcasecmp(arg, "xml-help") == 0)
            {
                xml_help( );
                tidyRelease( tdoc );
                return 0; /* success */
            }
            else if ( strcasecmp(arg, "help-config") == 0 )
            {
                optionhelp( tdoc );
                tidyRelease( tdoc );
                return 0; /* success */
            }
            else if ( strcasecmp(arg, "help-option") == 0 )
            {
                if ( argc >= 3)
                {
                    optionDescribe( tdoc, argv[2] );
                }
                else
                {
                    printf( "%s\n", tidyLocalizedString(TC_STRING_MUST_SPECIFY));
                }
                tidyRelease( tdoc );
                return 0; /* success */
            }
            else if ( strcasecmp(arg, "xml-config") == 0 )
            {
                XMLoptionhelp( tdoc );
                tidyRelease( tdoc );
                return 0; /* success */
            }
            else if ( strcasecmp(arg, "show-config") == 0 )
            {
                optionvalues( tdoc );
                tidyRelease( tdoc );
                return 0; /* success */
            }
            else if ( strcasecmp(arg, "config") == 0 )
            {
                if ( argc >= 3 )
                {
                    ctmbstr post;

                    tidyLoadConfig( tdoc, argv[2] );

                    /* Set new error output stream if setting changed */
                    post = tidyOptGetValue( tdoc, TidyErrFile );
                    if ( post && (!errfil || !samefile(errfil, post)) )
                    {
                        errfil = post;
                        errout = tidySetErrorFile( tdoc, post );
                    }

                    --argc;
                    ++argv;
                }
            }

#if SUPPORT_ASIAN_ENCODINGS
            else if ( strcasecmp(arg, "language") == 0 ||
                     strcasecmp(arg,     "lang") == 0 )
            {
                if ( argc >= 3 )
                {
                    tidyOptSetValue( tdoc, TidyLanguage, argv[2] );
                    --argc;
                    ++argv;
                }
            }
#endif

            else if ( strcasecmp(arg, "output") == 0 ||
                     strcasecmp(arg, "-output-file") == 0 ||
                     strcasecmp(arg, "o") == 0 )
            {
                if ( argc >= 3 )
                {
                    tidyOptSetValue( tdoc, TidyOutFile, argv[2] );
                    --argc;
                    ++argv;
                }
            }
            else if ( strcasecmp(arg,  "file") == 0 ||
                     strcasecmp(arg, "-file") == 0 ||
                     strcasecmp(arg,     "f") == 0 )
            {
                if ( argc >= 3 )
                {
                    errfil = argv[2];
                    errout = tidySetErrorFile( tdoc, errfil );
                    --argc;
                    ++argv;
                }
            }
            else if ( strcasecmp(arg,  "wrap") == 0 ||
                     strcasecmp(arg, "-wrap") == 0 ||
                     strcasecmp(arg,     "w") == 0 )
            {
                if ( argc >= 3 )
                {
                    uint wraplen = 0;
                    int nfields = sscanf( argv[2], "%u", &wraplen );
                    tidyOptSetInt( tdoc, TidyWrapLen, wraplen );
                    if (nfields > 0)
                    {
                        --argc;
                        ++argv;
                    }
                }
            }
            else if ( strcasecmp(arg,  "version") == 0 ||
                     strcasecmp(arg, "-version") == 0 ||
                     strcasecmp(arg,        "v") == 0 )
            {
                version();
                tidyRelease( tdoc );
                return 0;  /* success */

            }
            else if ( strncmp(argv[1], "--", 2 ) == 0)
            {
                if ( tidyOptParseValue(tdoc, argv[1]+2, argv[2]) )
                {
                    /* Set new error output stream if setting changed */
                    ctmbstr post = tidyOptGetValue( tdoc, TidyErrFile );
                    if ( post && (!errfil || !samefile(errfil, post)) )
                    {
                        errfil = post;
                        errout = tidySetErrorFile( tdoc, post );
                    }

                    ++argv;
                    --argc;
                }
            }

#if SUPPORT_ACCESSIBILITY_CHECKS
            else if ( strcasecmp(arg, "access") == 0 )
            {
                if ( argc >= 3 )
                {
                    uint acclvl = 0;
                    int nfields = sscanf( argv[2], "%u", &acclvl );
                    tidyOptSetInt( tdoc, TidyAccessibilityCheckLevel, acclvl );
                    if (nfields > 0)
                    {
                        --argc;
                        ++argv;
                    }
                }
            }
#endif

            else
            {
                uint c;
                ctmbstr s = argv[1];

                while ( (c = *++s) != '\0' )
                {
                    switch ( c )
                    {
                        case 'i':
                            tidyOptSetInt( tdoc, TidyIndentContent, TidyAutoState );
                            if ( tidyOptGetInt(tdoc, TidyIndentSpaces) == 0 )
                                tidyOptResetToDefault( tdoc, TidyIndentSpaces );
                            break;

                            /* Usurp -o for output file.  Anyone hiding end tags?
                             case 'o':
                             tidyOptSetBool( tdoc, TidyHideEndTags, yes );
                             break;
                             */

                        case 'u':
                            tidyOptSetBool( tdoc, TidyUpperCaseTags, yes );
                            break;

                        case 'c':
                            tidyOptSetBool( tdoc, TidyMakeClean, yes );
                            break;

                        case 'g':
                            tidyOptSetBool( tdoc, TidyGDocClean, yes );
                            break;

                        case 'b':
                            tidyOptSetBool( tdoc, TidyMakeBare, yes );
                            break;

                        case 'n':
                            tidyOptSetBool( tdoc, TidyNumEntities, yes );
                            break;

                        case 'm':
                            tidyOptSetBool( tdoc, TidyWriteBack, yes );
                            break;

                        case 'e':
                            tidyOptSetBool( tdoc, TidyShowMarkup, no );
                            break;

                        case 'q':
                            tidyOptSetBool( tdoc, TidyQuiet, yes );
                            break;

                        default:
                            unknownOption( c );
                            break;
                    }
                }
            }

            --argc;
            ++argv;
            continue;
        }

        if ( argc > 1 )
        {
            htmlfil = argv[1];
#if (!defined(NDEBUG) && defined(_MSC_VER))
            SPRTF("Tidying '%s'\n", htmlfil);
#endif // DEBUG outout
            if ( tidyOptGetBool(tdoc, TidyEmacs) )
                tidyOptSetValue( tdoc, TidyEmacsFile, htmlfil );
            status = tidyParseFile( tdoc, htmlfil );
        }
        else
        {
            htmlfil = "stdin";
            status = tidyParseStdin( tdoc );
        }

        if ( status >= 0 )
            status = tidyCleanAndRepair( tdoc );

        if ( status >= 0 ) {
            status = tidyRunDiagnostics( tdoc );
            if ( !tidyOptGetBool(tdoc, TidyQuiet) ) {
                /* NOT quiet, show DOCTYPE, if not already shown */
                if (!tidyOptGetBool(tdoc, TidyShowInfo)) {
                    tidyOptSetBool( tdoc, TidyShowInfo, yes );
                    tidyReportDoctype( tdoc );  /* FIX20140913: like warnings, errors, ALWAYS report DOCTYPE */
                    tidyOptSetBool( tdoc, TidyShowInfo, no );
                }
            }

        }
        if ( status > 1 ) /* If errors, do we want to force output? */
            status = ( tidyOptGetBool(tdoc, TidyForceOutput) ? status : -1 );

        if ( status >= 0 && tidyOptGetBool(tdoc, TidyShowMarkup) )
        {
            if ( tidyOptGetBool(tdoc, TidyWriteBack) && argc > 1 )
                status = tidySaveFile( tdoc, htmlfil );
            else
            {
                ctmbstr outfil = tidyOptGetValue( tdoc, TidyOutFile );
                if ( outfil ) {
                    status = tidySaveFile( tdoc, outfil );
                } else {
#if !defined(NDEBUG) && defined(_MSC_VER)
                    static char tmp_buf[264];
                    sprintf(tmp_buf,"%s.html",get_log_file());
                    status = tidySaveFile( tdoc, tmp_buf );
                    SPRTF("Saved tidied content to '%s'\n",tmp_buf);
#else
                    status = tidySaveStdout( tdoc );
#endif
                }
            }
        }
        
        contentErrors   += tidyErrorCount( tdoc );
        contentWarnings += tidyWarningCount( tdoc );
        accessWarnings  += tidyAccessWarningCount( tdoc );
        
        --argc;
        ++argv;
        
        if ( argc <= 1 )
            break;
    }
    
    if (!tidyOptGetBool(tdoc, TidyQuiet) &&
        errout == stderr && !contentErrors)
        fprintf(errout, "\n");
    
    if (contentErrors + contentWarnings > 0 && 
        !tidyOptGetBool(tdoc, TidyQuiet))
        tidyErrorSummary(tdoc);
    
    if (!tidyOptGetBool(tdoc, TidyQuiet))
        tidyGeneralInfo(tdoc);
    
    /* called to free hash tables etc. */
    tidyRelease( tdoc );
    
    /* return status can be used by scripts */
    if ( contentErrors > 0 )
        return 2;
    
    if ( contentWarnings > 0 )
        return 1;
    
    /* 0 signifies all is ok */
    return 0;
}

/*
 * local variables:
 * mode: c
 * indent-tabs-mode: nil
 * c-basic-offset: 4
 * eval: (c-set-offset 'substatement-open 0)
 * end:
 */
