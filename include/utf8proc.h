#pragma once

#include "con4m_base.h"

typedef enum {
  CP_CATEGORY_CN  = 0, /**< Other, not assigned */
  CP_CATEGORY_LU  = 1, /**< Letter, uppercase */
  CP_CATEGORY_LL  = 2, /**< Letter, lowercase */
  CP_CATEGORY_LT  = 3, /**< Letter, titlecase */
  CP_CATEGORY_LM  = 4, /**< Letter, modifier */
  CP_CATEGORY_LO  = 5, /**< Letter, other */
  CP_CATEGORY_MN  = 6, /**< Mark, nonspacing */
  CP_CATEGORY_MC  = 7, /**< Mark, spacing combining */
  CP_CATEGORY_ME  = 8, /**< Mark, enclosing */
  CP_CATEGORY_ND  = 9, /**< Number, decimal digit */
  CP_CATEGORY_NL = 10, /**< Number, letter */
  CP_CATEGORY_NO = 11, /**< Number, other */
  CP_CATEGORY_PC = 12, /**< Punctuation, connector */
  CP_CATEGORY_PD = 13, /**< Punctuation, dash */
  CP_CATEGORY_PS = 14, /**< Punctuation, open */
  CP_CATEGORY_PE = 15, /**< Punctuation, close */
  CP_CATEGORY_PI = 16, /**< Punctuation, initial quote */
  CP_CATEGORY_PF = 17, /**< Punctuation, final quote */
  CP_CATEGORY_PO = 18, /**< Punctuation, other */
  CP_CATEGORY_SM = 19, /**< Symbol, math */
  CP_CATEGORY_SC = 20, /**< Symbol, currency */
  CP_CATEGORY_SK = 21, /**< Symbol, modifier */
  CP_CATEGORY_SO = 22, /**< Symbol, other */
  CP_CATEGORY_ZS = 23, /**< Separator, space */
  CP_CATEGORY_ZL = 24, /**< Separator, line */
  CP_CATEGORY_ZP = 25, /**< Separator, paragraph */
  CP_CATEGORY_CC = 26, /**< Other, control */
  CP_CATEGORY_CF = 27, /**< Other, format */
  CP_CATEGORY_CS = 28, /**< Other, surrogate */
  CP_CATEGORY_CO = 29, /**< Other, private use */
} cp_category_t;

typedef enum {
    LB_MUSTBREAK  = 0,
    LB_ALLOWBREAK = 1,
    LB_NOBREAK    = 2
} lbreak_kind_t;

typedef enum {
  /** The given UTF-8 input is NULL terminated. */
  UTF8PROC_NULLTERM  = (1<<0),

  /** Unicode Versioning Stability has to be respected. */
  UTF8PROC_STABLE    = (1<<1),

  /** Compatibility decomposition (i.e. formatting information is lost). */
  UTF8PROC_COMPAT    = (1<<2),

  /** Return a result with composed characters. */
  UTF8PROC_COMPOSE   = (1<<3),

  /** Return a result with decomposed characters. */
  UTF8PROC_DECOMPOSE = (1<<4),

  /** Strip "default ignorable characters" such as SOFT-HYPHEN or
   * ZERO-WIDTH-SPACE. */
  UTF8PROC_IGNORE    = (1<<5),

  /** Return an error, if the input contains unassigned codepoints. */
  UTF8PROC_REJECTNA  = (1<<6),

  /**
   * Indicating that NLF-sequences (LF, CRLF, CR, NEL) are representing a
   * line break, and should be converted to the codepoint for line
   * separation (LS).
   */
  UTF8PROC_NLF2LS    = (1<<7),

  /**
   * Indicating that NLF-sequences are representing a paragraph break, and
   * should be converted to the codepoint for paragraph separation
   * (PS).
   */
  UTF8PROC_NLF2PS    = (1<<8),

  /** Indicating that the meaning of NLF-sequences is unknown. */
  UTF8PROC_NLF2LF    = (UTF8PROC_NLF2LS | UTF8PROC_NLF2PS),

  /** Strips and/or convers control characters.
   *
   * NLF-sequences are transformed into space, except if one of the
   * NLF2LS/PS/LF options is given. HorizontalTab (HT) and FormFeed (FF)
   * are treated as a NLF-sequence in this case.  All other control
   * characters are simply removed.
   */
  UTF8PROC_STRIPCC   = (1<<9),

  /**
   * Performs unicode case folding, to be able to do a case-insensitive
   * string comparison.
   */
  UTF8PROC_CASEFOLD  = (1<<10),

  /**
   * Inserts 0xFF bytes at the beginning of each sequence which is
   * representing a single grapheme cluster (see UAX#29).
   */
  UTF8PROC_CHARBOUND = (1<<11),

  /** Lumps certain characters together.
   *
   * E.g. HYPHEN U+2010 and MINUS U+2212 to ASCII "-". See lump.md for details.
   *
   * If NLF2LF is set, this includes a transformation of paragraph and
   * line separators to ASCII line-feed (LF).
   */
  UTF8PROC_LUMP      = (1<<12),

  /** Strips all character markings.
   *
   * This includes non-spacing, spacing and enclosing (i.e. accents).
   * @note This option works only with @ref UTF8PROC_COMPOSE or
   *       @ref UTF8PROC_DECOMPOSE
   */
  UTF8PROC_STRIPMARK = (1<<13),

  /**
   * Strip unassigned codepoints.
   */
  UTF8PROC_STRIPNA    = (1<<14),
} utf8proc_option_t;

// From libutf8proc
extern int utf8proc_iterate(const uint8_t *str, ssize_t len, int32_t *cp);
extern bool utf8proc_codepoint_valid(int32_t cp);
extern int utf8proc_encode_char(int32_t cp, uint8_t *dst);
extern const cp_category_t utf8proc_category(int32_t cp);
extern int utf8proc_charwidth(int32_t cp);
extern int32_t utf8proc_tolower(int32_t cp);
extern int32_t utf8proc_toupper(int32_t cp);
extern int utf8proc_charwidth(int32_t cp);
extern bool utf8proc_grapheme_break_stateful(int32_t cp1, int32_t cp2,
					     int32_t *state);
extern int32_t utf8proc_map(const uint8_t *str, int32_t len, uint8_t **out,
			    utf8proc_option_t options);
