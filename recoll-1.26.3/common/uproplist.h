/* Copyright (C) 2004 J.F.Dockes
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#ifndef _PROPLIST_H_INCLUDED_
#define _PROPLIST_H_INCLUDED_


/** 
 * A subset of Unicode chars that we consider word breaks when we
 * split text in words. 
 *
 * This is used as a quick fix to the ascii-based code, and is not correct.
 * the correct way would be to do what http://www.unicode.org/reports/tr29/ 
 * says. 
*/

// Punctuation chararacters blocks array.  Each block is defined by a
// starting and ending code point (both included). MUST BE SORTED.
static const unsigned unipuncblocks[] = {
    // Start of latin-1 supplement block, up to capital A grave
    0x0080, 0x00BF,
    // General punctuation
    0x2000, 0x206F,
    // Superscripts and subscripts
    0x2070, 0x209F,
    // Currency symbols
    0x20A0, 0x20CF,
    // Letterlike symbols
    0x2100, 0x214f,
    // Number forms
    0x2150, 0x218F,
    // Arrows
    0x2190, 0x21FF,
    // Mathematical Operators
    0x2200, 0x22FF,
    // Miscellaneous Technical
    0x2300, 0x23FF,
    // Control Pictures
    0x2400, 0x243F,
    // Optical Character Recognition
    0x2440, 0x245F,
    // Enclosed Alphanumerics
    0x2460, 0x24FF,
    // Box Drawing
    0x2500, 0x257F,
    // Block Elements
    0x2580, 0x259F,
    // Geometric Shapes
    0x25A0, 0x25FF,
    // Miscellaneous Symbols
    0x2600, 0x26FF,
    // Dingbats
    0x2700, 0x27BF,
    // Miscellaneous Mathematical Symbols-A 	
    0x27C0, 0x27EF,
    // Supplemental Arrows-A
    0x27F0, 0x27FF,
    // Supplemental Arrows-B
    0x2900, 0x297F,
    // Miscellaneous Mathematical Symbols-B 	
    0x2980,  0x29FF,
    // Supplemental Mathematical Operators
    0x2A00, 0x2AFF,
    // Miscellaneous Symbols and Arrows
    0x2B00, 0x2BFF,
};

// Other punctuation characters list. Not all punctuation is in a
// separate block some is found in the middle of alphanumeric codes.
static const unsigned int unipunc[] = {
    0x00D7, /* MULTIPLICATION SIGN */
    0x00F7, /* DIVISION SIGN */
    0x037E, /* GREEK QUESTION MARK */
    0x0387, /* GREEK ANO TELEIA */
    0x055C, /* ARMENIAN EXCLAMATION MARK */
    0x055E, /* ARMENIAN QUESTION MARK */
    0x0589, /* ARMENIAN FULL STOP */
    0x058A, /* ARMENIAN HYPHEN */
    0x05C3, /* HEBREW PUNCTUATION SOF PASUQ */
    0x060C, /* ARABIC COMMA */
    0x061B, /* ARABIC SEMICOLON */
    0x061F, /* ARABIC QUESTION MARK */
    0x06D4, /* ARABIC FULL STOP */
    0x0964, /* DEVANAGARI DANDA */
    0x0965, /* DEVANAGARI DOUBLE DANDA */
    0x166E, /* CANADIAN SYLLABICS FULL STOP */
    0x1680, /* OGHAM SPACE MARK */
    0x16EB, /* RUNIC SINGLE PUNCTUATION */
    0x16EC, /* RUNIC MULTIPLE PUNCTUATION */
    0x16ED, /* RUNIC CROSS PUNCTUATION */
    0x1803, /* MONGOLIAN FULL STOP */
    0x1806, /* MONGOLIAN TODO SOFT HYPHEN */
    0x1809, /* MONGOLIAN MANCHU FULL STOP */
    0x180E, /* MONGOLIAN VOWEL SEPARATOR */
    0x2E2E, /* REVERSED QUESTION MARK;Po;0;ON;;;;;N;;;;; */
    0x3000, /* IDEOGRAPHIC SPACE*/
    0x3002, /* IDEOGRAPHIC FULL STOP*/
    0x300C, /* LEFT CORNER BRACKET*/
    0x300D, /* RIGHT CORNER BRACKET*/
    0x300E, /* LEFT WHITE CORNER BRACKET*/
    0x300F, /* RIGHT WHITE CORNER BRACKET*/
    0x301C, /* WAVE DASH*/
    0x301D, /* REVERSED DOUBLE PRIME QUOTATION MARK*/
    0x301E, /* LOW DOUBLE PRIME QUOTATION MARK*/
    0x3030, /* WAVY DASH*/
    0x30FB, /* KATAKANA MIDDLE DOT*/
    0xC2B6, /* PILCROW SIGN;So;0;ON;;;;;N;PARAGRAPH SIGN;;;; */
    0xC3B7, /* DIVISION SIGN;Sm;0;ON;;;;;N;;;;; */
    0xFE31, /* PRESENTATION FORM FOR VERTICAL EM DASH*/
    0xFE32, /* PRESENTATION FORM FOR VERTICAL EN DASH*/
    0xFE41, /* PRESENTATION FORM FOR VERTICAL LEFT CORNER BRACKET*/
    0xFE42, /* PRESENTATION FORM FOR VERTICAL RIGHT CORNER BRACKET*/
    0xFE43, /* PRESENTATION FORM FOR VERTICAL LEFT WHITE CORNER BRACKET*/
    0xFE44, /* PRESENTATION FORM FOR VERTICAL RIGHT WHITE CORNER BRACKET*/
    0xFE50, /* [3] SMALL COMMA..SMALL FULL STOP*/
    0xFE51, /* [3] SMALL COMMA..SMALL FULL STOP*/
    0xFE52, /* STOP*/
    0xFE52, /* [3] SMALL COMMA..SMALL FULL STOP*/
    0xFE54, /* [4] SMALL SEMICOLON..SMALL EXCLAMATION MARK*/
    0xFE55, /* [4] SMALL SEMICOLON..SMALL EXCLAMATION MARK*/
    0xFE56, /* [4] SMALL SEMICOLON..SMALL EXCLAMATION MARK*/
    0xFE57, /* [4] SMALL SEMICOLON..SMALL EXCLAMATION MARK*/
    0xFE58, /* SMALL EM DASH */
    0xFE63, /* SMALL HYPHEN-MINUS */
    0xFF01, /* FULLWIDTH EXCLAMATION MARK */
    0xFF02, /* FULLWIDTH QUOTATION MARK */
    0xFF03, /* FULLWIDTH NUMBER SIGN */
    0xFF04, /* FULLWIDTH DOLLAR SIGN */
    0xFF05, /* FULLWIDTH PERCENT SIGN */
    0xFF06, /* FULLWIDTH AMPERSAND */
    0xFF07, /* FULLWIDTH APOSTROPHE */
    0xFF08, /* FULLWIDTH LEFT PARENTHESIS */
    0xFF09, /* FULLWIDTH RIGHT PARENTHESIS */
    0xFF0A, /* FULLWIDTH ASTERISK */
    0xFF0B, /* FULLWIDTH PLUS SIGN */
    0xFF0C, /* FULLWIDTH COMMA */
    0xFF0D, /* FULLWIDTH HYPHEN-MINUS */
    0xFF0E, /* FULLWIDTH FULL STOP */
    0xFF0F, /* FULLWIDTH SOLIDUS  */
    0xFF1A, /* [2] FULLWIDTH COLON..FULLWIDTH SEMICOLON*/
    0xFF1B, /* [2] FULLWIDTH COLON..FULLWIDTH SEMICOLON*/
    0xFF1F, /* FULLWIDTH QUESTION MARK*/
    0xFF61, /* HALFWIDTH IDEOGRAPHIC FULL STOP*/
    0xFF62, /* HALFWIDTH LEFT CORNER BRACKET*/
    0xFF63, /* HALFWIDTH RIGHT CORNER BRACKET*/
    0xFF64, /* HALFWIDTH IDEOGRAPHIC COMMA*/
    0xFF65, /* HALFWIDTH KATAKANA MIDDLE DOT*/
};

// Characters that should just be discarded. Some of these are in the
// above blocks, but this array is tested first, so it's not worth
// breaking the blocks
static const unsigned int uniskip[] = {
    0x00AD, /* SOFT HYPHEN */
    0x034F, /* COMBINING GRAPHEME JOINER */
    0x2027, /* HYPHENATION POINT */
    0x200C, /* ZERO WIDTH NON-JOINER */
    0x200D, /* ZERO WIDTH JOINER */
    0x2060, /* WORD JOINER . Actually this should not be ignored but used to 
	     * prevent a word break... */
};

/* Things that would visibly break a block of text, rendering obvious the need
 * of quotation if a phrase search is wanted */
static const unsigned int avsbwht[] = {
    0x0009, /* CHARACTER TABULATION */
    0x000A, /* LINE FEED */
    0x000D, /* CARRIAGE RETURN */
    0x0020, /* SPACE;Zs;0;WS */
    0x00A0, /* NO-BREAK SPACE;Zs;0;CS */
    0x1680, /* OGHAM SPACE MARK;Zs;0;WS */
    0x180E, /* MONGOLIAN VOWEL SEPARATOR;Zs;0;WS */
    0x2000, /* EN QUAD;Zs;0;WS */
    0x2001, /* EM QUAD;Zs;0;WS */
    0x2002, /* EN SPACE;Zs;0;WS */
    0x2003, /* EM SPACE;Zs;0;WS */
    0x2004, /* THREE-PER-EM SPACE;Zs;0;WS */
    0x2005, /* FOUR-PER-EM SPACE;Zs;0;WS */
    0x2006, /* SIX-PER-EM SPACE;Zs;0;WS */
    0x2007, /* FIGURE SPACE;Zs;0;WS */
    0x2008, /* PUNCTUATION SPACE;Zs;0;WS */
    0x2009, /* THIN SPACE;Zs;0;WS */
    0x200A, /* HAIR SPACE;Zs;0;WS */
    0x202F, /* NARROW NO-BREAK SPACE;Zs;0;CS */
    0x205F, /* MEDIUM MATHEMATICAL SPACE;Zs;0;WS */
    0x3000, /* IDEOGRAPHIC SPACE;Zs;0;WS */
};

#endif // _PROPLIST_H_INCLUDED_
