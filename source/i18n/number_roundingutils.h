// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING && !UPRV_INCOMPLETE_CPP11_SUPPORT
#ifndef __NUMBER_ROUNDINGUTILS_H__
#define __NUMBER_ROUNDINGUTILS_H__

#include "number_types.h"

U_NAMESPACE_BEGIN
namespace number {
namespace impl {
namespace roundingutils {

enum Section {
    SECTION_LOWER_EDGE = -1,
    SECTION_UPPER_EDGE = -2,
    SECTION_LOWER = 1,
    SECTION_MIDPOINT = 2,
    SECTION_UPPER = 3
};

/**
 * Converts a rounding mode and metadata about the quantity being rounded to a boolean determining
 * whether the value should be rounded toward infinity or toward zero.
 *
 * <p>The parameters are of type int because benchmarks on an x86-64 processor against OpenJDK
 * showed that ints were demonstrably faster than enums in switch statements.
 *
 * @param isEven Whether the digit immediately before the rounding magnitude is even.
 * @param isNegative Whether the quantity is negative.
 * @param section Whether the part of the quantity to the right of the rounding magnitude is
 *     exactly halfway between two digits, whether it is in the lower part (closer to zero), or
 *     whether it is in the upper part (closer to infinity). See {@link #SECTION_LOWER}, {@link
 *     #SECTION_MIDPOINT}, and {@link #SECTION_UPPER}.
 * @param roundingMode The integer version of the {@link RoundingMode}, which you can get via
 *     {@link RoundingMode#ordinal}.
 * @param status Error code, set to U_FORMAT_INEXACT_ERROR if the rounding mode is kRoundUnnecessary.
 * @return true if the number should be rounded toward zero; false if it should be rounded toward
 *     infinity.
 */
inline bool
getRoundingDirection(bool isEven, bool isNegative, Section section, RoundingMode roundingMode,
                     UErrorCode &status) {
    switch (roundingMode) {
        case RoundingMode::UNUM_ROUND_UP:
            // round away from zero
            return false;

        case RoundingMode::UNUM_ROUND_DOWN:
            // round toward zero
            return true;

        case RoundingMode::UNUM_ROUND_CEILING:
            // round toward positive infinity
            return isNegative;

        case RoundingMode::UNUM_ROUND_FLOOR:
            // round toward negative infinity
            return !isNegative;

        case RoundingMode::UNUM_ROUND_HALFUP:
            switch (section) {
                case SECTION_MIDPOINT:
                    return false;
                case SECTION_LOWER:
                    return true;
                case SECTION_UPPER:
                    return false;
                default:
                    break;
            }
            break;

        case RoundingMode::UNUM_ROUND_HALFDOWN:
            switch (section) {
                case SECTION_MIDPOINT:
                    return true;
                case SECTION_LOWER:
                    return true;
                case SECTION_UPPER:
                    return false;
                default:
                    break;
            }
            break;

        case RoundingMode::UNUM_ROUND_HALFEVEN:
            switch (section) {
                case SECTION_MIDPOINT:
                    return isEven;
                case SECTION_LOWER:
                    return true;
                case SECTION_UPPER:
                    return false;
                default:
                    break;
            }
            break;

        default:
            break;
    }

    status = U_FORMAT_INEXACT_ERROR;
    return false;
}

/**
 * Gets whether the given rounding mode's rounding boundary is at the midpoint. The rounding
 * boundary is the point at which a number switches from being rounded down to being rounded up.
 * For example, with rounding mode HALF_EVEN, HALF_UP, or HALF_DOWN, the rounding boundary is at
 * the midpoint, and this function would return true. However, for UP, DOWN, CEILING, and FLOOR,
 * the rounding boundary is at the "edge", and this function would return false.
 *
 * @param roundingMode The integer version of the {@link RoundingMode}.
 * @return true if rounding mode is HALF_EVEN, HALF_UP, or HALF_DOWN; false otherwise.
 */
inline bool roundsAtMidpoint(int roundingMode) {
    switch (roundingMode) {
        case RoundingMode::UNUM_ROUND_UP:
        case RoundingMode::UNUM_ROUND_DOWN:
        case RoundingMode::UNUM_ROUND_CEILING:
        case RoundingMode::UNUM_ROUND_FLOOR:
            return false;

        default:
            return true;
    }
}

} // namespace roundingutils
} // namespace impl
} // namespace number
U_NAMESPACE_END

#endif //__NUMBER_ROUNDINGUTILS_H__

#endif /* #if !UCONFIG_NO_FORMATTING */
