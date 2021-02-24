#include "qjson.h"
#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h> 
#include <stdint.h>
#include <errno.h>
#define _XOPEN_SOURCE       /* See feature_test_macros(7) */
#include <time.h>

// qjson_version returns the version of the code and the supported
// syntax (e.g. "qjson-c: v0.1.1 syntax: v0.0.0").
const char* qjson_version() {
	return "qjson-c: v0.0.0 syntax: v0.0.0";
}

// Define NDEBUG to disable asserts and boundary checking.

const int maxDepth = 200;

typedef unsigned char byte;

// pos is a position in the input string.
typedef struct {
	int b; // index of the current char in the input string
	int s; // index of the first char of the line in the input string
	int l; // line number starting at 0
} pos_t;

enum tokenTag_t {
	tagUnknown,
	tagError,
	tagIntegerVal,
	tagDecimalVal,
	tagPlus,
	tagMinus,
	tagMultiplication,
	tagDivision,
	tagXor,
	tagAnd,
	tagOr,
	tagInverse,
	tagModulo,
	tagOpenParen,
	tagCloseParen,
	tagWeeks,
	tagDays,
	tagHours,
	tagMinutes,
	tagSeconds,
	tagOpenBrace,
	tagCloseBrace,
	tagOpenSquare,
	tagCloseSquare,
	tagColon,
	tagQuotelessString,
	tagDoubleQuotedString,
	tagSingleQuotedString,
	tagMultilineString,
	tagComma
};

// slice_t is a slice of characters. 
typedef struct {
	const char *p; // pointer to the first char of the slice
	int         l; // byte length of the slice
} slice_t;

// token_t is a token parsed by the tokenizer.
typedef struct {
	enum tokenTag_t tag; // token tag of the token
	pos_t           pos; // position of the first byte of the token
	slice_t         val; // token value 
} token_t;

// outBuf_t is an output buffer that will grow its storage space as needed.
// Data is written with outBufByte() or outBufString().
typedef struct {
	char  *buf; // data storage allocated with malloc
	int    len; // number of data bytes in storage
	int    cap; // maximum capacity of storage.
} outBuf_t;

// engine_t is the conversion engine.
typedef struct {
	const char *in;     // input string
	slice_t     p;      // text left to parse
	pos_t       pos;    // position of text left to parse
	outBuf_t    out;    // output buffer
	token_t     tk;     // current token
	int         depth;  // depth of [] and {}
} engine_t;

// error_t is an error message with associated pos.
typedef struct {
	pos_t       pos; // the position of the error
	const char *err; // the error message (one of the ErrXXX string)
} error_t;


// -----------------------------------------------------
// errors
// -----------------------------------------------------

// ErrEndOfInput is returned when the end of input is reached.
const char* const ErrEndOfInput = "end of input";

// ErrInvalidChar is returned when an invalid rune is found in the input stream.
const char* const ErrInvalidChar = "invalid character";

// ErrTruncatedChar occurs when the last utf8 char of the input is truncated.
const char* const ErrTruncatedChar = "last utf8 char is truncated";

// ErrSyntaxError is returned when a non-expected token is met.
const char* const ErrSyntaxError = "syntax error";

// ErrUnclosedDoubleQuoteString is returned when a double quote string is unclosed.
const char* const ErrUnclosedDoubleQuoteString = "unclosed double quote string";

// ErrUnclosedSingleQuoteString is returned when a single quote string is unclosed.
const char* const ErrUnclosedSingleQuoteString = "unclosed single quote string";

// ErrUnclosedSlashStarComment is returned when the end of input is found in a /*...*/
const char* const ErrUnclosedSlashStarComment = "unclosed /*...*/ comment";

// ErrNewlineInDoubleQuoteString is returned when a newline is met in a double quoted string.
const char* const ErrNewlineInDoubleQuoteString = "newline in double quoted string";

// ErrNewlineInSingleQuoteString is returned when a newline is met in a single quoted string.
const char* const ErrNewlineInSingleQuoteString = "newline in single quoted string";

// ErrExpectStringIdentifier is return when an invalid identifier type is found.
const char* const ErrExpectStringIdentifier = "expect string identifier";

// ErrExpectColon is return when an a colon is not found after the identifier.
const char* const ErrExpectColon = "expect a colon";

// ErrInvalidValueType is return when an invalid value type is found.
const char* const ErrInvalidValueType = "invalid value type";

// ErrMaxObjectArrayDepth is return when the number of encapsuled object has reached a limit.
const char* const ErrMaxObjectArrayDepth = "too many object or array encapsulations";

// ErrUnclosedObject is returned when the end of input is met before the object was closed.
const char* const ErrUnclosedObject = "unclosed object";

// ErrUnclosedArray is returned when the end of input is met before the array was closed.
const char* const ErrUnclosedArray = "unclosed array";

// ErrUnexpectedEndOfInput is returned when the end of input is met in an unexpected location.
const char* const ErrUnexpectedEndOfInput = "unexpected end of input";

// ErrExpectIdentifierAfterComma is returned when a comma is at end of input or object.
const char* const ErrExpectIdentifierAfterComma = "expect identifier after comma";

// ErrExpectValueAfterComma is returned when a comma is at end of input or array.
const char* const ErrExpectValueAfterComma = "expect value after comma";

// ErrInvalidEscapeSequence is returned when an invalid escape sequence is found in a string.
const char* const ErrInvalidEscapeSequence = "invalid escape squence";

// ErrInvalidNumericExpression is returned when an unrecognized text is found in a numeric expression.
const char* const ErrInvalidNumericExpression = "invalid numeric expression";

// ErrInvalidBinaryNumber is returned when the tokenizer met an invalid binary number.
const char* const ErrInvalidBinaryNumber = "invalid binary number";

// ErrInvalidHexadecimalNumber is returned when the tokenizer met an invalid hexadecimal number.
const char* const ErrInvalidHexadecimalNumber = "invalid hexadecimal number";

// ErrInvalidOctalNumber is returned when the tokenizer met an invalid octal number.
const char* const ErrInvalidOctalNumber = "invalid octal number";

// ErrInvalidIntegerNumber is returned when the tokenizer met an invalid integer number.
const char* const ErrInvalidIntegerNumber = "invalid integer number";

// ErrInvalidDecimalNumber is returned when the tokenizer met an invalid decimal number.
const char* const ErrInvalidDecimalNumber = "invalid decimal number";

// ErrNumberOverflow is retured when a number would overflow a float64 representation.
const char* const ErrNumberOverflow = "number overflow";

// ErrUnopenedParenthesis is returned when a close parenthesis has no matching open parenthesis.
const char* const ErrUnopenedParenthesis = "missing open parenthesis";

// ErrDivisionByZero is returned when there is a division by zero in an expression.
const char* const ErrDivisionByZero = "division by zero";

// ErrUnclosedParenthesis is returned when an open parenthesis has no matching close parenthesis.
const char* const ErrUnclosedParenthesis = "missing close parenthesis";

// ErrOperandMustBeInteger is returned when a binary operation is attempted on a float.
const char* const ErrOperandMustBeInteger = "operand must be integer";

// ErrOperandsMustBeInteger is returned when a binary or a modulo operation is attempted on a float.
const char* const ErrOperandsMustBeInteger = "operands must be integer";

// ErrMarginMustBeWhitespaceOnly is returned when a non whitespace character is found in front of `.
const char* const ErrMarginMustBeWhitespaceOnly = "multiline margin must contain only whitespaces";

// ErrUnclosedMultiline is returned when the end of input is met before the ending `.
const char* const ErrUnclosedMultiline = "unclosed multiline";

// ErrInvalidMarginChar is returned when the margin doesn’t match the start of multiline margin.
const char* const ErrInvalidMarginChar = "invalid margin character";

// ErrMissingNewlineSpecifier is returned when the starting ` of a multiline is not followed by \n or \r\n.
const char* const ErrMissingNewlineSpecifier = "missing \\n or \\r\\n after multiline start";

// ErrInvalidNewlineSpecifier is returned when the starting ` of a multiline is not followed by \n or \r\n.
const char* const ErrInvalidNewlineSpecifier = "expect \\n or \\r\\n after `";

// ErrInvalidMultilineStart is returned when non whitespace or line comments follow the opening `.
const char* const ErrInvalidMultilineStart = "invalid multiline start line";

// ErrUnexpectedCloseBrace is return when } is met where a value is expected.
const char* const ErrUnexpectedCloseBrace = "unexpected }";

// ErrUnexpectedCloseSquare is return when } is met where a value is expected.
const char* const ErrUnexpectedCloseSquare = "unexpected ]";

// ErrInvalidISODateTime is returned when the parsed ISO date time is invalid.
const char* const ErrInvalidISODateTime = "invalid ISO date time";


error_t *newError(pos_t pos, const char* err) {
	error_t *tmp = malloc(sizeof(error_t));
	tmp->pos = pos;
	tmp->err = err;
	return tmp;
}

// ----------------------------------------------------------------------------------------------------------------------------------------
// Tokenizer
// ----------------------------------------------------------------------------------------------------------------------------------------

// whitespace return the byte length of the white space in front of p. 
int whitespace(slice_t p) {
	if (p.l == 0)
		return 0; 
	if (p.p[0] == ' ' || p.p[0] == '\t')
		return 1;
	if (p.l > 1 && (byte)p.p[0] == 0xC2 && (byte)p.p[1] == 0xA0)
		return 2;
	return 0;
}

// newline return the byte length of the newline in front of p.
int newline(slice_t p) {
	if (p.l == 0)
		return 0; 
	if (p.p[0] == '\n')
		return 1;
	if (p.l > 1 && p.p[0] == '\r' && p.p[1] == '\n')
		return 2;
	return 0;
}

// popBytes removes n bytes from the front of p. Use popNewline
// insteal when e.p starts with a newline.
void popBytes(engine_t *e, int n) {
	e->p.p += n;
	e->p.l -= n;
	e->pos.b += n;
}

// popNewline returns false if there is no newline in front of p, 
// otherwise it return true after removing the newline.
bool popNewline(engine_t *e) {
	int n = newline(e->p);
	if (n == 0)
		return false;
	popBytes(e, n);
	e->pos.s = e->pos.b;
	e->pos.l++;
	return true;
}

const byte s0 = 0x00; // invalid character (e.g. control characters)
const byte s1 = 0x01; // valid characters (printable ascii characters)
const byte s2 = 0x12; // rule 1, 2 byte long
const byte s3 = 0x23; // rule 2, 3 byte long
const byte s4 = 0x13; // rule 1, 3 byte long
const byte s5 = 0x33; // rule 3, 3 byqte long
const byte s6 = 0x44; // rule 4, 4 byte long
const byte s7 = 0x14; // rule 1, 4 byte long
const byte s8 = 0x54; // rule 5, 4 byte long

// All control characters except \t are invalid.
// skipChar and readChar return false when the char is \n or \n.
// All valid utf8 character is valid.
const byte utf8Table[256] = {
	s0, s0, s0, s0, s0, s0, s0, s0, s0, s1, s0, s0, s0, s0, s0, s0, // 00
	s0, s0, s0, s0, s0, s0, s0, s0, s0, s0, s0, s0, s0, s0, s0, s0, // 10
	s1, s1, s1, s1, s1, s1, s1, s1, s1, s1, s1, s1, s1, s1, s1, s1, // 20
	s1, s1, s1, s1, s1, s1, s1, s1, s1, s1, s1, s1, s1, s1, s1, s1, // 30
	s1, s1, s1, s1, s1, s1, s1, s1, s1, s1, s1, s1, s1, s1, s1, s1, // 40
	s1, s1, s1, s1, s1, s1, s1, s1, s1, s1, s1, s1, s1, s1, s1, s1, // 50
	s1, s1, s1, s1, s1, s1, s1, s1, s1, s1, s1, s1, s1, s1, s1, s1, // 60
	s1, s1, s1, s1, s1, s1, s1, s1, s1, s1, s1, s1, s1, s1, s1, s1, // 70
	s0, s0, s0, s0, s0, s0, s0, s0, s0, s0, s0, s0, s0, s0, s0, s0, // 80
	s0, s0, s0, s0, s0, s0, s0, s0, s0, s0, s0, s0, s0, s0, s0, s0, // 90
	s0, s0, s0, s0, s0, s0, s0, s0, s0, s0, s0, s0, s0, s0, s0, s0, // A0
	s0, s0, s0, s0, s0, s0, s0, s0, s0, s0, s0, s0, s0, s0, s0, s0, // B0
	s0, s0, s2, s2, s2, s2, s2, s2, s2, s2, s2, s2, s2, s2, s2, s2, // C0
	s2, s2, s2, s2, s2, s2, s2, s2, s2, s2, s2, s2, s2, s2, s2, s2, // D0
	s3, s4, s4, s4, s4, s4, s4, s4, s4, s4, s4, s4, s4, s5, s4, s4, // E0
	s6, s7, s7, s7, s8, s0, s0, s0, s0, s0, s0, s0, s0, s0, s0, s0, // F0
};

const byte utf8lo = (byte)0x80;
const byte utf8hi = (byte)0xBF;

const byte utf8Range[16] = {
	0, 0,
	utf8lo, utf8hi,
	0xA0, utf8hi,
	utf8lo, 0x9F,
	0x90, utf8hi,
	utf8lo, 0x8F,
};

// charX requires that x == s0 || x >= s2.
error_t* qcharX(engine_t *e, byte x, int* nOut) {
 	if (x == s0)
		return newError(e->pos, ErrInvalidChar);
	
	int n = (int)(x & 0xF);
	if (n > e->p.l) {
		return newError(e->pos, ErrTruncatedChar);
	}
	byte b2 = (byte)e->p.p[1];
	byte r = (x >> 4) << 1;
	if (b2 < utf8Range[r] || b2 > utf8Range[r+1]) {
		return newError(e->pos, ErrInvalidChar);
	}
	if (n >= 3) {
		if (((byte)e->p.p[2]) < utf8lo || e->p.p[2] > utf8hi) {
			return newError(e->pos, ErrInvalidChar);
		}
		if (n == 4) {
			if (((byte)e->p.p[3]) < utf8lo || e->p.p[3] > utf8hi) {
				return newError(e->pos, ErrInvalidChar);
			}
		}
	}
	*nOut = n;
	return NULL;
}

// char returns the byte length of the char in front of tk.p and nil,
// otherwise it returns 0 and the error which can be ErrEndOfInput,
// ErrInvalidChar or ErrTruncatedChar. The char is not popped.
error_t* qchar(engine_t *e, int* n) {
	*n = 0;
	if (e->p.l == 0) {
		return NULL;
	}
	byte x = utf8Table[(byte)e->p.p[0]];
	if (x == s1) {
		*n = 1;
		return NULL;
	}
	// Called by char for mid-stack inlining.
	return qcharX(e, x, n);
}

// column return the number of utf8 chars in p. It requires that p contains
// a sequence of valid utf8 encoded chars.
int column(slice_t p) {
	assert(p.p != NULL);
	assert(p.l >= 0);
	int cnt = 0;
	while (p.l > 0) {
		int n = (int)(utf8Table[(byte)p.p[0]] & 0xF);
		if (n == 0 || n > p.l)
			break;
		p.p += n;
		p.l -= n;
		cnt++;
	}
	return cnt;
}

// skipRestOfLine pops all characters until an error occurs, a newline is met, or the
// end of input is met. In the later case no error is returned.
error_t* skipRestOfLine(engine_t *e) {
	for (;;) {
		if (popNewline(e) || e->p.l == 0) {
			return NULL;
		}
		int n; 
		error_t *err = qchar(e, &n);
		if (err != NULL) {
			return err;
		}
		popBytes(e, n);
	}
}

// skipLineComment return true and nil error if it successfully skipped #... or //... comments
// including the newline or the end of input is reached. Otherwise return false with the error.
error_t* skipLineComment(engine_t *e, bool *out) {
	*out = false;
	if (e->p.l == 0)
		return NULL;
	if (e->p.p[0] == '#' || (e->p.p[0] == '/' && e->p.l >= 2 && e->p.p[1] == '/')) {
		error_t* err = skipRestOfLine(e);
		if (err == NULL)
			*out = true;
		return err;
	}
	return NULL;
}

// skipMultilineComment return false and nil when tk.p is not the start of a
// multiline comment. Return true and nil if successfully skipped a /*...*/ comment.
// Otherwise it returns false and an error.
error_t* skipMultilineComment(engine_t *e, bool *out) {
	*out = false;
	if (e->p.l == 0 || e->p.p[0] != '/' || e->p.l < 2 || e->p.p[1] != '*') {
		return NULL;
	}
	pos_t startPos = e->pos;
	popBytes(e, 2);
	for (;;) {
		if (e->p.l == 0) {
			return newError(startPos, ErrUnclosedSlashStarComment);
		}
		if (e->p.p[0] == '*' && e->p.l >= 2 && e->p.p[1] == '/') {
			popBytes(e, 2);
			*out = true;
			return NULL;
		}
		if (popNewline(e)) {
			continue;
		}
		if (e->p.p[0] < 0x20) { // control characters are valid
			popBytes(e, 1);
			continue;
		}
		int n;
		error_t* err = qchar(e, &n);
		if (err != NULL) {
			return err;
		}
		popBytes(e, n);
	}
}

// skips all whitespace characters.
void skipWhitespaces(engine_t *e) {
	for (int n = whitespace(e->p); n != 0; n = whitespace(e->p)) {
		popBytes(e, n);
	}
}

// doubleQuotedString tries to parse a double quoted string. It returns nil
// if there is no double quoted string in front of tk.p. Requires tk is not
// done an dk.p is not empty.
error_t* doubleQuotedString(engine_t *e, slice_t *out) {
	out->p = NULL;
	out->l = 0;
	pos_t startPos = e->pos;
	if (e->p.l == 0 || e->p.p[0] != '"') {
		return NULL;
	}
	popBytes(e, 1);
	for (;;) {
		if (e->p.l == 0) {
			return newError(startPos, ErrUnclosedDoubleQuoteString);
		}
		if (e->p.p[0] == '\\' && e->p.l > 1 && e->p.p[1] == '"') {
			popBytes(e, 2);
			continue;
		}
		if (e->p.p[0] == '"') {
			popBytes(e, 1);
			out->p = e->in+startPos.b;
			out->l = e->pos.b - startPos.b;
			return NULL;
		}
		if (newline(e->p) != 0) {
			return newError(startPos, ErrNewlineInDoubleQuoteString);
		}
		int n;
		error_t *err = qchar(e, &n);
		if (err != NULL) {
			return err;
		}
		popBytes(e, n);
	}
}

// singleQuotedString tries to parse a singgle quoted string. It returns nil
// if there is no single quoted string in front of tk.p. Requires tk is not
// done an dk.p is not empty.
error_t* singleQuotedString(engine_t *e, slice_t *out) {
	out->p = NULL;
	out->l = 0;
	pos_t startPos = e->pos;
	if (e->p.l == 0 || e->p.p[0] != '\'') {
		return NULL;
	}
	popBytes(e, 1);
	for (;;) {
		if (e->p.l == 0)  {
			return newError(startPos, ErrUnclosedSingleQuoteString);
		}
		if (e->p.p[0] == '\\' && e->p.l >= 2 && e->p.p[1] == '\'') {
			popBytes(e, 2);
			continue;
		}
		if (e->p.p[0] == '\'') {
			popBytes(e, 1);
			out->p = e->in+startPos.b;
			out->l = e->pos.b - startPos.b;
			return NULL;
		}
		if (newline(e->p) != 0) {
			return newError(startPos, ErrNewlineInSingleQuoteString);
		}
		int n;
		error_t *err = qchar(e, &n);
		if (err != NULL) {
			return err;
		}
		popBytes(e, n);
	}
}

int parseISODateTimeLiteral(slice_t);

// lenISODateTime is called when parsing a quoteless string and e->p.p[0] == ':'. It test
// if the : bolongs to an ISO date time. If no, it returns 0, otherwise it returns the offset
// to the first byte that doesn’t belong the the ISO date time.
int lenISODateTime(engine_t *e) {
	if (e->p.p[0] == ':' && e->pos.b >= 13) {
		int n = parseISODateTimeLiteral((slice_t){e->p.p-13, e->p.l+13});
		if (n > 13)
			return n - 13;
	}
	return 0;
}

// quotelessString include any valid characters until any of
// , { } [ ] : \n \r\n // /*, the end of input or an error is met.
// The : belonging to an ISO date time doesn’t terminate the 
// quoteless string. 
// The quoteless string is right trimmed of whitespace characters.
// It return nil, nil, when the quoteles string is empty.
error_t* quotelessString(engine_t* e, slice_t *out) {
	const byte stopByte[256] = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, // 00  \n \r
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 10
		0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, // 20  # , /
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, // 30  :
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 40
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, // 50 [ ]
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 60
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, // 70 { }
	};
	out->p = NULL;
	out->l = 0;
	pos_t startPos = e->pos;
	int endIdx = startPos.b;
	for (;;) {
		if (e->p.l == 0)
			break;
		if (whitespace(e->p) != 0) {
			skipWhitespaces(e);
			continue;
		}
		if (stopByte[(byte)e->p.p[0]] != 0) {
			if ((e->p.p[0] == '/' && e->p.l > 1 && (e->p.p[1] == '/' || e->p.p[1] == '*')) ||
				newline(e->p) != 0 || (e->p.p[0] != '\r' && e->p.p[0] != '/')) {
				// we met any of , : { } [ ] # \n \r\n // /*
				int n = lenISODateTime(e);
				if (n == 0)
					break;
				popBytes(e, n);
				endIdx = e->pos.b;
				continue;
			}
		}
		int n;
		error_t *err = qchar(e, &n);
		if (err != NULL) {
			return err;
		}
		popBytes(e, n);
		endIdx = e->pos.b;
	}
	if (startPos.b == endIdx) {
		return NULL;
	}
	out->p = e->in + startPos.b;
	out->l = endIdx - startPos.b;
	return NULL;
}

const enum tokenTag_t tkTagTable[256] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 00
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 10
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, tagComma, 0, 0, 0, // 20
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, tagColon, 0, 0, 0, 0, 0, // 30
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 40
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, tagOpenSquare, 0, tagCloseSquare, 0, 0, // 50
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 60
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, tagOpenBrace, 0, tagCloseBrace, 0, 0, // 70
};

// delimiter return the delimiter tag or tagUnknown.
// When the tag is different from tagUnknown, the delimiter is popped
// from e->p. Requires that e->p is not empty.
enum tokenTag_t delimiter(engine_t *e) {
	enum tokenTag_t tag = tkTagTable[(byte)e->p.p[0]];
	if (tag != tagUnknown) {
		popBytes(e, 1);
	}
	return tag;
}

// skipSpaces skips whitespaces, comments, newlines, and return
// error if any. Return nil if the end of input is reached.
error_t* skipSpaces(engine_t *e) {
	error_t *err = NULL;
	bool ok;
	while (err == NULL && e->p.l > 0) {
		skipWhitespaces(e);
		err = skipLineComment(e, &ok);
		if (ok || err != NULL)
			continue;
		err = skipMultilineComment(e, &ok);
		if (ok || err != NULL)
			continue;
		if (!popNewline(e))
			break;
	}
	return err;
}

int matchingMarginLength(slice_t margin, slice_t line) {
	int n = margin.l;
	if (line.l < margin.l) {
		n = line.l;
	}
	for (int i = 0; i < n; i++) {
		if (line.p[i] != margin.p[i]) {
			return i;
		}
	}
	return n;
}

int newlineSpecifier(slice_t p) {
	if (p.p[0] == '\\') {
		if (p.l > 1 && p.p[1] == 'n') {
			return 2;
		}
		if (p.l > 3 && p.p[1] == 'r' && p.p[2] == '\\' && p.p[3] == 'n') {
			return 4;
		}
	}
	return 0;
}

// return the index in p of the end of the margin.
int getMargin(slice_t p) {
	int b = 0;
	while (p.l > 0) {
		int n = whitespace(p);
		if (n == 0)
			break;
		p.p += n;
		p.l -= n;
		b += n;
	}
	return b;
}

// multilineString test if e.p starts with a multiline string. If it is,
// it returns the multiline string including the margin and trailing `, and
// pops it from e.p. Otherwise it return nil. It returns a non nil slice of
// lenght 0 if an error occured.
error_t* multilineString(engine_t *e, slice_t *out) {
	out->p = NULL;
	out->l = 0;
	if (e->p.l == 0 || e->p.p[0] != '`') {
		return NULL;
	}
	int b = getMargin((slice_t){e->in+e->pos.s,e->pos.b-e->pos.s}) + e->pos.s;
	if (b != e->pos.b) {
		return newError((pos_t){b, e->pos.s, e->pos.l}, ErrMarginMustBeWhitespaceOnly);
	}
	slice_t margin = {e->in+e->pos.s, e->pos.b-e->pos.s};
	pos_t startPos = e->pos; // for error reporting
	popBytes(e, 1);     // pops starting `
	skipWhitespaces(e);
	if (e->p.l == 0)
		return newError(startPos, ErrMissingNewlineSpecifier);
	int n = newlineSpecifier(e->p);
	if (n == 0) 
		return newError(startPos, ErrInvalidNewlineSpecifier);
	popBytes(e, n);
	skipWhitespaces(e);
	if (!popNewline(e)) {
		bool ok;
		error_t *err = skipLineComment(e, &ok); 
		if (err != NULL)
			return err;
		if (!ok) {
			return newError(startPos, ErrInvalidMultilineStart);
		}
	}
	if (e->p.l == 0)
		return newError(startPos, ErrUnclosedMultiline);
	n = matchingMarginLength(margin, e->p);
	if (n != margin.l)
		return newError((pos_t){e->pos.b + n, e->pos.s, e->pos.l}, ErrInvalidMarginChar);
	popBytes(e, n);
	while (e->p.l > 0) {
		if (popNewline(e)) {
			int n = matchingMarginLength(margin, e->p);
			if (n != margin.l)
				return newError((pos_t){e->pos.b+n, e->pos.s, e->pos.l}, ErrInvalidMarginChar);
			if (n > 0)
				popBytes(e, n);
			continue;
		}
		if (e->p.p[0] < 0x20) {
			popBytes(e,1);
			continue;
		}
		if (e->p.p[0] == '`') {
			popBytes(e, 1);
			if (e->p.l == 0 || e->p.p[0] != '\\') {
				out->p = e->in+startPos.s;
				out->l = e->pos.b-startPos.s;
				return NULL; // we reached the end of the multiline
			}
			continue;
		}
		int n;
		error_t *err = qchar(e, &n);
		if (err != NULL) {
			return err;
		}
		popBytes(e, n);
	}
	return newError(startPos, ErrUnclosedMultiline);
}


// nextToken reads the next token. The token is accessed with the token()
// method. The token is set to an error if an error is detected or the
// end of input is reached.
void nextToken(engine_t *e) {
	if (e->tk.tag == tagError)
		return;
	error_t *err = skipSpaces(e);
	if (err != NULL) {
		e->tk = (token_t){tagError, err->pos, (slice_t){err->err, strlen(err->err)}};
		free(err);
		return;
	}
	pos_t tokenPos = e->pos;
	if (e->p.l == 0) {
		e->tk = (token_t){tagError, e->pos, (slice_t){ErrEndOfInput, strlen(ErrEndOfInput)}};
		return;
	}
	enum tokenTag_t tag = delimiter(e); 
	if (tag != tagUnknown) {
		e->tk = (token_t){tag, tokenPos, (slice_t){NULL, 0}};
		return;
	}
	slice_t s;
	err = doubleQuotedString(e, &s);
	if (err != NULL) {
		e->tk = (token_t){tagError, err->pos, (slice_t){err->err, strlen(err->err)}};
		free(err);
		return;
	}
	if (s.p != NULL) {
		e->tk = (token_t){tagDoubleQuotedString, tokenPos, s};
		return;
	}
	err = singleQuotedString(e, &s);
	if (err != NULL) {
		e->tk = (token_t){tagError, err->pos, (slice_t){err->err, strlen(err->err)}};
		free(err);
		return;
	}
	if (s.p != NULL) {
		e->tk = (token_t){tagSingleQuotedString, tokenPos, s};
		return;
	}
	err = multilineString(e, &s);
	if (err != NULL) {
		e->tk = (token_t){tagError, err->pos, (slice_t){err->err, strlen(err->err)}};
		free(err);
		return;
	}
	if (s.p != NULL) {
		e->tk = (token_t){tagMultilineString, tokenPos, s};
		return;
	}
	err = quotelessString(e, &s);
	if (err != NULL) {
		e->tk = (token_t){tagError, err->pos, (slice_t){err->err, strlen(err->err)}};
		free(err);
		return;
	}
	if (s.p != NULL) {
		e->tk = (token_t){tagQuotelessString, tokenPos, s};
		return;
	}
	assert(false); // we should never reach this line
}


// ----------------------------------------------------------------------------------------------------------------------------------------
// Engine
// ----------------------------------------------------------------------------------------------------------------------------------------

// done return true when an error is met. The error may be that the end of 
// input is reached.
bool done(engine_t *e) {
	return e->tk.tag == tagError;
}

void setErrorAndPos(engine_t *e, const char *err, pos_t pos) {
	e->tk = (token_t){tagError, pos, (slice_t){err, strlen(err)}};
}

void setError(engine_t *e, const char *err) {
	setErrorAndPos(e, err, e->pos);
}

// ----------------------------------------------------------------------------------------------------------------------------------------
// Output
// ----------------------------------------------------------------------------------------------------------------------------------------

void outputInit(engine_t *e) {
	e->out.buf = NULL;
	e->out.len = e->out.cap = 0;
}

// outputGrow grows the output buffer without modifying len.
void outputGrow(engine_t *e) {
	if (e->out.buf == NULL) {
		e->out.cap = 1024;
		e->out.buf = malloc(e->out.cap);
		e->out.len = 0;
	}
	int newCap = e->out.cap*2;
	char *tmp = malloc(newCap);
	memcpy(tmp, e->out.buf, e->out.cap);
	free(e->out.buf);
	e->out.buf = tmp;
	e->out.cap = newCap;
}

// outBufByte appends a byte to the output buffer.
void outputByte(engine_t *e, char c) {
	assert(e->out.len <= e->out.cap);
	if (e->out.len == e->out.cap)
		outputGrow(e);
	e->out.buf[e->out.len++] = c;
}

// outBufString appends a string to the output buffer.
void outputString(engine_t *e, const char *s) {
	assert(e->out.len <=e->out.cap);
	int l = strlen(s);
	while (e->out.len + l > e->out.cap)
		outputGrow(e);
	memcpy(e->out.buf+e->out.len, s, l);
	e->out.len += l;
}

// outBufGet returns the output buffer content. 
// On return, the output buffer is empty.
char* outputGet(engine_t *e) {
	char *tmp = realloc(e->out.buf, e->out.len);
	e->out.buf = NULL;
	e->out.len = 0;
	e->out.cap = 0;
	return tmp;
}

// outBufReset reset the output buffer to an empty state.
void outputReset(engine_t *e) {
	e->out.len = 0;
}


bool isHexDigit(byte v);

void outputDoubleQuotedString(engine_t *e) {
	char c;
	slice_t str = e->tk.val;
	outputByte(e, '"');
	for (int i = 1; i < str.l-1; i++) {
		switch (str.p[i]) {
		case '/':
			if (str.p[i-1] == '<')
				outputByte(e, '\\');
			break;
		case '\t':
			outputByte(e, '\\');
			outputByte(e, 't');
			continue;
		case '\\':
			c = str.p[i+1];
			if (c != 't' && c != 'n' && c != 'r' && c != 'f' && c != 'b' && c != '/' && c != '\\' && c != '"' &&
				!(c == 'u' && str.l >= i+6 && isHexDigit(str.p[i+2]) && isHexDigit(str.p[i+3]) && isHexDigit(str.p[i+5]) && isHexDigit(str.p[i+5]))) {
				setErrorAndPos(e, ErrInvalidEscapeSequence, (pos_t){e->tk.pos.b+i, e->tk.pos.s, e->tk.pos.l});
				return;
			}
			break;
		}
		outputByte(e, str.p[i]);
	}
	outputByte(e, '"');
}

void outputSingleQuotedString(engine_t *e) {
	char c;
	slice_t str = e->tk.val;
	outputByte(e, '"');
	for (int i = 1; i < str.l-1; i++) {
		switch (str.p[i]) {
		case '/':
			if (str.p[i-1] == '<') {
				outputByte(e, '\\');
			}
			break;
		case '\t':
			outputByte(e, '\\');
			outputByte(e, 't');
			continue;
		case '\\':
			c = str.p[i+1];
			if (c != 't' && c != 'n' && c != 'r' && c != 'f' && c != 'b' && c != '/' && c != '\\' && c != '\'' &&
				!(c == 'u' && str.l >= i+6 && isHexDigit(str.p[i+2]) && isHexDigit(str.p[i+3]) && isHexDigit(str.p[i+5]) && isHexDigit(str.p[i+5]))) {
				setErrorAndPos(e, ErrInvalidEscapeSequence, (pos_t){e->tk.pos.b+i, e->tk.pos.s, e->tk.pos.l});
				return;
			}
			if (c == '\'')
				continue;
			break;
		case '"':
			outputByte(e, '\\');
			break;
		}
		outputByte(e, str.p[i]);
	}
	outputByte(e, '"');
}

void outputQuotelessString(engine_t *e) {
	slice_t str = e->tk.val;
	outputByte(e, '"');
	for (int i = 0; i < str.l; i++) {
		switch (str.p[i]) {
		case '"':
			outputByte(e, '\\');
			break;
		case '\t':
			outputByte(e, '\\');
			outputByte(e, 't');
			continue;
		case '/':
			if (i > 0 && str.p[i-1] == '<') {
				outputByte(e, '\\');
			}
			break;
		case '\\':
			outputByte(e, '\\');
			break;
		}
		outputByte(e, str.p[i]);
	}
	outputByte(e, '"');
}

void outputMultilineString(engine_t *e) {
	slice_t str = e->tk.val;
	int p = 0;
	while (str.p[p] != '`')
		p++;
	slice_t margin = str;
	margin.l = p;
	str.p = str.p+p+1;
	str.l = str.l-p-1;
	for (int n = whitespace(str); n > 0; n = whitespace(str)) {
		str.p = str.p+n;
		str.l = str.l-n;
	}
	str.p++;
	str.l--;
	const char* nl;
	if (str.p[0] == 'n') {
		nl = "\\n";
		str.p++;
		str.l--;
	} else {
		nl = "\\r\\n";
		str.p += 3;
		str.l -= 3;
	}
	while (str.p[0] != '\n') {
		str.p++;
		str.l--;
	}
	// skip \n with margin of first line, and drop closing `
	str.p = str.p+1+margin.l;
	str.l = str.l-2-margin.l;
	outputByte(e,'"');
	while (str.l > 0) {
		int n = newline(str);
		if (n != 0) {
			outputString(e, nl);
			str.p += n+margin.l;
			str.l -= n+margin.l;
			continue;
		}
		if (str.p[0] < 0x20) {
			char tmp[256];
			switch (str.p[0]) {
			case '\b':
				outputString(e, "\\b");
				break;
			case '\t':
				outputString(e, "\\t");
				break;
			case '\r':
				outputString(e, "\\r");
				break;
			case '\f':
				outputString(e, "\\f");
				break;
			default:
				sprintf(tmp, "\\u00%0X", str.p[0]);
				outputString(e, tmp);
				break;
			}
			str.p++;
			str.l--;
			continue;
		}
		if (str.p[0] == '<') {
			outputByte(e, '<');
			if (str.l > 1 && str.p[1] == '/')
				outputByte(e, '\\');
			str.p++;
			str.l--;
			continue;
		}
		if (str.p[0] == '"') {
			outputByte(e, '\\');
			outputByte(e, '"');
			str.p++;
			str.l--;
			continue;
		}
		if (str.p[0] == '`' && str.l > 1 && str.p[1] == '\\') {
			outputByte(e, '`');
			str.p += 2;
			str.l -= 2;
			continue;
		}
		if (str.p[0] == '\\') {
			outputByte(e, '\\');
			outputByte(e, '\\');
			str.p++;
			str.l--;
			continue;
		}
		outputByte(e, str.p[0]);
		str.p++;
		str.l--;
	}
	outputByte(e, '"');
}




// ----------------------------------------------------------------------------------------------------------------------------------------
// NumTokenizer
// ----------------------------------------------------------------------------------------------------------------------------------------

// The numTokenizer is used only for numbers and arithmetic operations.
// The numTokenizer input is a quoteless string. The output are the operators
// "()+-*/%^|&~" and int and float values, or an error. The binary and
// hexadecimal numbers are converted into int by the tokenizer.
//
//
// Expression evaluation:
// int are implicitely cast into float when combined with a float in a
// the operations "+-*/". The operations "^~|&%" require two ints and
// output an int.

typedef struct {
	enum tokenTag_t tag;
	int pos; 
	union {
		int64_t i;
		double f;
		const char* e;
	} val;
} numToken_t;


typedef struct {
	slice_t    in;  // input expression to parse
	slice_t    p;   // expression left to parse
	int        pos; // position in b of the first byte of p
	numToken_t tk;  // the last token
} numEngine_t;


bool numDone(numEngine_t *e) {
	return e->tk.tag == tagError;
}

void numPopBytes(numEngine_t *e, int n) {
	assert(n <= e->p.l);
	e->p.p += n;
	e->p.l -= n;
	e->pos += n;
}

enum tokenTag_t tkOpTable[256] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 00
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 10
	0, 0, 0, 0, 0, tagModulo, tagAnd, 0, tagOpenParen, // 20
	tagCloseParen, tagMultiplication, tagPlus, 0, tagMinus, 0, tagDivision, // 29
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 30
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 40
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, tagXor, 0, // 50
	0, 0, 0, 0, tagDays, 0, 0, 0, tagHours, 0, 0, 0, 0, tagMinutes, 0, 0, // 60
	0, 0, 0, tagSeconds, 0, 0, 0, tagWeeks, 0, 0, 0, 0, tagOr, 0, tagInverse, 0, // 70
};

// nextOperator returns true and pops the operator if tk.p start with
// an operatore. Otherwise return false.
bool nextOperator(numEngine_t *e) {
	enum tokenTag_t x = tkOpTable[(byte)e->p.p[0]];
	if (x == tagUnknown)
		return false;
	e->tk = (numToken_t){x, e->pos, {.i=0}};
	numPopBytes(e, 1);
	return true;
}

// inRange return true if lo <= v <= hi.
bool inRange(byte v, byte lo, byte hi) {
	return (unsigned char)(v-lo) <= (unsigned char)(hi-lo);
}

// skips n bytes in front of v and the optional underscore.
// returns the number of bytes skipped and v trimmed by this number,
// or -1 and NULL out value if the of v is reached.
int skipHeaderAndOptionalUnderscore(int n, slice_t v, slice_t *out) {
	out->p = NULL;
	out->l = 0;
	if (n >= v.l) {
		return -1;
	}
	v.p += n;
	v.l -=n;
	if (v.p[0] == '_') {
		n++;
		v.p++;
		v.l--;
		if (v.l == 0) {
			return -1;
		}
	}
	*out = v;
	return n;
}

bool isBinDigit(byte v) {
	return v == '0' || v == '1';
}

// return the number of bytes parsed or 0 if not valid digits
int parseBinDigits(slice_t v) {
	if (!isBinDigit(v.p[0])) {
		return 0;
	}
	for (int p = 1; p < v.l; p++) {
		if (v.p[p] == '_') {
			p++;
			if (p == v.l)
				return -1;
		}
		if (!isBinDigit(v.p[p])) {
			if (v.p[p-1] == '_') {
				return -1;
			}
			return p;
		}
	}
	return v.l;
}

// return 0 if not literal, -1 if invalid literal, n when valid and length is n bytes.
int parseBinLiteral(slice_t v) {
	if (v.l < 2 || v.p[0] != '0' || (v.p[1]&(byte)(0xDF)) != 'B')
		return 0;
	int n = skipHeaderAndOptionalUnderscore(2, v, &v);
	if (n >= 0) {
		int p = parseBinDigits(v);
		if (p > 0)
			return n + p;
	}
	return -1;
}

// return the value which is in the range 0 to MAX_INT, or -1 if overflows.
int64_t decodeBinLiteral(slice_t v) {
	uint64_t val = 0;
	v.p += 2;
	v.l -= 2;
	for (int p = 0; p < v.l; p++) {
		if (v.p[p] == '_')
			continue;
		if ((val&0x8000000000000000) != 0) {
			return -1;
		}
		val = val << 1;
		if (v.p[p] == '1')
			val |= 1;
	}
	if ((val&0x8000000000000000) != 0)
		return -1;
	return (int64_t)val;
}


bool nextBinValue(numEngine_t *e) {
	int n = parseBinLiteral(e->p);
	if (n == 0)
		return false;
	if (n < 0) {
		e->tk = (numToken_t){tagError, e->pos, {.e=ErrInvalidBinaryNumber}};
		return true;
	}
	int64_t val = decodeBinLiteral((slice_t){e->p.p, n});
	if (val < 0) {
		e->tk = (numToken_t){tagError, e->pos, {.e=ErrNumberOverflow}};
		return true;
	}
	e->tk = (numToken_t){tagIntegerVal,e->pos, {.i=val}};
	numPopBytes(e, n);
	return true;
}


bool isOctDigit(byte v) {
	return inRange(v, '0', '7');
}

// return the number of bytes parsed or 0 if not valid digits
int parseOctDigits(slice_t v) {
	int l = v.l;
	if (l == 0 || !isOctDigit(v.p[0]))
		return 0;
	for (int p = 1; p < l; p++) {
		if (v.p[p] == '_') {
			p++;
			if (p == l)
				return -1;
		}
		if (!isOctDigit(v.p[p])) {
			if (v.p[p-1] == '_')
				return -1;
			return p;
		}
	}
	return l;
}

// return 0 if not literal, -1 if invalid literal, n when valid and length is n bytes.
int parseOctLiteral(slice_t v) {
	if (v.l < 1 || v.p[0] != '0')
		return 0;
	if (v.l >= 2 && (v.p[1]&(byte)(0xDF)) == 'O') {
		int n = skipHeaderAndOptionalUnderscore(2, v, &v);
		if (n >= 0) {
			int p = parseOctDigits(v);
			if (p > 0)
				return n + p;
		}
		return -1;
	}
	// a 0 at end of input or followed by anything different from _ and an oct digit
	// is not an octal number. It’s thus not invalid.
	if (v.l < 2 || (v.p[1] != '_' && !isOctDigit(v.p[1])))
		return 0;
	int n = skipHeaderAndOptionalUnderscore(1, v, &v);
	if (n >= 0) {
		int p = parseOctDigits(v);
		if (p > 0)
			return n + p;
	}
	return -1;
}

// return the value which is in the range 0 to MAX_INT, or -1 if overflows.
int decodeOctLiteral(slice_t v) {
	uint64_t val = 0;
	if ((v.p[1]&(byte)(0xDF)) == 'O') {
		v.p += 2;
		v.l -= 2;
	} else {
		v.p++;
		v.l--;
	}
	for (int p = 0; p < v.l; p++) {
		if (v.p[p] == '_')
			continue;
		if ((val&0xF000000000000000) != 0)
			return -1;
		val = val<<3 | (uint64_t)(v.p[p]-'0');
	}
	return (int64_t)val;
}

bool nextOctValue(numEngine_t *e) {
	int n = parseOctLiteral(e->p);
	if (n == 0)
		return false;
	if (n < 0) {
		e->tk = (numToken_t){tagError, e->pos, {.e=ErrInvalidOctalNumber}};
		return true;
	}
	int64_t val = decodeOctLiteral((slice_t){e->p.p,n});
	if (val < 0) {
		e->tk = (numToken_t){tagError, e->pos, {.e=ErrNumberOverflow}};
		return true;
	}
	e->tk = (numToken_t){tagIntegerVal, e->pos, {.i=val}};
	numPopBytes(e, n);
	return true;
}

bool isIntDigit(byte v) {
	return inRange(v, '0', '9');
}

// return the number of bytes parsed or 0 if not valid digits
int parseIntDigits(slice_t v) {
	int l = v.l;
	if (l == 0 || !isIntDigit(v.p[0])) 
		return 0;
	for (int p = 1; p < l; p++) {
		if (v.p[p] == '_') {
			p++;
			if (p == l)
				return -1;
		}
		if (!isIntDigit(v.p[p])) {
			if (v.p[p-1] == '_') {
				return -1;
			}
			return p;
		}
	}
	return l;
}

// return 0 if not literal or n when valid and length is n bytes.
int parseIntLiteral(slice_t v) {
	if (inRange(v.p[0], '1', '9'))
		return parseIntDigits(v);
	if (v.p[0] != '0')
		return 0;
	if (v.l > 1 && (v.p[1] == '_' || isIntDigit(v.p[1])))
		return -1;
	return 1;
}

// return the value which is in the range 0 to MAX_INT, or -1 if overflows.
int64_t decodeIntLiteral(slice_t v) {
	uint64_t val = 0;
	for (int p = 0; p < v.l; p++) {
		if (v.p[p] == '_')
			continue;
		if (val > 0x1999999999999999)
			return -1;
		val = val*10 + (uint64_t)(v.p[p]-'0');
	}
	if ((val&0x8000000000000000) != 0)
		return -1;
	return (int64_t)val;
}

bool nextIntValue(numEngine_t *e) {
	int n = parseIntLiteral(e->p); // n is allways >= 0
	if (n == 0)
		return false;
	if (n < 0) {
		e->tk = (numToken_t){tagError, e->pos, {.e=ErrInvalidIntegerNumber}};
		return true;
	}
	int64_t val = decodeIntLiteral((slice_t){e->p.p, n});
	if (val < 0) {
		e->tk = (numToken_t){tagError, e->pos, {.e=ErrNumberOverflow}};
		return true;
	}
	e->tk = (numToken_t){tagIntegerVal, e->pos, {.i=val}};
	numPopBytes(e, n);
	return true;
}

bool isHexDigit(byte v) {
	return isIntDigit(v) || inRange(v&(byte)(0xDF), 'A', 'F');
}

// return the number of bytes parsed or 0 if not valid digits
int parseHexDigits(slice_t v) {
	int l = v.l;
	if (l == 0 || !isHexDigit(v.p[0]))
		return 0;
	for (int p = 1; p < l; p++) {
		if (v.p[p] == '_') {
			p++;
			if (p == l) 
				return -1;
		}
		if (!isHexDigit(v.p[p])) {
			if (v.p[p-1] == '_')
				return -1;
			return p;
		}
	}
	return l;
}

// return 0 if not literal, -1 if invalid literal, n when valid and length is n bytes.
int parseHexLiteral(slice_t v) {
	if (v.l < 2 || v.p[0] != '0' || (v.p[1]&(byte)(0xDF)) != 'X')
		return 0;
	int n = skipHeaderAndOptionalUnderscore(2, v, &v);
	if (n >= 0) {
		int p = parseHexDigits(v);
		if (p > 0)
			return n + p;
	}
	return -1;
}

// return the value which is in the range 0 to MAX_INT, or -1 if overflows.
int64_t decodeHexLiteral(slice_t v) {
	uint64_t val = 0;
	v.p += 2;
	v.l -= 2;
	for (int p = 0; p < v.l; p++) {
		if (v.p[p] == '_')
			continue;
		if ((val&0xF000000000000000) != 0)
			return -1;
		if (inRange(v.p[p], '0', '9')) {
			val = val<<4 | (uint64_t)(v.p[p]-'0');
			continue;
		} 
		val = val<<4 | ((uint64_t)((v.p[p] & (byte)(0xDF))-'A') + 10);
	}
	if ((val&0x8000000000000000) != 0)
		return -1;
	return (int64_t)val;
}

bool nextHexValue(numEngine_t *e) {
	int n = parseHexLiteral(e->p);
	if (n == 0)
		return false;
	if (n < 0) {
		e->tk = (numToken_t){tagError,e->pos, {.e=ErrInvalidHexadecimalNumber}};
		return true;
	}
	int64_t val = decodeHexLiteral((slice_t){e->p.p, n});
	if (val < 0) {
		e->tk = (numToken_t){tagError,e->pos, {.e=ErrNumberOverflow}};
		return true;
	}
	e->tk = (numToken_t){tagIntegerVal, e->pos, {.i=val}};
	numPopBytes(e, n);
	return true;
}

// return 0 if not exponent, -1 if invalid exponent, n when valid and length is n bytes.
int parseExponent(slice_t v) {
	if (v.l == 0 || (v.p[0]&(byte)(0xDF)) != 'E')
		return 0;
	int n = 1;
	v.p++;
	v.l--;
	if (v.l == 0)
		return -1;
	if (v.p[0] == '+' || v.p[0] == '-') {
		n++;
		v.p++;
		v.l--;
		if (v.l == 0)
			return -1;
	}
	int p = parseIntDigits(v);
	if (p > 0)
		return n + p;
	return -1;
}

// return 0 if not literal, -1 if invalid literal, n when valid and length is n bytes.
int parseDecLiteral(slice_t v) {
	int p = parseIntDigits(v);
	if (p < 0)
		return 0;
	if (p == 0) {
		// numbers must be of the form .123[e[+/-]145]
		if (v.p[0] != '.' || v.l < 2)
			return 0;
		v.p++;
		v.l--;
		p = parseIntDigits(v);
		if (p < 0)
			return -1;
		if (p == 0) {
			if (v.l > 0 && (v.p[0] == '_' || (v.p[0]&(byte)(0xDF)) == 'E'))
				return -1;
			return 0;
		}
		v.p += p;
		v.l -= p;
		int q = parseExponent(v);
		if (q < 0)
			return -1;
		return 1 + p + q;
	}
	//  numbers must be of the form 123e[+/-]145 or 123.456[e[+/-]789]
	int n = p;
	v.p += p;
	v.l -= p;
	int q = parseExponent(v);
	if (q < 0)
		return -1;
	if (q > 0)
		return p + q;
	// numbers must be of the form 123.456[e[+/-]789]
	if (v.l == 0)
		return 0;
	if (v.p[0] != '.')
		return 0; // not invalid, but not a decimal number
	n++;
	v.p++;
	v.l--;
	q = parseIntDigits(v);
	if (q > 0) {
		n += q;
		v.p += q;
		v.l -= q;
	}
	if (q < 0)
		return -1;
	p = parseExponent(v);
	if (p < 0)
		return -1;
	n += p;
	if (v.l > p && v.p[p] == '_')
		return -1;
	return n;
}

// return the value which is in the range 0 to MAX_INT, or -1 if overflows.
double decodeDecLiteral(slice_t v) {
	if (v.l > 255)
		return -1;
	char buf[256];
	memcpy(buf, v.p, v.l);
	buf[v.l] = '\0';
	char *eptr;
	double x = strtod(buf, &eptr);
	if (x == 0 && errno == ERANGE) 
		return -1;
	return x;
}

bool nextDecValue(numEngine_t *e) {
	int n = parseDecLiteral(e->p);
	if (n == 0)
		return false;
	if (n < 0) {
		e->tk = (numToken_t){tagError, e->pos, {.e=ErrInvalidDecimalNumber}};
		return true;
	}
	double val = decodeDecLiteral((slice_t){e->p.p, n});
	if (val < 0) {
		e->tk = (numToken_t){tagError, e->pos, {.e=ErrInvalidDecimalNumber}};
		return true;
	}
	e->tk = (numToken_t){tagDecimalVal, e->pos, {.f=val}};
	numPopBytes(e, n);
	return true;
}

// see https://fr.wikipedia.org/wiki/ISO_8601 (ex: 1997−07−16T19:20+01:00) RFC3339
int parseISODateTimeLiteral(slice_t v) {
	// must start with date
	if (v.l < 11 || v.p[10] != 'T' || v.p[4] != '-' || v.p[7] != '-' ||
		!isIntDigit(v.p[0]) || !isIntDigit(v.p[1]) || !isIntDigit(v.p[2]) || !isIntDigit(v.p[3]) ||
		!isIntDigit(v.p[5]) || !isIntDigit(v.p[6]) || !isIntDigit(v.p[8]) || !isIntDigit(v.p[9]))
		return 0;
	int n = 11;
	v.l -= 11;
	v.p += 11;
	if (v.l == 0)
		return n;
	// followed by hours and minutes (seconds and rest is optional)
	if (v.l < 5 || v.p[2] != ':' || !isIntDigit(v.p[0]) || !isIntDigit(v.p[1]) || 
		!isIntDigit(v.p[3]) || !isIntDigit(v.p[4]))
		return -1;
	n += 5;
	v.l -= 5;
	v.p += 5;
	if (v.l == 0)
		return n;
	if (v.p[0] == 'Z')
		return n+1;
	if (v.p[0] != ':')
		return n;
	if (v.l < 3 || !isIntDigit(v.p[1]) || !isIntDigit(v.p[2]))
		return -1;
	n += 3;
	v.l -= 3;
	v.p += 3;
	if (v.l == 0)
		return n;
	if (v.p[0] == 'Z')
		return n+1;
	if (v.p[0] != '.' && v.p[0] != '+' && v.p[0] != '-')
		return n;
	// milli or micro seconds
	if (v.p[0] == '.') {
		n++;
		v.l--;
		v.p++;
		int p = 0;
		while (v.l > p && isIntDigit(v.p[p]))
			p++;
		if (p != 6 && p != 3)
			return -1;
		n += p;
		v.l -= p;
		v.p += p;
	}
	if (v.l == 0)
		return n;
	if (v.p[0] == 'Z')
		return n+1;
	if (v.p[0] != '+' && v.p[0] != '-')
		return n;
	// time offset
	n++;
	v.l--;
	v.p++;
	if (v.l < 5 || v.p[2] != ':' || !isIntDigit(v.p[0]) || !isIntDigit(v.p[1]) ||
		!isIntDigit(v.p[3]) || !isIntDigit(v.p[4]))
		return -1;
	return n + 5;
}

typedef struct {
	int y;    /* Year.         [1970-...]   */
	int M;    /* Month.        [1-12]       */
	int d;    /* Day.          [1-31]       */
	int h;    /* Hours.        [0-24]       */
	int m;    /* Minutes.      [0-59]       */
	int s;    /* Seconds.      [0-60]       */
	int ho;   /* Hour offset.  [-15-15]     */
	int mo;   /* Min offset.   [0-59]       */
	double f; /* Frac. of sec. [0-0.999999] */
} ISODateTime_t;


// makeTime converts the decoded ISO date time into UTC time in seconds since 1970-01-01T00:00:00Z. 
double makeTime(ISODateTime_t dt) {
	if (dt.y < 1970 || dt.M < 1 || dt.M >12 || dt.d < 1 || dt.d > 31 || 
		dt.h < 0 || dt.h > 24 || dt.m < 0 || dt.m > 59 || dt.s < 0 || dt.s > 60 || 
		dt.ho < -15 || dt.ho > 15 || dt.mo < 0 || dt.mo > 59 ||
		(dt.h == 24 && dt.m != 0 && dt.s != 0 && dt.f != 0))
		return -1;
	struct tm t;
	memset(&t, 0, sizeof(t));
	t.tm_year = dt.y - 1900;
	t.tm_mon = dt.M - 1;
	t.tm_mday = dt.d;
	t.tm_hour = dt.h;
	t.tm_min = dt.m;
	t.tm_sec = dt.s;
	double v = (double)timegm(&t) + dt.f;
	if (dt.ho < 0)
		v = v - dt.ho*3600 + dt.mo*60;
	else
		v = v - dt.ho*3600 - dt.mo*60;
	return v;
}

// return -1 if decoding failed
double decodeISODateTimeLiteral(slice_t v) {
	if (v.l > 255)
		return -1;
	char buf[256], *p = buf;
	memcpy(buf, v.p, v.l);
	buf[v.l] = '\0';
	ISODateTime_t t;
	memset(&t, 0, sizeof(t));

	int k = sscanf(p, "%d-%d-%dT%d:%d:%d", &t.y, &t.M, &t.d, &t.h, &t.m, &t.s);
	if (k < 3 || k == 4)
		return -1;
	if (k == 3 || k == 5)
		return makeTime(t);
	p += 19;
	if (*p == '\0' || *p == 'Z')
		return makeTime(t);
	if (*p == '.') {
		p++;
		int num;
		k = sscanf(p, "%d", &num);
		if (k != 1)
			return -1;
		if (isIntDigit(p[3])) {
			t.f = ((double)num)/1000000;
			p += 6;
		} else {
			t.f = ((double)num)/1000;
			p += 3;
		}
	}
	if (*p == '\0' || *p == 'Z')
		return makeTime(t);
	k = sscanf(p, "%d:%d", &t.ho, &t.mo);
	if (k != 2)
		return -1;
	return makeTime(t);
}

bool nextISODateTimeValue(numEngine_t *e) {
	int n = parseISODateTimeLiteral(e->p);
	if (n == 0)
		return false;
	if (n < 0) {
		e->tk = (numToken_t){tagError, e->pos, {.e=ErrInvalidISODateTime}};
		return true;
	}
	double val = decodeISODateTimeLiteral((slice_t){e->p.p, n});
	if (val < 0) {
		e->tk = (numToken_t){tagError, e->pos, {.e=ErrInvalidISODateTime}};
		return true;
	}
	e->tk = (numToken_t){tagDecimalVal, e->pos, {.f=val}};
	numPopBytes(e, n);
	return true;
}

void numNextToken(numEngine_t *e) {
	if (e->tk.tag == tagError)
		return;
	while (e->p.l > 0) {
		int n = whitespace(e->p);
		if (n == 0)
			break;
		numPopBytes(e, n); 
	}
	if (e->p.l == 0) {
		e->tk = (numToken_t){tagError, e->pos, {.e=ErrEndOfInput} };
		return;
	}
	if (!nextOperator(e) && !nextISODateTimeValue(e) && !nextBinValue(e) && !nextHexValue(e) &&
		!nextDecValue(e) && !nextOctValue(e) && !nextIntValue(e)) {
			e->tk = (numToken_t){tagError, e->pos, {.e=ErrInvalidNumericExpression}};
	}
}

void numEngineInit(numEngine_t *e, slice_t in) {
	assert(in.p != NULL);
	assert(in.l != 0);
	e->in = in;
	e->p = in;
	e->pos = 0;
	e->tk.tag = tagUnknown;
	e->tk.pos = 0;
	e->tk.val.i = 0;
	numNextToken(e);
}

byte precedenceTable[256] = {
	0, // tagUnknown
	0, // tagError
	0, // tagIntegerVal
	0, // tagDecimalVal
	1, // tagPlus
	1, // tagMinus
	2, // tagMultiplication
	2, // tagDivision
	1, // tagXor
	2, // tagAnd
	1, // tagOr
	1, // tagInverse
	2, // tagModulo
	0, // tagOpenParen
	0, // tagCloseParen
	4, // tagWeeks
	4, // tagDays
	4, // tagHours
	4, // tagMinutes
	4, // tagSeconds
};

const byte highestPrecedence = 4;

// A nudXXX function returns the result of evaluating the expression at the current
// location. We use a numToken to store this result. It can only be an integer, a
// decimal or an error. The pos field value is only meaningful with errors.
typedef numToken_t nudFunc(numEngine_t *e, numToken_t t);

// A ledXXX function is an n-ary operation function. It is given its left operand
// as an argument and may get the right operand if needed from a call to expression().
// It return its result as a numToken. It can only be an integer, a
// decimal or an error. The pos field value is only meaningful with errors.
typedef numToken_t ledFunc(numEngine_t *, numToken_t t, numToken_t left);

numToken_t nudValue(numEngine_t *e, numToken_t t);
numToken_t nudPlus(numEngine_t *e, numToken_t t);
numToken_t nudMinus(numEngine_t *e, numToken_t t);
numToken_t nudOpenParen(numEngine_t *e, numToken_t t);
numToken_t nudCloseParen(numEngine_t *e, numToken_t t);
numToken_t nudInverse(numEngine_t *e, numToken_t t);

nudFunc* nudTable[256] = {
	NULL,          // tagUnknown
	NULL,          // tagError
	&nudValue,     // tagIntegerVal
	&nudValue,     // tagDecimalVal
	&nudPlus,      // tagPlus
	&nudMinus,     // tagMinus
	NULL,          // tagMultiplication
	NULL,          // tagDivision
	NULL,          // tagXor
	NULL,          // tagAnd
	NULL,          // tagOr
	&nudInverse,   // tagInverse
	NULL,          // tagModulo
	&nudOpenParen, // tagOpenParen
	&nudCloseParen,// tagCloseParen
	NULL,          // tagWeeks
	NULL,          // tagDays
	NULL,          // tagHours
	NULL,          // tagMinutes
	NULL,          // tagSeconds
};
numToken_t ledPlus(numEngine_t *e, numToken_t t, numToken_t left);
numToken_t ledMinus(numEngine_t *e, numToken_t t, numToken_t left);
numToken_t ledMultiplication(numEngine_t *e, numToken_t t, numToken_t left);
numToken_t ledDivision(numEngine_t *e, numToken_t t, numToken_t left);
numToken_t ledXor(numEngine_t *e, numToken_t t, numToken_t left);
numToken_t ledAnd(numEngine_t *e, numToken_t t, numToken_t left);
numToken_t ledOr(numEngine_t *e, numToken_t t, numToken_t left);
numToken_t ledModulo(numEngine_t *e, numToken_t t, numToken_t left);
numToken_t ledWeeks(numEngine_t *e, numToken_t t, numToken_t left);
numToken_t ledDays(numEngine_t *e, numToken_t t, numToken_t left);
numToken_t ledHours(numEngine_t *e, numToken_t t, numToken_t left);
numToken_t ledMinutes(numEngine_t *e, numToken_t t, numToken_t left);
numToken_t ledSeconds(numEngine_t *e, numToken_t t, numToken_t left);

ledFunc* ledTable[256] = {
	NULL,              // tagUnknown
	NULL,              // tagError
	NULL,              // tagIntegerVal
	NULL,              // tagDecimalVal
	ledPlus,           // tagPlus
	ledMinus,          // tagMinus
	ledMultiplication, // tagMultiplication
	ledDivision,       // tagDivision
	ledXor,            // tagXor
	ledAnd,            // tagAnd
	ledOr,             // tagOr
	NULL,              // tagInverse
	ledModulo,         // tagModulo
	NULL,              // tagOpenParen
	NULL,              // tagCloseParen
	ledWeeks,          // tagWeeks
	ledDays,           // tagDays
	ledHours,          // tagHours
	ledMinutes,        // tagMinutes
	ledSeconds,        // tagSeconds
};

numToken_t nud(numEngine_t *e, numToken_t t) {
	nudFunc *f = nudTable[t.tag];
	if (f != NULL)
		return f(e, t);
	return (numToken_t){tagError, t.pos, {.e=ErrInvalidNumericExpression}};
}

numToken_t led(numEngine_t *e, numToken_t t, numToken_t left) {
	ledFunc *f = ledTable[t.tag];
	if (f != NULL) 
		return f(e, t, left);
	return (numToken_t){tagError, t.pos, {.e=ErrInvalidNumericExpression}};
}

// expression evaluates the expression at the current token position.
// On return, the current token will be the first token after the evaluated
// expression.
// It returns a numToken_t that can be an integer or a decimal value, or
// an error. The pos field value is meaningful only with errors. The integer
// or the decimal value is the result of the expression evaluation.
numToken_t expression(numEngine_t *e, byte rbp) {
	if (numDone(e))
		return e->tk;
	numToken_t t = e->tk;
	numNextToken(e);
	numToken_t left = nud(e, t);
	while (left.tag != tagError && rbp < precedenceTable[e->tk.tag]) {
		t = e->tk;
		numNextToken(e);
		left = led(e, t, left);
	}
	return left;
}

// evalNumberExpression evaluates the expression in input and
// return the resulting value as a numToken. The returned value
// may be a decimal value or an error.
numToken_t evalNumberExpression(slice_t input) {
	numEngine_t e;
	numEngineInit(&e, input);
	numToken_t t = expression(&e, 0);
	if (t.tag == tagError || t.tag == tagDecimalVal) 
		return t;
	assert(t.tag == tagIntegerVal);
	return (numToken_t){tagDecimalVal, t.pos, {.f=(double)t.val.i}};
}

// isNumberExpr return true if p is a number expression. It looks for the
// first digit that must be in the range '0' to '9'.
bool isNumberExpr(slice_t p) {
	for (int i = 0; i < p.l; i++) {
		if (p.p[i] == '+' || p.p[i] == '-' || p.p[i] == ' ' || p.p[i] == '\t' || p.p[i] == '(')
			continue;
		return isIntDigit(p.p[i]) || (p.p[i] == '.' && i+1 < p.l && isIntDigit(p.p[i+1]));
	}
	return false;
}

// normalizeTypes ensures that v1 anv v2 are both integer or decimal values.
// Requires that v1 and v2 are integer or decimal.
void normalizeTypes(numToken_t *v1, numToken_t *v2) {
	assert(v1->tag == tagIntegerVal || v1->tag == tagDecimalVal);
	assert(v2->tag == tagIntegerVal || v2->tag == tagDecimalVal);
	if (v1->tag == tagIntegerVal) {
		if (v2->tag == tagDecimalVal) {
			*v1 = (numToken_t){tagDecimalVal, v1->pos, {.f=(double)v1->val.i}};
			return;
		}
	} else if (v2->tag == tagIntegerVal) {
		*v2 = (numToken_t){tagDecimalVal, v2->pos, {.f=(double)v2->val.i}};
		return;
	}
}

numToken_t nudValue(numEngine_t *e, numToken_t t) {
	(void)e;
	return t;
}

numToken_t nudPlus(numEngine_t *e, numToken_t t) {
	(void)t;
	numToken_t right = expression(e, highestPrecedence + 1);
	if (right.tag == tagError && right.val.e == ErrEndOfInput)
		right.val.e = ErrInvalidNumericExpression;
	return right;
}

numToken_t nudMinus(numEngine_t *e, numToken_t t) {
	(void)t;
	numToken_t right = expression(e, highestPrecedence + 1);
	if (right.tag == tagError) {
		if (right.val.e == ErrEndOfInput)
			right.val.e = ErrInvalidNumericExpression;
		return right;
	}
	if (right.tag == tagIntegerVal)
		right.val.i = -right.val.i;
	else
		right.val.f = -right.val.f;
	return right;
}

numToken_t nudOpenParen(numEngine_t *e, numToken_t t) {
	numToken_t right = expression(e, precedenceTable[tagOpenParen]);
	if (right.tag == tagError) {
		if (right.val.e == ErrEndOfInput) 
			right.val.e = ErrInvalidNumericExpression;
		return right;
	}
	if (e->tk.tag != tagCloseParen)
		return (numToken_t){tagError, t.pos, {.e=ErrUnclosedParenthesis}};
	numNextToken(e);
	return right;
}

numToken_t nudCloseParen(numEngine_t *e, numToken_t t) {
	(void)e;
	(void)t;
	return (numToken_t){tagError, t.pos, {.e=ErrUnopenedParenthesis}};
}

numToken_t nudInverse(numEngine_t *e, numToken_t t) {
	(void)t;
	numToken_t right = expression(e, highestPrecedence + 1);
	if (right.tag == tagError) {
		if (right.val.e == ErrEndOfInput)
			right.val.e = ErrInvalidNumericExpression;
		return right;
	}
	assert(right.tag == tagIntegerVal || right.tag == tagDecimalVal);
	if (right.tag == tagDecimalVal)
		return (numToken_t){tagError, t.pos, {.e=ErrOperandMustBeInteger}};	
	right.val.i = ~right.val.i;
	return right;
}

numToken_t ledPlus(numEngine_t *e, numToken_t t, numToken_t left) {
	(void)t;
	assert(left.tag == tagIntegerVal || left.tag == tagDecimalVal);
	numToken_t right = expression(e, precedenceTable[tagPlus]);
	if (right.tag == tagError) {
		if (right.val.e == ErrEndOfInput)
			right.val.e = ErrInvalidNumericExpression;
		return right;
	}
	assert(right.tag == tagIntegerVal || right.tag == tagDecimalVal);
	normalizeTypes(&left, &right);
	if (left.tag == tagIntegerVal) {
		assert(right.tag == tagIntegerVal);
		left.val.i += right.val.i;
	} else {
		assert(right.tag == tagDecimalVal);
		left.val.f += right.val.f;
	}
	return left;
}

numToken_t ledMinus(numEngine_t *e, numToken_t t, numToken_t left) {
	(void)t;
	assert(left.tag == tagIntegerVal || left.tag == tagDecimalVal);
	numToken_t right = expression(e, precedenceTable[tagMinus]);
	if (right.tag == tagError) {
		if (right.val.e == ErrEndOfInput)
			right.val.e = ErrInvalidNumericExpression;
		return right;
	}
	assert(right.tag == tagIntegerVal || right.tag == tagDecimalVal);
	normalizeTypes(&left, &right);
	if (left.tag == tagIntegerVal)
		left.val.i -= right.val.i;
	else
		left.val.f -= right.val.f;	
	return left;
}

numToken_t ledMultiplication(numEngine_t *e, numToken_t t, numToken_t left) {
	(void)t;
	assert(left.tag == tagIntegerVal || left.tag == tagDecimalVal);
	numToken_t right = expression(e, precedenceTable[tagMultiplication]);
	if (right.tag == tagError) {
		if (right.val.e == ErrEndOfInput)
			right.val.e = ErrInvalidNumericExpression;
		return right;
	}
	assert(right.tag == tagIntegerVal || right.tag == tagDecimalVal);
	normalizeTypes(&left, &right);
	if (left.tag == tagIntegerVal)
		left.val.i *= right.val.i;
	else
		left.val.f *= right.val.f;	
	return left;
}

numToken_t ledDivision(numEngine_t *e, numToken_t t, numToken_t left) {
	(void)t;
	assert(left.tag == tagIntegerVal || left.tag == tagDecimalVal);
	numToken_t right = expression(e, precedenceTable[tagDivision]);
	if (right.tag == tagError) {
		if (right.val.e == ErrEndOfInput)
			right.val.e = ErrInvalidNumericExpression;
		return right;
	}
	assert(right.tag == tagIntegerVal || right.tag == tagDecimalVal);
	normalizeTypes(&left, &right);
	if (left.tag == tagIntegerVal) {
		if (right.val.i == 0)
			return (numToken_t){tagError, t.pos, {.e=ErrDivisionByZero}};
		left.val.i /= right.val.i;
	} else {
		if (right.val.f == 0)
			return (numToken_t){tagError, t.pos, {.e=ErrDivisionByZero}};
		left.val.f /= right.val.f;
	}
	return left;
}

numToken_t ledModulo(numEngine_t *e, numToken_t t, numToken_t left) {
	assert(left.tag == tagIntegerVal || left.tag == tagDecimalVal);
	numToken_t right = expression(e, precedenceTable[tagModulo]);
	if (right.tag == tagError) {
		if (right.val.e == ErrEndOfInput)
			right.val.e = ErrInvalidNumericExpression;
		return right;
	}
	assert(right.tag == tagIntegerVal || right.tag == tagDecimalVal);
	normalizeTypes(&left, &right);
	if (right.tag == tagDecimalVal)
		return (numToken_t){tagError, t.pos, {.e=ErrOperandMustBeInteger}};
	if (right.val.i == 0)
		return (numToken_t){tagError, t.pos, {.e=ErrDivisionByZero}};
	left.val.i %= right.val.i;
	return left;
}

numToken_t ledAnd(numEngine_t *e, numToken_t t, numToken_t left) {
	assert(left.tag == tagIntegerVal || left.tag == tagDecimalVal);
	numToken_t right = expression(e, precedenceTable[tagAnd]);
	if (right.tag == tagError) {
		if (right.val.e == ErrEndOfInput)
			right.val.e = ErrInvalidNumericExpression;
		return right;
	}
	assert(right.tag == tagIntegerVal || right.tag == tagDecimalVal);
	normalizeTypes(&left, &right);
	if (right.tag == tagDecimalVal)
		return (numToken_t){tagError, t.pos, {.e=ErrOperandMustBeInteger}};
	left.val.i &= right.val.i;
	return left;
}

numToken_t ledOr(numEngine_t *e, numToken_t t, numToken_t left) {
	assert(left.tag == tagIntegerVal || left.tag == tagDecimalVal);
	numToken_t right = expression(e, precedenceTable[tagOr]);
	if (right.tag == tagError) {
		if (right.val.e == ErrEndOfInput)
			right.val.e = ErrInvalidNumericExpression;
		return right;
	}
	assert(right.tag == tagIntegerVal || right.tag == tagDecimalVal);
	normalizeTypes(&left, &right);
	if (right.tag == tagDecimalVal)
		return (numToken_t){tagError, t.pos, {.e=ErrOperandMustBeInteger}};
	left.val.i |= right.val.i;
	return left;
}

numToken_t ledXor(numEngine_t *e, numToken_t t, numToken_t left) {
	assert(left.tag == tagIntegerVal || left.tag == tagDecimalVal);
	numToken_t right = expression(e, precedenceTable[tagXor]);
	if (right.tag == tagError) {
		if (right.val.e == ErrEndOfInput)
			right.val.e = ErrInvalidNumericExpression;
		return right;
	}
	assert(right.tag == tagIntegerVal || right.tag == tagDecimalVal);
	normalizeTypes(&left, &right);
	if (right.tag == tagDecimalVal)
		return (numToken_t){tagError, t.pos, {.e=ErrOperandMustBeInteger}};
	left.val.i ^= right.val.i;
	return left;
}

numToken_t toDouble(numToken_t t) {
	assert(t.tag == tagIntegerVal || t.tag == tagDecimalVal);
	if (t.tag == tagIntegerVal)
		return (numToken_t){tagDecimalVal, t.pos, {.f=(double)t.val.i}};
	return t;
}


numToken_t ledDuration(numEngine_t *e, numToken_t t, numToken_t left, double duration, byte rbp) {
	(void)t;
	assert(left.tag == tagIntegerVal || left.tag == tagDecimalVal);
	left = toDouble(left);
	if (e->tk.tag == tagCloseParen) {
		left.val.f *= duration;
		return left;
	}
	numToken_t right = expression(e, rbp);
	if (right.tag == tagError) { 
		if (right.val.e == ErrEndOfInput) { // right hand operand is optional
			left.val.f *= duration;
			return left;
		}
		return right; // return error
	}
	right = toDouble(right);
	left.val.f = left.val.f*duration + right.val.f;
	return left;
}

numToken_t ledWeeks(numEngine_t *e, numToken_t t, numToken_t left) {
	return ledDuration(e, t, left, 3600 * 24 * 7, precedenceTable[tagWeeks]-1);
}

numToken_t ledDays(numEngine_t *e, numToken_t t, numToken_t left) {
	return ledDuration(e, t, left, 3600 * 24, precedenceTable[tagDays]-1);
}

numToken_t ledHours(numEngine_t *e, numToken_t t, numToken_t left) {
	return ledDuration(e, t, left, 3600, precedenceTable[tagHours]-1);
}

numToken_t ledMinutes(numEngine_t *e, numToken_t t, numToken_t left) {
	return ledDuration(e, t, left, 60, precedenceTable[tagMinutes]-1);
}

numToken_t ledSeconds(numEngine_t *e, numToken_t t, numToken_t left) {
	return ledDuration(e, t, left, 1, precedenceTable[tagSeconds]-1);
}

// ----------------------------------------------------------------------------------------------------------------------------------------
// Engine
// ----------------------------------------------------------------------------------------------------------------------------------------

// isNullValue return true when p is equal to "null", "Null", "NULL".
const char* isLiteralValue(slice_t p) {
	switch (p.l) {
	case 5:
		if ((p.p[0] == 'f' || p.p[0] == 'F') &&
			((p.p[1] == 'a' && p.p[2] == 'l' && p.p[3] == 's' && p.p[4] == 'e') ||
				(p.p[1] == 'A' && p.p[2] == 'L' && p.p[3] == 'S' && p.p[4] == 'E'))) {
			return "false";
		}
		// fall through
	case 4:
		if ((p.p[0] == 'n' || p.p[0] == 'N') &&
			((p.p[1] == 'u' && p.p[2] == 'l' && p.p[3] == 'l') || (p.p[1] == 'U' && p.p[2] == 'L' && p.p[3] == 'L'))) {
			return "null";
		}
		if ((p.p[0] == 't' || p.p[0] == 'T') &&
			((p.p[1] == 'r' && p.p[2] == 'u' && p.p[3] == 'e') || (p.p[1] == 'R' && p.p[2] == 'U' && p.p[3] == 'E'))) {
			return "true";
		}
		// fall through
	case 3:
		if ((p.p[0] == 'y' || p.p[0] == 'Y') &&
			((p.p[1] == 'e' && p.p[2] == 's') || (p.p[1] == 'E' && p.p[2] == 'S'))) {
			return "true";
		}
		if ((p.p[0] == 'o' || p.p[0] == 'O') &&
			((p.p[1] == 'f' && p.p[2] == 'f') || (p.p[1] == 'F' && p.p[2] == 'F'))) {
			return "false";
		}
		// fall through
	case 2:
		if ((p.p[0] == 'o' || p.p[0] == 'O') && (p.p[1] == 'n' || p.p[1] == 'N')) {
			return "true";
		}
		if ((p.p[0] == 'n' || p.p[0] == 'N') && (p.p[1] == 'o' || p.p[1] == 'O')) {
			return "false";
		}
		// fall through
	}
	return NULL;
}

bool values(engine_t *e);
bool members(engine_t *e);

// value process a value. If an error occurred it returns with the error set,
// otherwise calls nextToken() and return the returned value of done().
bool value(engine_t *e) {
	slice_t val;
	const char* str;
	pos_t startPos;
	char buf[256];
	switch (e->tk.tag) {
	case tagCloseSquare:
		setError(e, ErrUnexpectedCloseSquare);
		return false;
	case tagCloseBrace:
		setError(e, ErrUnexpectedCloseBrace);
		return false;
	case tagDoubleQuotedString:
		outputDoubleQuotedString(e);
		break;
	case tagSingleQuotedString:
		outputSingleQuotedString(e);
		break;
	case tagMultilineString:
		outputMultilineString(e);
		break;
	case tagQuotelessString:
		val = e->tk.val;
		str = isLiteralValue(val);
		if (str != NULL) {
			outputString(e, str);
			break;
		}
		if (isNumberExpr(val)) {
			numToken_t t = evalNumberExpression(val);
			if (t.tag == tagError) {
				setErrorAndPos(e, t.val.e, (pos_t){e->tk.pos.b+t.pos, e->tk.pos.s, e->tk.pos.l});
				return true;
			}
			assert(t.tag != tagIntegerVal);
			sprintf(buf, "%.16g", t.val.f);
			outputString(e, buf);			
		} else {
			outputQuotelessString(e);
		}
		break;
	case tagOpenBrace:
		startPos = e->tk.pos;
		nextToken(e);
		if (done(e)) {
			if (e->tk.val.p == ErrEndOfInput)
				setErrorAndPos(e, ErrUnclosedObject, startPos);
			return true;
		}
		if (e->depth == maxDepth) {
			setError(e, ErrMaxObjectArrayDepth);
			return true;
		}
		e->depth++;
		if (members(e)) {
			if (e->tk.val.p == ErrEndOfInput)
				setErrorAndPos(e, ErrUnclosedObject, startPos);
			return true;
		}
		e->depth--;
		break;
	case tagOpenSquare:
		nextToken(e);
		if (done(e)) {
			if (e->tk.val.p == ErrEndOfInput)
				setError(e, ErrUnclosedArray);
			return true;
		}
		startPos = e->tk.pos;
		if (e->depth == maxDepth) {
			setError(e, ErrMaxObjectArrayDepth);
			return true;
		}
		e->depth++;
		if (values(e)) {
			if (e->tk.val.p == ErrEndOfInput)
				setErrorAndPos(e, ErrUnclosedArray, startPos);
			return true;
		}
		e->depth--;
		break;
	default:
		setError(e, ErrSyntaxError);
		//		e.setError(fmt.Errorf("expected value, got %v", e.tk))
		return false;
	}
	nextToken(e);
	return done(e);
}

// values process 0 or more values and pops the ending ]. Return done().
bool values(engine_t *e) {
	bool notFirst = false;
	outputByte(e, '[');
	while (!done(e) && e->tk.tag != tagCloseSquare) {
		if (notFirst) {
			outputByte(e, ',');
			if (e->tk.tag == tagComma) {
				nextToken(e);
				if (done(e)) {
					if (e->tk.val.p == ErrEndOfInput) {
						setError(e, ErrExpectValueAfterComma);
					}
					break;
				}
				if (e->tk.tag == tagCloseBrace || e->tk.tag == tagCloseSquare) {
					setError(e, ErrExpectValueAfterComma);
					break;
				}
			}
		} else {
			notFirst = true;
		}
		if (value(e)) {
			break;
		}
	}
	outputByte(e, ']');
	return done(e);
}

bool member(engine_t *e) {
	switch (e->tk.tag) {
	case tagCloseSquare:
		setError(e, ErrUnexpectedCloseSquare);
		return false;
	case tagDoubleQuotedString:
		outputDoubleQuotedString(e);
		break;
	case tagSingleQuotedString:
		outputSingleQuotedString(e);
		break;
	case tagQuotelessString:
		outputQuotelessString(e);
		break;
	default:
		setError(e,ErrExpectStringIdentifier);
		break;
	}
	nextToken(e);
	if (done(e)) {
		if (e->tk.val.p == ErrEndOfInput)
			setError(e,ErrUnexpectedEndOfInput);
		return true;
	}
	if (e->tk.tag != tagColon) {
		setError(e, ErrExpectColon);
		return true;
	}
	outputByte(e, ':');
	nextToken(e);
	if (done(e)) {
		if (e->tk.val.p == ErrEndOfInput)
			setError(e,ErrUnexpectedEndOfInput);
		return true;
	}
	return value(e);
}

// values process 0 or more members (identifiers : value) and pops the ending }. Return done().
bool members(engine_t *e) {
	bool notFirst = false;
	outputByte(e, '{');
	while (!done(e) && e->tk.tag != tagCloseBrace) {
		if (notFirst) {
			outputByte(e, ',');
			if (e->tk.tag == tagComma) {
				nextToken(e);
				if (done(e)) {
					if (e->tk.val.p == ErrEndOfInput)
						setError(e, ErrExpectIdentifierAfterComma);
					break;
				}
				if (e->tk.tag == tagCloseBrace || e->tk.tag == tagCloseSquare) {
					setError(e, ErrExpectIdentifierAfterComma);
					break;
				}
			}
		} else {
			notFirst = true;
		}
		if (member(e))
			break;
	}
	outputByte(e, '}');
	return done(e);
}

// ----------------------------------------------------------------------------------------------------------------------------------------
// Main
// ----------------------------------------------------------------------------------------------------------------------------------------

int myStrLen(const char* str) {
	int i = 0;
	while (str[i] != 0)
		i++;
	return i;
}

// qjson_decode accept a qjson text string as input and returns a 
// heap allocated string. If the string start with the character '{',
// the string is the json encoding of the input text, otherwise it
// is an error message. qjson_decode will never return NULL or an
// empty string.
char* qjson_decode(const char *qjsonText) {
	int len = (qjsonText == NULL) ? 0 : myStrLen(qjsonText); // strlen(qjsonText);
	if (len == 0 ) {
		return strcpy(malloc(3), "{}");
	}
	engine_t e;
	e.in = qjsonText;
	e.p = (slice_t){e.in, len};
	outputInit(&e);
	e.depth = 0;
	e.pos = (pos_t){0,0,0};
	e.tk.tag = tagUnknown;
	e.tk.pos = e.pos;
	e.tk.val.p = NULL;
	e.tk.val.l = 0;
	nextToken(&e);
	members(&e);
	if (e.tk.tag == tagCloseBrace)
		e.tk = (token_t){tagError, e.tk.pos, {ErrSyntaxError, strlen(ErrSyntaxError)}};
	assert(e.tk.tag == tagError);
	if (e.tk.val.p == ErrEndOfInput) {
		outputByte(&e, '\0');
		return outputGet(&e);
	}
	outputReset(&e);
	outputString(&e, e.tk.val.p);
	char buf[256];
	sprintf(buf, " at line %d col %d", e.tk.pos.l+1, column((slice_t){e.in+e.tk.pos.s, e.tk.pos.b-e.tk.pos.s})+1);
	outputString(&e, buf);
	outputByte(&e, '\0');
	return outputGet(&e);
}