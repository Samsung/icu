// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING && !UPRV_INCOMPLETE_CPP11_SUPPORT
#ifndef __NUMBER_PATTERNSTRING_H__
#define __NUMBER_PATTERNSTRING_H__


#include <cstdint>
#include "unicode/unum.h"
#include "unicode/unistr.h"
#include "number_types.h"
#include "number_decimalquantity.h"
#include "number_decimfmtprops.h"
#include "number_affixutils.h"

U_NAMESPACE_BEGIN namespace number {
namespace impl {

// Forward declaration
class PatternParser;

// Exported as U_I18N_API because it is a public member field of exported ParsedSubpatternInfo
struct U_I18N_API Endpoints {
    int32_t start = 0;
    int32_t end = 0;
};

// Exported as U_I18N_API because it is a public member field of exported ParsedPatternInfo
struct U_I18N_API ParsedSubpatternInfo {
    int64_t groupingSizes = 0x0000ffffffff0000L;
    int32_t integerLeadingHashSigns = 0;
    int32_t integerTrailingHashSigns = 0;
    int32_t integerNumerals = 0;
    int32_t integerAtSigns = 0;
    int32_t integerTotal = 0; // for convenience
    int32_t fractionNumerals = 0;
    int32_t fractionHashSigns = 0;
    int32_t fractionTotal = 0; // for convenience
    bool hasDecimal = false;
    int32_t widthExceptAffixes = 0;
    NullableValue<UNumberFormatPadPosition> paddingLocation;
    DecimalQuantity rounding;
    bool exponentHasPlusSign = false;
    int32_t exponentZeros = 0;
    bool hasPercentSign = false;
    bool hasPerMilleSign = false;
    bool hasCurrencySign = false;
    bool hasMinusSign = false;
    bool hasPlusSign = false;

    Endpoints prefixEndpoints;
    Endpoints suffixEndpoints;
    Endpoints paddingEndpoints;
};

// Exported as U_I18N_API because it is needed for the unit test PatternStringTest
struct U_I18N_API ParsedPatternInfo : public AffixPatternProvider, public UMemory {
    UnicodeString pattern;
    ParsedSubpatternInfo positive;
    ParsedSubpatternInfo negative;

    ParsedPatternInfo() : state(this->pattern), currentSubpattern(nullptr) {}

    ~ParsedPatternInfo() U_OVERRIDE = default;

    static int32_t getLengthFromEndpoints(const Endpoints &endpoints);

    char16_t charAt(int32_t flags, int32_t index) const U_OVERRIDE;

    int32_t length(int32_t flags) const U_OVERRIDE;

    UnicodeString getString(int32_t flags) const;

    bool positiveHasPlusSign() const U_OVERRIDE;

    bool hasNegativeSubpattern() const U_OVERRIDE;

    bool negativeHasMinusSign() const U_OVERRIDE;

    bool hasCurrencySign() const U_OVERRIDE;

    bool containsSymbolType(AffixPatternType type, UErrorCode &status) const U_OVERRIDE;

    bool hasBody() const U_OVERRIDE;

  private:
    struct U_I18N_API ParserState {
        const UnicodeString &pattern; // reference to the parent
        int32_t offset = 0;

        explicit ParserState(const UnicodeString &_pattern) : pattern(_pattern) {};

        UChar32 peek();

        UChar32 next();

        // TODO: We don't currently do anything with the message string.
        // This method is here as a shell for Java compatibility.
        inline void toParseException(const char16_t *message) { (void)message; }
    }
    state;

    // NOTE: In Java, these are written as pure functions.
    // In C++, they're written as methods.
    // The behavior is the same.

    // Mutable transient pointer:
    ParsedSubpatternInfo *currentSubpattern;

    // In Java, "negative == null" tells us whether or not we had a negative subpattern.
    // In C++, we need to remember in another boolean.
    bool fHasNegativeSubpattern = false;

    const Endpoints &getEndpoints(int32_t flags) const;

    /** Run the recursive descent parser. */
    void consumePattern(const UnicodeString &patternString, UErrorCode &status);

    void consumeSubpattern(UErrorCode &status);

    void consumePadding(PadPosition paddingLocation, UErrorCode &status);

    void consumeAffix(Endpoints &endpoints, UErrorCode &status);

    void consumeLiteral(UErrorCode &status);

    void consumeFormat(UErrorCode &status);

    void consumeIntegerFormat(UErrorCode &status);

    void consumeFractionFormat(UErrorCode &status);

    void consumeExponent(UErrorCode &status);

    friend class PatternParser;
};

class U_I18N_API PatternParser {
  public:
    /**
     * Runs the recursive descent parser on the given pattern string, returning a data structure with raw information
     * about the pattern string.
     *
     * <p>
     * To obtain a more useful form of the data, consider using {@link #parseToProperties} instead.
     *
     * TODO: Change argument type to const char16_t* instead of UnicodeString?
     *
     * @param patternString
     *            The LDML decimal format pattern (Excel-style pattern) to parse.
     * @return The results of the parse.
     */
    static void
    parseToPatternInfo(const UnicodeString& patternString, ParsedPatternInfo &patternInfo, UErrorCode &status);

    enum IgnoreRounding {
        IGNORE_ROUNDING_NEVER = 0, IGNORE_ROUNDING_IF_CURRENCY = 1, IGNORE_ROUNDING_ALWAYS = 2
    };

    /**
     * Parses a pattern string into a new property bag.
     *
     * @param pattern
     *            The pattern string, like "#,##0.00"
     * @param ignoreRounding
     *            Whether to leave out rounding information (minFrac, maxFrac, and rounding increment) when parsing the
     *            pattern. This may be desirable if a custom rounding mode, such as CurrencyUsage, is to be used
     *            instead.
     * @return A property bag object.
     * @throws IllegalArgumentException
     *             If there is a syntax error in the pattern string.
     */
    static DecimalFormatProperties
    parseToProperties(const UnicodeString& pattern, IgnoreRounding ignoreRounding, UErrorCode &status);

    /**
     * Parses a pattern string into an existing property bag. All properties that can be encoded into a pattern string
     * will be overwritten with either their default value or with the value coming from the pattern string. Properties
     * that cannot be encoded into a pattern string, such as rounding mode, are not modified.
     *
     * @param pattern
     *            The pattern string, like "#,##0.00"
     * @param properties
     *            The property bag object to overwrite.
     * @param ignoreRounding
     *            See {@link #parseToProperties(String pattern, int ignoreRounding)}.
     * @throws IllegalArgumentException
     *             If there was a syntax error in the pattern string.
     */
    static void parseToExistingProperties(const UnicodeString& pattern, DecimalFormatProperties& properties,
                                          IgnoreRounding ignoreRounding, UErrorCode &status);

  private:
    static void
    parseToExistingPropertiesImpl(const UnicodeString& pattern, DecimalFormatProperties &properties,
                                  IgnoreRounding ignoreRounding, UErrorCode &status);

    /** Finalizes the temporary data stored in the ParsedPatternInfo to the Properties. */
    static void
    patternInfoToProperties(DecimalFormatProperties &properties, ParsedPatternInfo& patternInfo,
                            IgnoreRounding _ignoreRounding, UErrorCode &status);
};

class U_I18N_API PatternStringUtils {
  public:
    /**
     * Creates a pattern string from a property bag.
     *
     * <p>
     * Since pattern strings support only a subset of the functionality available in a property bag, a new property bag
     * created from the string returned by this function may not be the same as the original property bag.
     *
     * @param properties
     *            The property bag to serialize.
     * @return A pattern string approximately serializing the property bag.
     */
    static UnicodeString
    propertiesToPatternString(const DecimalFormatProperties &properties, UErrorCode &status);


    /**
     * Converts a pattern between standard notation and localized notation. Localized notation means that instead of
     * using generic placeholders in the pattern, you use the corresponding locale-specific characters instead. For
     * example, in locale <em>fr-FR</em>, the period in the pattern "0.000" means "decimal" in standard notation (as it
     * does in every other locale), but it means "grouping" in localized notation.
     *
     * <p>
     * A greedy string-substitution strategy is used to substitute locale symbols. If two symbols are ambiguous or have
     * the same prefix, the result is not well-defined.
     *
     * <p>
     * Locale symbols are not allowed to contain the ASCII quote character.
     *
     * <p>
     * This method is provided for backwards compatibility and should not be used in any new code.
     *
     * TODO(C++): This method is not yet implemented.
     *
     * @param input
     *            The pattern to convert.
     * @param symbols
     *            The symbols corresponding to the localized pattern.
     * @param toLocalized
     *            true to convert from standard to localized notation; false to convert from localized to standard
     *            notation.
     * @return The pattern expressed in the other notation.
     */
    static UnicodeString
    convertLocalized(UnicodeString input, DecimalFormatSymbols symbols, bool toLocalized,
                     UErrorCode &status);

  private:
    /** @return The number of chars inserted. */
    static int
    escapePaddingString(UnicodeString input, UnicodeString &output, int startIndex, UErrorCode &status);
};

} // namespace impl
} // namespace number
U_NAMESPACE_END


#endif //__NUMBER_PATTERNSTRING_H__

#endif /* #if !UCONFIG_NO_FORMATTING */
