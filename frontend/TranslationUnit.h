// Copyright (c) 2008 Roberto Raggi <roberto.raggi@gmail.com>
// Copyright (c) 2016-20 Leandro T. C. Melo <ltcmelo@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#ifndef PSYCHE_TRANSLATION_UNIT_H__
#define PSYCHE_TRANSLATION_UNIT_H__

#include "FrontendConfig.h"

#include "ASTFwds.h"
#include "Token.h"
#include "DiagnosticCollector.h"
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#if !(__cplusplus > 199711L || __GXX_EXPERIMENTAL_CXX0X__ || _MSC_VER >= 1600 || defined( _LIBCPP_VERSION )) \
    || (defined(__GNUC_LIBSTD__) && ((__GNUC_LIBSTD__-0) * 100 + __GNUC_LIBSTD_MINOR__-0 <= 402))
#define USE_TR1
#endif

#if defined(_MSC_VER) && _MSC_VER < 1600
#  include <map>
#elif defined(USE_TR1)
#  include <tr1/unordered_map>
#else
#  include <unordered_map>
#endif

namespace psyche {

class CFE_API TranslationUnit
{
    TranslationUnit(const TranslationUnit &other);
    void operator =(const TranslationUnit &other);

public:
    TranslationUnit(Control *control, const StringLiteral* fileId);
    ~TranslationUnit();

    Control *control() const;

    const StringLiteral* fileId() const;
    const char *fileName() const;
    unsigned fileNameLength() const;

    const char *firstSourceChar() const;
    const char *lastSourceChar() const;

    void setSource(const char *source, unsigned size);
    unsigned sourceLength() const;
    std::string fetchSource(AST* ast) const;

    bool isTokenized() const;
    void tokenize();
    unsigned tokenCount() const { return _tokens.size(); }
    const Token &tokenAt(unsigned index) const { return _tokens[index]; }

    Kind tokenKind(unsigned index) const { return tokenAt(index).kind(); }
    const char *spell(unsigned index) const;

    unsigned commentCount() const;
    const Token &commentAt(unsigned index) const;

    unsigned matchingBrace(unsigned index) const;
    const Identifier* identifier(unsigned index) const;
    const Literal* literal(unsigned index) const;
    const StringLiteral* stringLiteral(unsigned index) const;
    const NumericLiteral* numericLiteral(unsigned index) const;

    MemoryPool *memoryPool() const;
    AST* ast() const;
    void releaseAst();

    bool blockErrors() const { return f._blockErrors; }
    bool blockErrors(bool block)
    {
        const bool previous = f._blockErrors;
        f._blockErrors = block;
        return previous;
    }

    void warning(unsigned index, const char *fmt, ...);
    void error(unsigned index, const char *fmt, ...);
    void fatal(unsigned index, const char *fmt, ...);
    void message(DiagnosticCollector::Severity severity,
                 unsigned index,
                 const char *format, va_list ap);

    bool skipFunctionBody() const;
    void setSkipFunctionBody(bool skipFunctionBody);

    enum ParseMode {
        ParseTranlationUnit,
        ParseDeclaration,
        ParseExpression,
        ParseDeclarator,
        ParseStatement
    };

    bool parse(ParseMode mode = ParseTranlationUnit);
    bool isParsed() const;

    void getTokenStartPosition(unsigned index, unsigned *line,
                               unsigned *column = 0,
                               const StringLiteral* *fileName = 0) const;

    void getTokenEndPosition(unsigned index, unsigned *line,
                             unsigned *column = 0,
                             const StringLiteral* *fileName = 0) const;

    void getPosition(unsigned utf16charOffset,
                     unsigned *line,
                     unsigned *column = 0,
                     const StringLiteral* *fileName = 0) const;

    void getTokenPosition(unsigned index,
                          unsigned *line,
                          unsigned *column = 0,
                          const StringLiteral* *fileName = 0) const;

    void pushLineOffset(unsigned offset);
    void pushPreprocessorLine(unsigned utf16charOffset,
                              unsigned line,
                              const StringLiteral* fileName);

    unsigned findPreviousLineOffset(unsigned tokenIndex) const;

    bool maybeSplitGreaterGreaterToken(unsigned tokenIndex);

    Dialect dialect() const { return _dialect; }
    void setDialect(Dialect dialect) { _dialect = dialect; }

private:
    struct PPLine {
        unsigned utf16charOffset;
        unsigned line;
        const StringLiteral* fileName;

        PPLine(unsigned utf16charOffset = 0,
               unsigned line = 0,
               const StringLiteral* fileName = 0)
            : utf16charOffset(utf16charOffset), line(line), fileName(fileName)
        { }

        bool operator == (const PPLine &other) const
        { return utf16charOffset == other.utf16charOffset; }

        bool operator != (const PPLine &other) const
        { return utf16charOffset != other.utf16charOffset; }

        bool operator < (const PPLine &other) const
        { return utf16charOffset < other.utf16charOffset; }
    };

    unsigned findLineNumber(unsigned utf16charOffset) const;
    unsigned findColumnNumber(unsigned utf16CharOffset, unsigned lineNumber) const;
    PPLine findPreprocessorLine(unsigned utf16charOffset) const;
    void showErrorLine(unsigned index, unsigned column, FILE *out);

    static const Token nullToken;

    Control *_control;
    const StringLiteral* _fileId;
    const char *_firstSourceChar;
    const char *_lastSourceChar;
    std::vector<Token> _tokens;
    std::vector<Token> _comments;
    std::vector<unsigned> _lineOffsets;
    std::vector<PPLine> _ppLines;

#if defined(_MSC_VER) && _MSC_VER < 1600
    // MSVC2008 and earlier do not implement TR1.
    typedef std::map<unsigned, std::pair<unsigned, unsigned> > TokenLineColumn;
#elif defined(USE_TR1)
    typedef std::tr1::unordered_map<unsigned, std::pair<unsigned, unsigned> > TokenLineColumn;
#else
    typedef std::unordered_map<unsigned, std::pair<unsigned, unsigned> > TokenLineColumn;
#endif

    TokenLineColumn _expandedLineColumn;
    MemoryPool *_pool;
    AST* _ast;
    TranslationUnit *_previousTranslationUnit;
    struct Flags {
        unsigned _tokenized: 1;
        unsigned _parsed: 1;
        unsigned _blockErrors: 1;
        unsigned _skipFunctionBody: 1;
    };
    union {
        unsigned _flags;
        Flags f;
    };
    Dialect _dialect;
};

} // namespace psyche


#endif
