// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
//
//  file:  rbbirb.cpp
//
//  Copyright (C) 2002-2011, International Business Machines Corporation and others.
//  All Rights Reserved.
//
//  This file contains the RBBIRuleBuilder class implementation.  This is the main class for
//    building (compiling) break rules into the tables required by the runtime
//    RBBI engine.
//

#include "unicode/utypes.h"

#if !UCONFIG_NO_BREAK_ITERATION

#include "unicode/brkiter.h"
#include "unicode/rbbi.h"
#include "unicode/ubrk.h"
#include "unicode/unistr.h"
#include "unicode/uniset.h"
#include "unicode/uchar.h"
#include "unicode/uchriter.h"
#include "unicode/parsepos.h"
#include "unicode/parseerr.h"

#include "cmemory.h"
#include "cstring.h"
#include "rbbirb.h"
#include "rbbinode.h"
#include "rbbiscan.h"
#include "rbbisetb.h"
#include "rbbitblb.h"
#include "rbbidata.h"
#include "uassert.h"


U_NAMESPACE_BEGIN


//----------------------------------------------------------------------------------------
//
//  Constructor.
//
//----------------------------------------------------------------------------------------
RBBIRuleBuilder::RBBIRuleBuilder(const UnicodeString   &rules,
                                       UParseError     *parseErr,
                                       UErrorCode      &status)
 : fRules(rules), fStrippedRules(rules)
{
    fStatus = &status; // status is checked below
    fParseError = parseErr;
    fDebugEnv   = NULL;
#ifdef RBBI_DEBUG
    fDebugEnv   = getenv("U_RBBIDEBUG");
#endif


    fForwardTree        = NULL;
    fReverseTree        = NULL;
    fSafeFwdTree        = NULL;
    fSafeRevTree        = NULL;
    fDefaultTree        = &fForwardTree;
    fForwardTables      = NULL;
    fReverseTables      = NULL;
    fSafeFwdTables      = NULL;
    fSafeRevTables      = NULL;
    fRuleStatusVals     = NULL;
    fChainRules         = FALSE;
    fLBCMNoChain        = FALSE;
    fLookAheadHardBreak = FALSE;
    fUSetNodes          = NULL;
    fRuleStatusVals     = NULL;
    fScanner            = NULL;
    fSetBuilder         = NULL;
    if (parseErr) {
        uprv_memset(parseErr, 0, sizeof(UParseError));
    }

    if (U_FAILURE(status)) {
        return;
    }

    fUSetNodes          = new UVector(status); // bcos status gets overwritten here
    fRuleStatusVals     = new UVector(status);
    fScanner            = new RBBIRuleScanner(this);
    fSetBuilder         = new RBBISetBuilder(this);
    if (U_FAILURE(status)) {
        return;
    }
    if(fSetBuilder == 0 || fScanner == 0 || fUSetNodes == 0 || fRuleStatusVals == 0) {
        status = U_MEMORY_ALLOCATION_ERROR;
    }
}



//----------------------------------------------------------------------------------------
//
//  Destructor
//
//----------------------------------------------------------------------------------------
RBBIRuleBuilder::~RBBIRuleBuilder() {

    int        i;
    for (i=0; ; i++) {
        RBBINode *n = (RBBINode *)fUSetNodes->elementAt(i);
        if (n==NULL) {
            break;
        }
        delete n;
    }

    delete fUSetNodes;
    delete fSetBuilder;
    delete fForwardTables;
    delete fReverseTables;
    delete fSafeFwdTables;
    delete fSafeRevTables;

    delete fForwardTree;
    delete fReverseTree;
    delete fSafeFwdTree;
    delete fSafeRevTree;
    delete fScanner;
    delete fRuleStatusVals;
}





//----------------------------------------------------------------------------------------
//
//   flattenData() -  Collect up the compiled RBBI rule data and put it into
//                    the format for saving in ICU data files,
//                    which is also the format needed by the RBBI runtime engine.
//
//----------------------------------------------------------------------------------------
static int32_t align8(int32_t i) {return (i+7) & 0xfffffff8;}

RBBIDataHeader *RBBIRuleBuilder::flattenData() {
    int32_t    i;

    if (U_FAILURE(*fStatus)) {
        return NULL;
    }

    // Remove whitespace from the rules to make it smaller.
    // The rule parser has already removed comments.
    fStrippedRules = fScanner->stripRules(fStrippedRules);

    // Calculate the size of each section in the data.
    //   Sizes here are padded up to a multiple of 8 for better memory alignment.
    //   Sections sizes actually stored in the header are for the actual data
    //     without the padding.
    //
    int32_t headerSize        = align8(sizeof(RBBIDataHeader));
    int32_t forwardTableSize  = align8(fForwardTables->getTableSize());
    int32_t reverseTableSize  = align8(fReverseTables->getTableSize());
    int32_t safeFwdTableSize  = align8(fSafeFwdTables->getTableSize());
    int32_t safeRevTableSize  = align8(fSafeRevTables->getTableSize());
    int32_t trieSize          = align8(fSetBuilder->getTrieSize());
    int32_t statusTableSize   = align8(fRuleStatusVals->size() * sizeof(int32_t));
    int32_t rulesSize         = align8((fStrippedRules.length()+1) * sizeof(UChar));

    (void)safeFwdTableSize;

    int32_t         totalSize = headerSize
                                + forwardTableSize 
                                + /* reverseTableSize */ 0
                                + /* safeFwdTableSize */ 0
                                + (safeRevTableSize ? safeRevTableSize : reverseTableSize)
                                + statusTableSize + trieSize + rulesSize;

    RBBIDataHeader  *data     = (RBBIDataHeader *)uprv_malloc(totalSize);
    if (data == NULL) {
        *fStatus = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }
    uprv_memset(data, 0, totalSize);


    data->fMagic            = 0xb1a0;
    data->fFormatVersion[0] = RBBI_DATA_FORMAT_VERSION[0];
    data->fFormatVersion[1] = RBBI_DATA_FORMAT_VERSION[1];
    data->fFormatVersion[2] = RBBI_DATA_FORMAT_VERSION[2];
    data->fFormatVersion[3] = RBBI_DATA_FORMAT_VERSION[3];
    data->fLength           = totalSize;
    data->fCatCount         = fSetBuilder->getNumCharCategories();

    // Only save the forward table and the safe reverse table,
    // because these are the only ones used at run-time.
    //
    // For the moment, we still build the other tables if they are present in the rule source files,
    // for backwards compatibility. Old rule files need to work, and this is the simplest approach.
    //
    // Additional backwards compatibility consideration: if no safe rules are provided, consider the
    // reverse rules to actually be the safe reverse rules.

    data->fFTable        = headerSize;
    data->fFTableLen     = forwardTableSize;

    // Do not save Reverse Table.
    data->fRTable        = data->fFTable  + forwardTableSize;
    data->fRTableLen     = 0;

    // Do not save the Safe Forward table.
    data->fSFTable       = data->fRTable + 0;
    data->fSFTableLen    = 0;

    data->fSRTable       = data->fSFTable + 0;
    if (safeRevTableSize > 0) {
        data->fSRTableLen    = safeRevTableSize;
    } else if (reverseTableSize > 0) {
        data->fSRTableLen    = reverseTableSize;
    } else {
        U_ASSERT(FALSE);    // Rule build should have failed for lack of a reverse table
                            // before reaching this point.
    }
        

    data->fTrie          = data->fSRTable + data->fSRTableLen;
    data->fTrieLen       = fSetBuilder->getTrieSize();
    data->fStatusTable   = data->fTrie    + trieSize;
    data->fStatusTableLen= statusTableSize;
    data->fRuleSource    = data->fStatusTable + statusTableSize;
    data->fRuleSourceLen = fStrippedRules.length() * sizeof(UChar);

    uprv_memset(data->fReserved, 0, sizeof(data->fReserved));

    fForwardTables->exportTable((uint8_t *)data + data->fFTable);
    // fReverseTables->exportTable((uint8_t *)data + data->fRTable);
    // fSafeFwdTables->exportTable((uint8_t *)data + data->fSFTable);
    if (safeRevTableSize > 0) {
        fSafeRevTables->exportTable((uint8_t *)data + data->fSRTable);
    } else {
        fReverseTables->exportTable((uint8_t *)data + data->fSRTable);
    }

    fSetBuilder->serializeTrie ((uint8_t *)data + data->fTrie);

    int32_t *ruleStatusTable = (int32_t *)((uint8_t *)data + data->fStatusTable);
    for (i=0; i<fRuleStatusVals->size(); i++) {
        ruleStatusTable[i] = fRuleStatusVals->elementAti(i);
    }

    fStrippedRules.extract((UChar *)((uint8_t *)data+data->fRuleSource), rulesSize/2+1, *fStatus);

    return data;
}






//----------------------------------------------------------------------------------------
//
//  createRuleBasedBreakIterator    construct from source rules that are passed in
//                                  in a UnicodeString
//
//----------------------------------------------------------------------------------------
BreakIterator *
RBBIRuleBuilder::createRuleBasedBreakIterator( const UnicodeString    &rules,
                                    UParseError      *parseError,
                                    UErrorCode       &status)
{
    // status checked below

    //
    // Read the input rules, generate a parse tree, symbol table,
    // and list of all Unicode Sets referenced by the rules.
    //
    RBBIRuleBuilder  builder(rules, parseError, status);
    if (U_FAILURE(status)) { // status checked here bcos build below doesn't
        return NULL;
    }
    builder.fScanner->parse();

    //
    // UnicodeSet processing.
    //    Munge the Unicode Sets to create a set of character categories.
    //    Generate the mapping tables (TRIE) from input code points to
    //    the character categories.
    //
    builder.fSetBuilder->buildRanges();


    //
    //   Generate the DFA state transition table.
    //
    builder.fForwardTables = new RBBITableBuilder(&builder, &builder.fForwardTree);
    builder.fReverseTables = new RBBITableBuilder(&builder, &builder.fReverseTree);
    builder.fSafeFwdTables = new RBBITableBuilder(&builder, &builder.fSafeFwdTree);
    builder.fSafeRevTables = new RBBITableBuilder(&builder, &builder.fSafeRevTree);
    if (builder.fForwardTables == NULL || builder.fReverseTables == NULL ||
        builder.fSafeFwdTables == NULL || builder.fSafeRevTables == NULL)
    {
        status = U_MEMORY_ALLOCATION_ERROR;
        delete builder.fForwardTables; builder.fForwardTables = NULL;
        delete builder.fReverseTables; builder.fReverseTables = NULL;
        delete builder.fSafeFwdTables; builder.fSafeFwdTables = NULL;
        delete builder.fSafeRevTables; builder.fSafeRevTables = NULL;
        return NULL;
    }

    builder.fForwardTables->build();
    builder.fReverseTables->build();
    builder.fSafeFwdTables->build();
    builder.fSafeRevTables->build();

#ifdef RBBI_DEBUG
    if (builder.fDebugEnv && uprv_strstr(builder.fDebugEnv, "states")) {
        builder.fForwardTables->printRuleStatusTable();
    }
#endif

    builder.optimizeTables();
    builder.fSetBuilder->buildTrie();



    //
    //   Package up the compiled data into a memory image
    //      in the run-time format.
    //
    RBBIDataHeader *data = builder.flattenData(); // returns NULL if error
    if (U_FAILURE(*builder.fStatus)) {
        return NULL;
    }


    //
    //  Clean up the compiler related stuff
    //


    //
    //  Create a break iterator from the compiled rules.
    //     (Identical to creation from stored pre-compiled rules)
    //
    // status is checked after init in construction.
    RuleBasedBreakIterator *This = new RuleBasedBreakIterator(data, status);
    if (U_FAILURE(status)) {
        delete This;
        This = NULL;
    } 
    else if(This == NULL) { // test for NULL
        status = U_MEMORY_ALLOCATION_ERROR;
    }
    return This;
}

void RBBIRuleBuilder::optimizeTables() {
    int32_t leftClass;
    int32_t rightClass;

    leftClass = 3;
    rightClass = 0;
    while (fForwardTables->findDuplCharClassFrom(leftClass, rightClass)) {
        fSetBuilder->mergeCategories(leftClass, rightClass);
        fForwardTables->removeColumn(rightClass);
        fReverseTables->removeColumn(rightClass);
        fSafeFwdTables->removeColumn(rightClass);
        fSafeRevTables->removeColumn(rightClass);
    }

    fForwardTables->removeDuplicateStates();
    fReverseTables->removeDuplicateStates();
    fSafeFwdTables->removeDuplicateStates();
    fSafeRevTables->removeDuplicateStates();



}

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_BREAK_ITERATION */
