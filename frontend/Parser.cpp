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

#include "Parser.h"
#include "Token.h"
#include "Lexer.h"
#include "Control.h"
#include "AST.h"
#include "Literals.h"
#include <iostream>
#include <unordered_map>
#include <utility>
#include <string>
#include <cstdio> // for putchar


#if defined(_MSC_VER) && (_MSC_VER < 1800)
#    define va_copy(dst, src) ((dst) = (src))
#elif defined(__INTEL_COMPILER) && !defined(va_copy)
#    define va_copy __va_copy
#endif

#define PSYCHE_NO_DEBUG_RULE
#define MAX_EXPRESSION_DEPTH 1000
#define MAX_STATEMENT_DEPTH 100

using namespace psyche;
using namespace psyche;

namespace {

class DebugRule {
public:
    static int depth;

    DebugRule(const char *name, const char *spell, unsigned idx, bool blocked)
    {
        for (int i = 0; i <= depth; ++i)
          fputc('-', stderr);

        ++depth;
        fprintf(stderr, " %s, ahead: '%s' (%u) - block-errors: %d\n", name, spell, idx, blocked);
    }

    ~DebugRule()
    { --depth; }
};

int DebugRule::depth = 0;

inline bool lookAtAssignmentOperator(int tokenKind)
{
    switch (tokenKind) {
    case T_EQUAL:
    case T_AMPER_EQUAL:
    case T_CARET_EQUAL:
    case T_SLASH_EQUAL:
    case T_GREATER_GREATER_EQUAL:
    case T_LESS_LESS_EQUAL:
    case T_MINUS_EQUAL:
    case T_PERCENT_EQUAL:
    case T_PIPE_EQUAL:
    case T_PLUS_EQUAL:
    case T_STAR_EQUAL:
    case T_TILDE_EQUAL:
        return true;
    default:
        return false;
    } // switch
}

namespace Prec {
enum {
    Unknown         = 0,
    Comma           = 1,
    Assignment      = 2,
    Conditional     = 3,
    LogicalOr       = 4,
    LogicalAnd      = 5,
    InclusiveOr     = 6,
    ExclusiveOr     = 7,
    And             = 8,
    Equality        = 9,
    Relational      = 10,
    Shift           = 11,
    Additive        = 12,
    Multiplicative  = 13,
    PointerToMember = 14
};
} // namespace Precedece

inline int precedence(int tokenKind, bool templateArguments)
{
    // ### this will/might need some tuning for C++0x
    // (see: [temp.names]p3)
    if (templateArguments && (tokenKind == T_GREATER || tokenKind == T_GREATER_GREATER))
        return -1;

    if (lookAtAssignmentOperator(tokenKind))
        return Prec::Assignment;

    switch (tokenKind) {
    case T_COMMA:           return Prec::Comma;
    case T_QUESTION:        return Prec::Conditional;
    case T_PIPE_PIPE:       return Prec::LogicalOr;
    case T_AMPER_AMPER:     return Prec::LogicalAnd;
    case T_PIPE:            return Prec::InclusiveOr;
    case T_CARET:           return Prec::ExclusiveOr;
    case T_AMPER:           return Prec::And;
    case T_EQUAL_EQUAL:
    case T_EXCLAIM_EQUAL:   return Prec::Equality;
    case T_GREATER:
    case T_LESS:
    case T_LESS_EQUAL:
    case T_GREATER_EQUAL:   return Prec::Relational;
    case T_LESS_LESS:
    case T_GREATER_GREATER: return Prec::ExclusiveOr;
    case T_PLUS:
    case T_MINUS:           return Prec::Additive;
    case T_STAR:
    case T_SLASH:
    case T_PERCENT:         return Prec::Multiplicative;
    case T_ARROW_STAR:
    case T_DOT_STAR:        return Prec::PointerToMember;
    default:                return Prec::Unknown;
    }
}

inline bool isBinaryOperator(int tokenKind)
{ return precedence(tokenKind, false) != Prec::Unknown; }

inline bool isRightAssociative(int tokenKind)
{
    const int prec = precedence(tokenKind, false);
    return prec == Prec::Conditional || prec == Prec::Assignment;
}

} // end of anonymous namespace

class Parser::ASTCache
{
    ASTCache(const ASTCache &other);
    void operator =(const ASTCache &other);

public:
    enum ASTKind {
        Expression,
        ExpressionList,
        ParameterDeclarationClause,
        TemplateId,
        TypeId
    };

public:
    ASTCache() {}

    void insert(ASTKind astKind, unsigned tokenIndexBeforeParsing,
                AST* resultingAST, unsigned resultingTokenIndex, bool resultingReturnValue)
    {
        const auto key = std::make_pair(astKind, tokenIndexBeforeParsing);

        ParseFunctionResult result;
        result.resultingAST = resultingAST;
        result.resultingTokenIndex = resultingTokenIndex;
        result.returnValue = resultingReturnValue;
        const auto keyValue = std::make_pair(key, result);
        _cache.insert(keyValue);
    }

    AST* find(ASTKind astKind, unsigned tokenIndex,
              unsigned *resultingTokenIndex, bool *foundInCache, bool *returnValue) const
    {
        const auto key = std::make_pair(astKind, tokenIndex);
        const auto it = _cache.find(key);
        if (it == _cache.end()) {
            *foundInCache = false;
            return 0;
        } else {
            *foundInCache = true;
            *resultingTokenIndex = it->second.resultingTokenIndex;
            *returnValue = it->second.returnValue;
            return it->second.resultingAST;
        }
    }

    void clear()
    {
        _cache.clear();
    }

private:
    struct KeyHasher {
        size_t operator()(const std::pair<int, unsigned> &key) const
        { return std::hash<int>()(key.first) ^ std::hash<unsigned>()(key.second); }
    };

    struct ParseFunctionResult {
        AST* resultingAST;
        unsigned resultingTokenIndex;
        bool returnValue;
    };

    typedef std::pair<int, unsigned> ASTKindAndTokenIndex;
    std::unordered_map<ASTKindAndTokenIndex, ParseFunctionResult, KeyHasher> _cache;
};

#ifndef PSYCHE_NO_DEBUG_RULE
#  define DEBUG_THIS_RULE() DebugRule __debug_rule__(__func__, token().spell(), cursor(), _translationUnit->blockErrors())
inline void debugPrintCheckCache(bool goodCase)
{
    for (int i = 0; i <= DebugRule::depth - 1; ++i)
        fputc('-', stderr);
    if (goodCase)
        fprintf(stderr, " CACHE: Re-using AST from Cache.\n");
    else
        fprintf(stderr, " CACHE: Already tried to parse this, skipping.\n");
}
#else
#  define DEBUG_THIS_RULE() do {} while (0)
inline void debugPrintCheckCache(bool) {}
#endif

#define CHECK_CACHE(ASTKind, ASTType) \
    do { \
        bool foundInCache; \
        unsigned newTokenIndex; \
        bool returnValue; \
        if (AST* ast = _astCache->find(ASTKind, cursor(), \
                                       &newTokenIndex, &foundInCache, &returnValue)) { \
            debugPrintCheckCache(true); \
            node = (ASTType *) ast; \
            _tokenIndex = newTokenIndex; \
            return returnValue; \
        } else if (foundInCache) { \
            debugPrintCheckCache(false); \
            return returnValue; \
        } \
    } while (0)

#define PARSE_EXPRESSION_WITH_OPERATOR_PRECEDENCE(node, minPrecedence) { \
    if (LA() == T_THROW) { \
        if (!parseThrowExpression(node)) \
            return false; \
    } else if (!parseCastExpression(node)) \
        return false; \
    \
    parseExpressionWithOperatorPrecedence(node, minPrecedence); \
    return true; \
}

Parser::Parser(TranslationUnit *unit)
    : _translationUnit(unit),
      _control(unit->control()),
      _pool(unit->memoryPool()),
      _language(unit->dialect()),
      _tokenIndex(1),
      _templateArguments(0),
      _inFunctionBody(false),
      _inExpressionStatement(false),
      _expressionDepth(0),
      _statementDepth(0),
      _astCache(new ASTCache),
      _expressionStatementAstCache(new ASTCache)
{ }

Parser::~Parser()
{
    delete _expressionStatementAstCache;
    delete _astCache;
}

bool Parser::switchTemplateArguments(bool templateArguments)
{
    bool previousTemplateArguments = _templateArguments;
    _templateArguments = templateArguments;
    return previousTemplateArguments;
}

bool Parser::maybeSplitGreaterGreaterToken(int n)
{
    return _translationUnit->maybeSplitGreaterGreaterToken(_tokenIndex + n - 1);
}

bool Parser::skipUntil(int token)
{
    while (int tk = LA()) {
        if (tk == token)
            return true;

        consumeToken();
    }

    return false;
}

void Parser::skipUntilDeclaration()
{
    for (; ; consumeToken()) {
        switch (LA()) {
        case T_EOF_SYMBOL:

        // end of a block
        case T_RBRACE:

        // names
        case T_IDENTIFIER:
        case T_COLON_COLON:
        case T_TILDE:
        case T_OPERATOR:

        // empty declaration
        case T_SEMICOLON:

        // member specification
        case T_USING:
        case T_TEMPLATE:
        case T_PUBLIC:
        case T_PROTECTED:
        case T_PRIVATE:

        // declarations
        case T_ENUM:
        case T_NAMESPACE:
        case T_INLINE:
        case T_STATIC_ASSERT:
        case T_ASM:
        case T_EXPORT:

        default:
            if (lookAtBuiltinTypeSpecifier() || lookAtClassKey() ||
                lookAtFunctionSpecifier() || lookAtStorageClassSpecifier())
                return;
        } // switch
    }
}

bool Parser::skipUntilStatement()
{
    while (int tk = LA()) {
        switch (tk) {
            case T_SEMICOLON:
            case T_LBRACE:
            case T_RBRACE:
            case T_CONST:
            case T_VOLATILE:
            case T_RESTRICT:
            case T_IDENTIFIER:
            case T_CASE:
            case T_DEFAULT:
            case T_IF:
            case T_SWITCH:
            case T_WHILE:
            case T_DO:
            case T_FOR:
            case T_BREAK:
            case T_CONTINUE:
            case T_RETURN:
            case T_GOTO:
            case T_TRY:
            case T_CATCH:
            case T_THROW:
            case T_CHAR:
            case T_CHAR16_T:
            case T_CHAR32_T:
            case T_WCHAR_T:
            case T_BOOL:
            case T_SHORT:
            case T_INT:
            case T_LONG:
            case T_SIGNED:
            case T_UNSIGNED:
            case T_FLOAT:
            case T_DOUBLE:
            case T_VOID:
            case T_CLASS:
            case T_STRUCT:
            case T_UNION:
            case T_ENUM:
            case T_COLON_COLON:
            case T_TEMPLATE:
            case T_USING:
                return true;

            default:
                consumeToken();
        }
    }

    return false;
}

bool Parser::skip(int l, int r)
{
    int count = 0;

    while (int tk = LA()) {
        if (tk == l)
            ++count;
        else if (tk == r)
            --count;
        else if (l != T_LBRACE && (tk == T_LBRACE ||
                                   tk == T_RBRACE ||
                                   tk == T_SEMICOLON))
            return false;

        if (count == 0)
            return true;

        consumeToken();
    }

    return false;
}

int Parser::find(int token, int stopAt)
{
    for (int i = 1; ; ++i) {
        const int tk = LA(i);
        if (!tk || tk == stopAt)
            return 0;
        if (tk == token)
            return i;
    }
    return 0;
}

void Parser::match(int kind, unsigned *tk)
{
    if (LA() == kind)
        *tk = consumeToken();
    else {
        *tk = 0;
        error(_tokenIndex, "expected token `%s' got `%s'",
              Token::name(kind), token().spell());
    }
}

bool Parser::parseClassOrNamespaceName(NameAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() == T_IDENTIFIER && (LA(2) == T_COLON_COLON || LA(2) == T_LESS)) {
        unsigned identifier_token = cursor();

        if (LA(2) == T_LESS) {
            bool blocked = blockErrors(true);
            if (parseTemplateId(node) && LA() == T_COLON_COLON) {
                blockErrors(blocked);
                return true;
            }
            blockErrors(blocked);
        }

        rewind(identifier_token);

        if (LA(2) == T_COLON_COLON) {
            SimpleNameAST* ast = new (_pool) SimpleNameAST;
            ast->identifier_token = consumeToken();
            node = ast;
            return true;
        }
    } else if (LA() == T_TEMPLATE) {
        unsigned template_token = consumeToken();
        if (parseTemplateId(node, template_token) && LA() == T_COLON_COLON)
            return true;
        rewind(template_token);
    }
    return false;
}

bool Parser::parseTemplateId(NameAST* &node, unsigned template_token)
{
    DEBUG_THIS_RULE();
    CHECK_CACHE(ASTCache::TemplateId, NameAST);

    const unsigned start = cursor();

    if (LA() == T_IDENTIFIER && LA(2) == T_LESS) {
        TemplateIdAST* ast = new (_pool) TemplateIdAST;
        ast->template_token = template_token;
        ast->identifier_token = consumeToken();
        ast->less_token = consumeToken();
        if (maybeSplitGreaterGreaterToken() || LA() == T_GREATER || parseTemplateArgumentList(
                ast->template_argument_list)) {
            if (maybeSplitGreaterGreaterToken() || LA() == T_GREATER) {
                ast->greater_token = consumeToken();
                node = ast;
                const bool result = true;
                _astCache->insert(ASTCache::TemplateId, start, node, cursor(), result);
                return result;
            }
        }
    }

    const bool result = false;
    _astCache->insert(ASTCache::TemplateId, start, 0, cursor(), result);
    rewind(start);
    return result;
}

bool Parser::parseNestedNameSpecifier(NestedNameSpecifierListAST* &node,
                                      bool /*acceptTemplateId*/)
{
    DEBUG_THIS_RULE();
    NestedNameSpecifierListAST* *nested_name_specifier = &node;
    NameAST* class_or_namespace_name = 0;
    if (parseClassOrNamespaceName(class_or_namespace_name) && LA() == T_COLON_COLON) {
        unsigned scope_token = consumeToken();

        NestedNameSpecifierAST* name = new (_pool) NestedNameSpecifierAST;
        name->class_or_namespace_name = class_or_namespace_name;
        name->scope_token = scope_token;

        *nested_name_specifier = new (_pool) NestedNameSpecifierListAST(name);
        nested_name_specifier = &(*nested_name_specifier)->next;

        while (parseClassOrNamespaceName(class_or_namespace_name) && LA() == T_COLON_COLON) {
            scope_token = consumeToken();

            name = new (_pool) NestedNameSpecifierAST;
            name->class_or_namespace_name = class_or_namespace_name;
            name->scope_token = scope_token;

            *nested_name_specifier = new (_pool) NestedNameSpecifierListAST(name);
            nested_name_specifier = &(*nested_name_specifier)->next;
        }

        // ### ugly hack
        rewind(scope_token);
        consumeToken();
        return true;
    }

    return false;
}

bool Parser::parseNestedNameSpecifierOpt(NestedNameSpecifierListAST* &name, bool acceptTemplateId)
{
    DEBUG_THIS_RULE();
    unsigned start = cursor();
    if (! parseNestedNameSpecifier(name, acceptTemplateId))
        rewind(start);
    return true;
}

bool Parser::parseName(NameAST* &node, bool acceptTemplateId)
{
    DEBUG_THIS_RULE();
    unsigned global_scope_token = 0;

    switch (LA()) {
    case T_COLON_COLON:
    case T_IDENTIFIER:
    case T_TILDE: // destructor-name-id
    case T_OPERATOR: // operator-name-id
    case T_TEMPLATE: // template introduced template-id
      break;
    default:
      return false;
    }

    if (LA() == T_COLON_COLON)
        global_scope_token = consumeToken();

    NestedNameSpecifierListAST* nested_name_specifier = 0;
    parseNestedNameSpecifierOpt(nested_name_specifier,
                                /*acceptTemplateId=*/ true);

    NameAST* unqualified_name = 0;
    if (parseUnqualifiedName(unqualified_name,
                             /*acceptTemplateId=*/ acceptTemplateId || nested_name_specifier != 0)) {
        if (! global_scope_token && ! nested_name_specifier) {
            node = unqualified_name;
            return true;
        }

        QualifiedNameAST* ast = new (_pool) QualifiedNameAST;
        ast->global_scope_token = global_scope_token;
        ast->nested_name_specifier_list = nested_name_specifier;
        ast->unqualified_name = unqualified_name;
        node = ast;
        return true;
    }

    return false;
}

bool Parser::parseTranslationUnit(TranslationUnitAST* &node)
{
    DEBUG_THIS_RULE();
    TranslationUnitAST* ast = new (_pool) TranslationUnitAST;
    DeclarationListAST* *decl = &ast->declaration_list;

    while (LA()) {
        unsigned start_declaration = cursor();

        DeclarationAST* declaration = 0;

        if (parseDeclaration(declaration)) {
            *decl = new (_pool) DeclarationListAST;
            (*decl)->value = declaration;
            decl = &(*decl)->next;
        } else {
            error(start_declaration, "expected a declaration");
            rewind(start_declaration + 1);
            skipUntilDeclaration();
        }


        if (TopLevelDeclarationProcessor *processor = _control->topLevelDeclarationProcessor()) {
            if (!processor->processDeclaration(declaration))
                break;
        }

        _templateArgumentList.clear();
    }

    node = ast;
    return true;
}

bool Parser::parseEmptyDeclaration(DeclarationAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() == T_SEMICOLON) {
        EmptyDeclarationAST* ast = new (_pool) EmptyDeclarationAST;
        ast->semicolon_token = consumeToken();
        node = ast;
        return true;
    }
    return false;
}

bool Parser::parseDeclaration(DeclarationAST* &node)
{
    DEBUG_THIS_RULE();

    switch (LA()) {
    case T_SEMICOLON:
        return parseEmptyDeclaration(node);

    case T_NAMESPACE:
        return parseNamespace(node);

    case T_USING:
        return parseUsing(node);

    case T_ASM:
        return parseAsmDefinition(node);

    case T_PSYCHEC_TEMPLATE:
        return parseGenericsDeclaration(node);

    case T_TEMPLATE:
    case T_EXPORT:
        return parseTemplateDeclaration(node);

    case T_INLINE:
        if (_language.cpp11 && LA(2) == T_NAMESPACE)
            return parseNamespace(node);
        return parseSimpleDeclaration(node);

    case T_STATIC_ASSERT:
        if (_language.cpp11)
            return parseStaticAssertDeclaration(node);
        return parseSimpleDeclaration(node);

    default: {
        if (LA() == T_EXTERN && LA(2) == T_TEMPLATE)
            return parseTemplateDeclaration(node);
        if (LA() == T_EXTERN && LA(2) == T_STRING_LITERAL)
            return parseLinkageSpecification(node);
        return parseSimpleDeclaration(node);
    }

    } // end switch

    return false;
}

bool Parser::parseLinkageSpecification(DeclarationAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() == T_EXTERN && LA(2) == T_STRING_LITERAL) {
        LinkageSpecificationAST* ast = new (_pool) LinkageSpecificationAST;
        ast->extern_token = consumeToken();
        ast->extern_type_token = consumeToken();

        if (LA() == T_LBRACE)
            parseLinkageBody(ast->declaration);
        else
            parseDeclaration(ast->declaration);

        node = ast;
        return true;
    }

    return false;
}

bool Parser::parseLinkageBody(DeclarationAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() == T_LBRACE) {
        LinkageBodyAST* ast = new (_pool) LinkageBodyAST;
        ast->lbrace_token = consumeToken();
        DeclarationListAST* *declaration_ptr = &ast->declaration_list;

        while (int tk = LA()) {
            if (tk == T_RBRACE)
                break;

            unsigned start_declaration = cursor();
            DeclarationAST* declaration = 0;
            if (parseDeclaration(declaration)) {
                *declaration_ptr = new (_pool) DeclarationListAST;
                (*declaration_ptr)->value = declaration;
                declaration_ptr = &(*declaration_ptr)->next;
            } else {
                error(start_declaration, "expected a declaration");
                rewind(start_declaration + 1);
                skipUntilDeclaration();
            }

            _templateArgumentList.clear();
        }
        match(T_RBRACE, &ast->rbrace_token);
        node = ast;
        return true;
    }
    return false;
}

bool Parser::parseStaticAssertDeclaration(DeclarationAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() != T_STATIC_ASSERT)
        return false;

    StaticAssertDeclarationAST* ast = new (_pool) StaticAssertDeclarationAST;
    ast->static_assert_token = consumeToken();
    match(T_LPAREN, &ast->lparen_token);
    parseConstantExpression(ast->expression);
    match(T_COMMA, &ast->comma_token);
    parseStringLiteral(ast->string_literal);
    match(T_RPAREN, &ast->rparen_token);
    match(T_SEMICOLON, &ast->semicolon_token);

    node = ast;
    return true;
}

// ### rename parseNamespaceAliarOrDeclaration?
bool Parser::parseNamespace(DeclarationAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() != T_NAMESPACE && !(_language.cpp11 && LA() == T_INLINE && LA(2) == T_NAMESPACE))
        return false;

    unsigned inline_token = 0;
    if (_language.cpp11 && LA() == T_INLINE)
        inline_token = consumeToken();

    unsigned namespace_token = consumeToken();

    if (LA() == T_IDENTIFIER && LA(2) == T_EQUAL) {
        if (inline_token)
            warning(inline_token, "namespace alias cannot be inline");
        NamespaceAliasDefinitionAST* ast =
                new (_pool) NamespaceAliasDefinitionAST;
        ast->namespace_token = namespace_token;
        ast->namespace_name_token = consumeToken();
        ast->equal_token = consumeToken();
        parseName(ast->name);
        match(T_SEMICOLON, &ast->semicolon_token);
        node = ast;
        return true;
    }

    NamespaceAST* ast = new (_pool) NamespaceAST;
    ast->inline_token = inline_token;
    ast->namespace_token = namespace_token;
    if (LA() == T_IDENTIFIER)
        ast->identifier_token = consumeToken();
    parseOptionalAttributeSpecifierSequence(ast->attribute_list);
    if (LA() == T_LBRACE) {
        parseLinkageBody(ast->linkage_body);
    } else { // attempt to do error recovery
        unsigned pos = cursor();
        for (;LA() != T_EOF_SYMBOL; consumeToken()) {
            switch (LA()) {
            case T_IDENTIFIER:
            case T_POUND:
            case T_POUND_POUND:
            case T___ATTRIBUTE__:
            case T_LPAREN:
            case T_RPAREN:
            case T_DEFAULT:
            case T_PUBLIC:
            case T_PRIVATE:
            case T_PROTECTED:
                continue;
            }
            if (token().isLiteral())
                continue;
            break;
        }
        if (LA() == T_LBRACE && parseLinkageBody(ast->linkage_body))
            warning(pos, "expected '{' before '%s'", _translationUnit->tokenAt(pos).spell());
        else
            rewind(pos);
    }
    node = ast;
    return true;
}

bool Parser::parseUsing(DeclarationAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() != T_USING)
        return false;

    if (LA(2) == T_NAMESPACE)
        return parseUsingDirective(node);

    if (_language.cpp11 && LA(2) == T_IDENTIFIER && parseAliasDeclaration(node))
        return true;

    UsingAST* ast = new (_pool) UsingAST;
    ast->using_token = consumeToken();

    if (LA() == T_TYPENAME)
        ast->typename_token = consumeToken();

    parseName(ast->name);
    match(T_SEMICOLON, &ast->semicolon_token);
    node = ast;
    return true;
}

bool Parser::parseUsingDirective(DeclarationAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() == T_USING && LA(2) == T_NAMESPACE) {
        UsingDirectiveAST* ast = new (_pool) UsingDirectiveAST;
        ast->using_token = consumeToken();
        ast->namespace_token = consumeToken();
        if (! parseName(ast->name))
            warning(cursor(), "expected `namespace name' before `%s'",
                    token().spell());
        match(T_SEMICOLON, &ast->semicolon_token);
        node = ast;
        return true;
    }
    return false;
}

// alias-declaration = 'using' identifier attribute-specifier-seq(opt) '=' type-id ';'
bool Parser::parseAliasDeclaration(DeclarationAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() != T_USING || LA(2) != T_IDENTIFIER)
        return false;

    if (!find(T_EQUAL, T_SEMICOLON))
        return false;

    AliasDeclarationAST* alias = new (_pool) AliasDeclarationAST;
    alias->using_token = consumeToken();
    SimpleNameAST* name = new (_pool) SimpleNameAST;
    name->identifier_token = consumeToken();
    alias->name = name;

    // ### attributes!
    while (LA() != T_EQUAL)
        consumeToken();

    alias->equal_token = consumeToken();

    ExpressionAST* expr = 0;
    parseTypeId(expr);
    if (expr)
        alias->typeId = expr->asTypeId();

    match(T_SEMICOLON, &alias->semicolon_token);

    node = alias;
    return true;
}

bool Parser::parseConversionFunctionId(NameAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() != T_OPERATOR)
        return false;
    unsigned operator_token = consumeToken();
    SpecifierListAST* type_specifier = 0;
    if (! parseTypeSpecifier(type_specifier))
        return false;
    PtrOperatorListAST* ptr_operators = 0, **ptr_operators_tail = &ptr_operators;
    while (parsePtrOperator(*ptr_operators_tail))
        ptr_operators_tail = &(*ptr_operators_tail)->next;

    ConversionFunctionIdAST* ast = new (_pool) ConversionFunctionIdAST;
    ast->operator_token = operator_token;
    ast->type_specifier_list = type_specifier;
    ast->ptr_operator_list = ptr_operators;
    node = ast;
    return true;
}

bool Parser::parseOperatorFunctionId(NameAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() != T_OPERATOR)
        return false;
    unsigned operator_token = consumeToken();

    OperatorAST* op = 0;
    if (! parseOperator(op))
        return false;

    OperatorFunctionIdAST* ast = new (_pool) OperatorFunctionIdAST;
    ast->operator_token = operator_token;
    ast->op = op;
    node = ast;
    return true;
}

Parser::TemplateArgumentListEntry *Parser::templateArgumentListEntry(unsigned tokenIndex)
{
    std::map<unsigned, TemplateArgumentListEntry>::iterator it =_templateArgumentList.find(tokenIndex);
    if (it != _templateArgumentList.end())
        return &it->second;

    return 0;
}

bool Parser::parseTemplateArgumentList(ExpressionListAST* &node)
{
    DEBUG_THIS_RULE();

    if (TemplateArgumentListEntry *entry = templateArgumentListEntry(cursor())) {
        rewind(entry->cursor);
        node = entry->ast;
        return entry->ast != 0;
    }

    unsigned start = cursor();

    ExpressionListAST* *template_argument_ptr = &node;
    ExpressionAST* template_argument = 0;
    const bool cxx11Enabled = _language.cpp11;
    if (parseTemplateArgument(template_argument)) {
        *template_argument_ptr = new (_pool) ExpressionListAST;
        (*template_argument_ptr)->value = template_argument;
        template_argument_ptr = &(*template_argument_ptr)->next;

        if (cxx11Enabled && LA() == T_DOT_DOT_DOT)
            consumeToken(); // ### store this token in the AST

        while (LA() == T_COMMA) {
            consumeToken(); // consume T_COMMA

            if (parseTemplateArgument(template_argument)) {
                *template_argument_ptr = new (_pool) ExpressionListAST;
                (*template_argument_ptr)->value = template_argument;
                template_argument_ptr = &(*template_argument_ptr)->next;

                if (cxx11Enabled && LA() == T_DOT_DOT_DOT)
                    consumeToken(); // ### store this token in the AST
            }
        }

        if (_pool != _translationUnit->memoryPool()) {
            MemoryPool *pool = _translationUnit->memoryPool();
            ExpressionListAST* template_argument_list = node;
            for (ExpressionListAST* iter = template_argument_list, **ast_iter = &node;
                 iter; iter = iter->next, ast_iter = &(*ast_iter)->next)
                *ast_iter = new (pool) ExpressionListAST((iter->value) ? iter->value->clone(pool) : 0);
        }

        _templateArgumentList.insert(std::make_pair(start, TemplateArgumentListEntry(start, cursor(), node)));
        return true;
    }

    _templateArgumentList.insert(std::make_pair(start, TemplateArgumentListEntry(start, cursor(), 0)));
    return false;
}

bool Parser::parseAsmDefinition(DeclarationAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() != T_ASM)
        return false;

    AsmDefinitionAST* ast = new (_pool) AsmDefinitionAST;
    ast->asm_token = consumeToken();

    if (LA() == T_VOLATILE)
        ast->qualifier_token = consumeToken();

    match(T_LPAREN, &ast->lparen_token);
    parseStringLiteral(ast->string_literal);

    if (LA() == T_COLON) {
        consumeToken(); // skip T_COLON
        parseAsmOperandList();
        if (LA() == T_COLON) {
            consumeToken();
            parseAsmOperandList();
            if (LA() == T_COLON) {
                consumeToken();
                parseAsmClobberList();
            }
        } else if (LA() == T_COLON_COLON) {
            consumeToken();
            parseAsmClobberList();
        }
    } else if (LA() == T_COLON_COLON) {
        consumeToken();
        parseAsmOperandList();

        if (LA() == T_COLON) {
          consumeToken();
          parseAsmClobberList();
        }
    }
    match(T_RPAREN, &ast->rparen_token);
    match(T_SEMICOLON, &ast->semicolon_token);
    node = ast;
    return true;
}

bool Parser::parseAsmOperandList()
{
    DEBUG_THIS_RULE();
    if (LA() != T_STRING_LITERAL)
        return true;

    if (parseAsmOperand()) {
        while (LA() == T_COMMA) {
            consumeToken();
            parseAsmOperand();
        }
        return true;
    }

    return false;
}

bool Parser::parseAsmOperand()
{
    DEBUG_THIS_RULE();
    unsigned string_literal_token = 0;
    match(T_STRING_LITERAL, &string_literal_token);

    if (LA() == T_LBRACKET) {
        /*unsigned lbracket_token = */ consumeToken();
        match(T_STRING_LITERAL, &string_literal_token);
        unsigned rbracket_token = 0;
        match(T_RBRACKET, &rbracket_token);
    }

    unsigned lparen_token = 0, rparen_token = 0;
    match(T_LPAREN, &lparen_token);
    ExpressionAST* expression = 0;
    parseExpression(expression);
    match(T_RPAREN, &rparen_token);
    return true;
}

bool Parser::parseAsmClobberList()
{
    DEBUG_THIS_RULE();
    if (LA() != T_STRING_LITERAL)
        return false;

    unsigned string_literal_token = consumeToken();

    while (LA() == T_COMMA) {
        consumeToken();
        match(T_STRING_LITERAL, &string_literal_token);
    }

    return true;
}

bool Parser::parseGenericsDeclaration(DeclarationAST* &node)
{
    DEBUG_THIS_RULE();

    if (! (LA(1) == T_PSYCHEC_TEMPLATE))
        return false;

    GenericsDeclarationAST* ast = new (_pool) GenericsDeclarationAST;

    ast->generics_token = consumeToken();

    while (LA()) {
        unsigned start_declaration = cursor();

        ast->declaration = 0;
        if (parseDeclaration(ast->declaration))
            break;

        error(start_declaration, "expected a declaration");
        rewind(start_declaration + 1);
        skipUntilDeclaration();
    }

    node = ast;
    return true;
}

bool Parser::parseTemplateDeclaration(DeclarationAST* &node)
{
    DEBUG_THIS_RULE();

    if (! (LA(1) == T_TEMPLATE
           || ((LA(1) == T_EXPORT || LA(1) == T_EXTERN)
                && LA(2) == T_TEMPLATE)))
        return false;

    TemplateDeclarationAST* ast = new (_pool) TemplateDeclarationAST;

    if (LA() == T_EXPORT || LA() == T_EXTERN)
        ast->export_token = consumeToken();

    ast->template_token = consumeToken();

    if (LA() == T_LESS) {
        ast->less_token = consumeToken();
        if (maybeSplitGreaterGreaterToken() || LA() == T_GREATER || parseTemplateParameterList(ast->template_parameter_list))
            match(T_GREATER, &ast->greater_token);
    }

    while (LA()) {
        unsigned start_declaration = cursor();

        ast->declaration = 0;
        if (parseDeclaration(ast->declaration))
            break;

        error(start_declaration, "expected a declaration");
        rewind(start_declaration + 1);
        skipUntilDeclaration();
    }

    node = ast;
    return true;
}

bool Parser::parseOperator(OperatorAST* &node) // ### FIXME
{
    DEBUG_THIS_RULE();
    OperatorAST* ast = new (_pool) OperatorAST;

    switch (LA()) {
    case T_NEW:
    case T_DELETE: {
        ast->op_token = consumeToken();
        if (LA() == T_LBRACKET) {
            ast->open_token = consumeToken();
            match(T_RBRACKET, &ast->close_token);
        }
    } break;

    case T_PLUS:
    case T_MINUS:
    case T_STAR:
    case T_SLASH:
    case T_PERCENT:
    case T_CARET:
    case T_AMPER:
    case T_PIPE:
    case T_TILDE:
    case T_EXCLAIM:
    case T_LESS:
    case T_GREATER:
    case T_COMMA:
    case T_AMPER_EQUAL:
    case T_CARET_EQUAL:
    case T_SLASH_EQUAL:
    case T_EQUAL:
    case T_EQUAL_EQUAL:
    case T_EXCLAIM_EQUAL:
    case T_GREATER_EQUAL:
    case T_GREATER_GREATER_EQUAL:
    case T_LESS_EQUAL:
    case T_LESS_LESS_EQUAL:
    case T_MINUS_EQUAL:
    case T_PERCENT_EQUAL:
    case T_PIPE_EQUAL:
    case T_PLUS_EQUAL:
    case T_STAR_EQUAL:
    case T_TILDE_EQUAL:
    case T_LESS_LESS:
    case T_GREATER_GREATER:
    case T_AMPER_AMPER:
    case T_PIPE_PIPE:
    case T_PLUS_PLUS:
    case T_MINUS_MINUS:
    case T_ARROW_STAR:
    case T_DOT_STAR:
    case T_ARROW:
        ast->op_token = consumeToken();
        break;

    default:
        if (LA() == T_LPAREN && LA(2) == T_RPAREN) {
            ast->op_token = ast->open_token = consumeToken();
            ast->close_token = consumeToken();
        } else if (LA() == T_LBRACKET && LA(2) == T_RBRACKET) {
            ast->op_token = ast->open_token = consumeToken();
            ast->close_token = consumeToken();
        } else if (_language.cpp11
                   && LA() == T_STRING_LITERAL
                   && LA(2) == T_IDENTIFIER
                   && !token().f.userDefinedLiteral
                   && token().string->size() == 0
                   && token(2).identifier->size() > 1
                   && token(2).identifier->chars()[0] == '_') {
            // C++11 user-defined literal operator, e.g.:
            // int operator"" _abc123(const char *str, size_t size) { ... }
            ast->op_token = consumeToken();
            consumeToken(); // consume literal operator identifier
        } else {
            return false;
        }
    }

    node = ast;
    return true;
}

bool Parser::parseQualifiers(SpecifierListAST* &node)
{
    DEBUG_THIS_RULE();

    unsigned start = cursor();

    SpecifierListAST* *ast = &node;
    while (*ast)
        ast = &(*ast)->next;

    while (int tk = LA()) {
        if (tk == T_CONST || tk == T_VOLATILE || tk == T_RESTRICT) {
            SimpleSpecifierAST* spec = new (_pool) SimpleSpecifierAST;
            spec->specifier_token = consumeToken();
            *ast = new (_pool) SpecifierListAST(spec);
            ast = &(*ast)->next;
        } else if (parseOptionalAttributeSpecifierSequence(*ast)) {
            continue;
        } else {
            break;
        }
    }

    return start != cursor();
}

bool Parser::parseRefQualifier(unsigned &ref_qualifier)
{
    DEBUG_THIS_RULE();

    if (!_language.cpp11)
        return false;

    if (LA() == T_AMPER || LA() == T_AMPER_AMPER) {
        ref_qualifier = consumeToken();
        return true;
    }

    return false;
}

/**
 * \brief Handles override and final from C++ 2011, they are pseudo keywords and has special meaning only in function declaration
 */
bool Parser::parseOverrideFinalQualifiers(SpecifierListAST* &node)
{
    DEBUG_THIS_RULE();

    if (!_language.cpp11)
        return false;

    unsigned start = cursor();

    SpecifierListAST* *ast = &node;
    while (*ast)
        ast = &(*ast)->next;

    while (LA() == T_IDENTIFIER) {
        const Identifier &id = *(_translationUnit->tokenAt(cursor()).identifier);

        if (id.equalTo(_control->cpp11Override())
                || id.equalTo(_control->cpp11Final())) {
            SimpleSpecifierAST* spec = new (_pool) SimpleSpecifierAST;
            spec->specifier_token = consumeToken();
            *ast = new (_pool) SpecifierListAST(spec);
            ast = &(*ast)->next;
        }
        else {
            break;
        }
    }

    return (start != cursor());
}

bool Parser::parsePtrOperator(PtrOperatorListAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() == T_AMPER || (_language.cpp11 && LA() == T_AMPER_AMPER)) {
        ReferenceAST* ast = new (_pool) ReferenceAST;
        ast->reference_token = consumeToken();
        node = new (_pool) PtrOperatorListAST(ast);
        return true;
    } else if (LA() == T_STAR) {
        PointerAST* ast = new (_pool) PointerAST;
        ast->star_token = consumeToken();
        parseQualifiers(ast->cv_qualifier_list);
        node = new (_pool) PtrOperatorListAST(ast);
        return true;
    } else if (LA() == T_COLON_COLON || LA() == T_IDENTIFIER) {
        unsigned scope_or_identifier_token = cursor();

        unsigned global_scope_token = 0;
        if (LA() == T_COLON_COLON)
            global_scope_token = consumeToken();

        NestedNameSpecifierListAST* nested_name_specifiers = 0;
        bool has_nested_name_specifier = parseNestedNameSpecifier(nested_name_specifiers, true);
        if (has_nested_name_specifier && LA() == T_STAR) {
            PointerToMemberAST* ast = new (_pool) PointerToMemberAST;
            ast->global_scope_token = global_scope_token;
            ast->nested_name_specifier_list = nested_name_specifiers;
            ast->star_token = consumeToken();
            parseQualifiers(ast->cv_qualifier_list);
            node = new (_pool) PtrOperatorListAST(ast);
            return true;
        }
        rewind(scope_or_identifier_token);
    }
    return false;
}

bool Parser::parseTemplateArgument(ExpressionAST* &node)
{
    DEBUG_THIS_RULE();
    unsigned start = cursor();
    if (parseTypeId(node)) {
        int index = 1;

        if (_language.cpp11 && LA() == T_DOT_DOT_DOT)
            index = 2;

        if (LA(index) == T_COMMA || maybeSplitGreaterGreaterToken(index) || LA(index) == T_GREATER)
            return true;
    }

    rewind(start);
    bool previousTemplateArguments = switchTemplateArguments(true);
    bool parsed = parseConstantExpression(node);
    (void) switchTemplateArguments(previousTemplateArguments);
    return parsed;
}

/** Parses a sequence of
 *
 * storage-class-specifier (unless noStorageSpecifier is true)
 * trailing-type-specifier, which contains
 *     cv-qualifier
 *     simple-type-specifier
 *     typename-specifier
 *     elaborated-type-specifier
 *
 * If onlySimpleTypeSpecifiers is true, it only eats simple-type-specifiers
 * and cv-qualifiers.
 */
bool Parser::parseDeclSpecifierSeq(SpecifierListAST* &decl_specifier_seq,
                                   bool noStorageSpecifiers,
                                   bool onlySimpleTypeSpecifiers)
{
    DEBUG_THIS_RULE();

    bool has_type_specifier = false;
    NameAST* named_type_specifier = 0;
    SpecifierListAST* *decl_specifier_seq_ptr = &decl_specifier_seq;
    for (;;) {
        if (! noStorageSpecifiers && ! onlySimpleTypeSpecifiers && lookAtStorageClassSpecifier()) {
            // storage-class-specifier
            SimpleSpecifierAST* spec = new (_pool) SimpleSpecifierAST;
            spec->specifier_token = consumeToken();
            *decl_specifier_seq_ptr = new (_pool) SpecifierListAST(spec);
            decl_specifier_seq_ptr = &(*decl_specifier_seq_ptr)->next;
        } else if (lookAtQualifier()) {
            // cv-qualifier
            SimpleSpecifierAST* spec = new (_pool) SimpleSpecifierAST;
            spec->specifier_token = consumeToken();
            *decl_specifier_seq_ptr = new (_pool) SpecifierListAST(spec);
            decl_specifier_seq_ptr = &(*decl_specifier_seq_ptr)->next;
        } else if (! named_type_specifier && lookAtBuiltinTypeSpecifier()) {
            // parts of simple-type-specifier
            parseBuiltinTypeSpecifier(*decl_specifier_seq_ptr);
            decl_specifier_seq_ptr = &(*decl_specifier_seq_ptr)->next;
            has_type_specifier = true;
        } else if (! has_type_specifier && (LA() == T_COLON_COLON ||
                                            LA() == T_IDENTIFIER)) {
            // parts of simple-type-specifier
            if (! parseName(named_type_specifier))
                return false;
            NamedTypeSpecifierAST* spec = new (_pool) NamedTypeSpecifierAST;
            spec->name = named_type_specifier;
            *decl_specifier_seq_ptr = new (_pool) SpecifierListAST(spec);
            decl_specifier_seq_ptr = &(*decl_specifier_seq_ptr)->next;
            has_type_specifier = true;
        } else if (! onlySimpleTypeSpecifiers && ! has_type_specifier) {
            unsigned startOfTypeSpecifier = cursor();
            if ((LA() == T_TYPENAME || LA() == T_ENUM || lookAtClassKey())
                    && (! parseElaboratedTypeSpecifier(*decl_specifier_seq_ptr))) {
                error(startOfTypeSpecifier, "expected an elaborated type specifier");
                break;
            } else if ((LA() == T_PSYCHEC_FORALL || LA() == T_PSYCHEC_EXISTS)
                       && (! parseQuantifiedTypeSpecifier(*decl_specifier_seq_ptr))) {
                error(startOfTypeSpecifier, "expected a quantified type specifier");
                break;
            }
            decl_specifier_seq_ptr = &(*decl_specifier_seq_ptr)->next;
            has_type_specifier = true;
        } else
            break;
    }

    return decl_specifier_seq != 0;
}

bool Parser::parseDeclaratorOrAbstractDeclarator(DeclaratorAST* &node, SpecifierListAST* decl_specifier_list)
{
    DEBUG_THIS_RULE();
    unsigned start = cursor();
    bool blocked = blockErrors(true);
    if (parseDeclarator(node, decl_specifier_list)) {
        blockErrors(blocked);
        return true;
    }
    blockErrors(blocked);
    rewind(start);
    return parseAbstractDeclarator(node, decl_specifier_list);
}

bool Parser::parseCoreDeclarator(DeclaratorAST* &node, SpecifierListAST* decl_specifier_list, ClassSpecifierAST* )
{
    DEBUG_THIS_RULE();
    unsigned start = cursor();
    SpecifierListAST* attributes = 0;
    parseOptionalAttributeSpecifierSequence(attributes);

    PtrOperatorListAST* ptr_operators = 0, **ptr_operators_tail = &ptr_operators;
    while (parsePtrOperator(*ptr_operators_tail))
        ptr_operators_tail = &(*ptr_operators_tail)->next;

    if (LA() == T_COLON_COLON || LA() == T_IDENTIFIER || LA() == T_TILDE || LA() == T_OPERATOR
        || (_language.cpp11 && LA() == T_DOT_DOT_DOT && (LA(2) == T_COLON_COLON || LA(2) == T_IDENTIFIER))) {

        unsigned dot_dot_dot_token = 0;

        if (LA() == T_DOT_DOT_DOT)
            dot_dot_dot_token = consumeToken();

        NameAST* name = 0;
        if (parseName(name)) {
            DeclaratorIdAST* declarator_id = new (_pool) DeclaratorIdAST;
            declarator_id->dot_dot_dot_token = dot_dot_dot_token;
            declarator_id->name = name;
            DeclaratorAST* ast = new (_pool) DeclaratorAST;
            ast->attribute_list = attributes;
            ast->ptr_operator_list = ptr_operators;
            ast->core_declarator = declarator_id;
            node = ast;
            return true;
        }
    } else if (decl_specifier_list && LA() == T_LPAREN) {
        if (attributes)
            warning(attributes->firstToken(), "unexpected attribtues");

        unsigned lparen_token = consumeToken();
        DeclaratorAST* declarator = 0;
        if (parseDeclarator(declarator, decl_specifier_list) && LA() == T_RPAREN) {
            NestedDeclaratorAST* nested_declarator = new (_pool) NestedDeclaratorAST;
            nested_declarator->lparen_token = lparen_token;
            nested_declarator->declarator = declarator;
            nested_declarator->rparen_token = consumeToken();
            DeclaratorAST* ast = new (_pool) DeclaratorAST;
            ast->ptr_operator_list = ptr_operators;
            ast->core_declarator = nested_declarator;
            node = ast;
            return true;
        }
    }
    rewind(start);
    return false;
}

static bool maybeCppInitializer(DeclaratorAST* declarator)
{
    if (declarator->ptr_operator_list)
        return false;
    CoreDeclaratorAST* core_declarator = declarator->core_declarator;
    if (! core_declarator)
        return false;
    DeclaratorIdAST* declarator_id = core_declarator->asDeclaratorId();
    if (! declarator_id)
        return false;
    else if (! declarator_id->name)
        return false;
    else if (! declarator_id->name->asSimpleName())
        return false;

    return true;
}

bool Parser::parseDeclarator(DeclaratorAST* &node, SpecifierListAST* decl_specifier_list, ClassSpecifierAST* declaringClass)
{
    DEBUG_THIS_RULE();
    if (! parseCoreDeclarator(node, decl_specifier_list, declaringClass))
        return false;

    PostfixDeclaratorListAST* *postfix_ptr = &node->postfix_declarator_list;

    for (;;) {
        unsigned startOfPostDeclarator = cursor();

        if (LA() == T_LPAREN) {
            if (! declaringClass && LA(2) != T_RPAREN && maybeCppInitializer(node)) {
                unsigned lparen_token = cursor();
                ExpressionAST* initializer = 0;

                bool blocked = blockErrors(true);
                if (parseInitializer(initializer, &node->equal_token)) {
                    // maybe the initializer also parses as a FunctionDeclarator?
                    ExpressionListParenAST* expr = 0;
                    if (initializer)
                        expr = initializer->asExpressionListParen();
                    if (expr) {
                        if (expr->expression_list && expr->rparen_token && (LA() == T_COMMA || LA() == T_SEMICOLON)) {
                            rewind(lparen_token);

                            // check for ambiguous declarators.

                            consumeToken();
                            ParameterDeclarationClauseAST* parameter_declaration_clause = 0;
                            if (parseParameterDeclarationClause(parameter_declaration_clause)
                                    && LA() == T_RPAREN) {
                                FunctionDeclaratorAST* ast = new (_pool) FunctionDeclaratorAST;
                                unsigned rparen_token = consumeToken();
                                if (LA() == T_PSYCHEC_OMISSION_MARKER)
                                    ast->psychec_omission_token = consumeToken();
                                ast->lparen_token = lparen_token;
                                ast->parameter_declaration_clause = parameter_declaration_clause;
                                ast->as_cpp_initializer = initializer;
                                ast->rparen_token = rparen_token;
                                *postfix_ptr = new (_pool) PostfixDeclaratorListAST(ast);

                                blockErrors(blocked);
                                return true;
                            }

                            blockErrors(blocked);
                            rewind(lparen_token);
                            return true;
                        }
                    }
                }

                blockErrors(blocked);
                rewind(lparen_token);
            }

            FunctionDeclaratorAST* ast = new (_pool) FunctionDeclaratorAST;
            ast->lparen_token = consumeToken();
            parseParameterDeclarationClause(ast->parameter_declaration_clause);
            if (LA() != T_RPAREN) {
                rewind(startOfPostDeclarator);
                break;
            }

            ast->rparen_token = consumeToken();
            if (LA() == T_PSYCHEC_OMISSION_MARKER)
                ast->psychec_omission_token = consumeToken();

            // ### parse attributes
            parseQualifiers(ast->cv_qualifier_list);
            parseRefQualifier(ast->ref_qualifier_token);
            parseExceptionSpecification(ast->exception_specification);

            if (_language.cpp11 && ! node->ptr_operator_list && LA() == T_ARROW) {
                // only allow if there is 1 type spec, which has to be 'auto'
                bool hasAuto = false;
                for (SpecifierListAST* iter = decl_specifier_list; !hasAuto && iter; iter = iter->next) {
                    SpecifierAST* spec = iter->value;
                    if (SimpleSpecifierAST* simpleSpec = spec->asSimpleSpecifier()) {
                        if (_translationUnit->tokenKind(simpleSpec->specifier_token) == T_AUTO)
                            hasAuto = true;
                    }
                }

                if (hasAuto)
                    parseTrailingReturnType(ast->trailing_return_type);
            }

            parseOverrideFinalQualifiers(ast->cv_qualifier_list);

            *postfix_ptr = new (_pool) PostfixDeclaratorListAST(ast);
            postfix_ptr = &(*postfix_ptr)->next;
        } else if (LA() == T_LBRACKET) {
            ArrayDeclaratorAST* ast = new (_pool) ArrayDeclaratorAST;
            ast->lbracket_token = consumeToken();
            if (LA() == T_RBRACKET || parseConstantExpression(ast->expression))
                match(T_RBRACKET, &ast->rbracket_token);
            *postfix_ptr = new (_pool) PostfixDeclaratorListAST(ast);
            postfix_ptr = &(*postfix_ptr)->next;
        } else
            break;
    }

    if (LA() == T___ASM__ && LA(2) == T_LPAREN) { // ### store the asm specifier in the AST
        consumeToken(); // skip __asm__
        consumeToken(); // skip T_LPAREN

        if (skipUntil(T_RPAREN))
            consumeToken(); // skip T_RPAREN
    }

    parseOptionalAttributeSpecifierSequence(node->post_attribute_list);
    return true;
}

bool Parser::parseAbstractCoreDeclarator(DeclaratorAST* &node, SpecifierListAST* decl_specifier_list)
{
    DEBUG_THIS_RULE();

    PtrOperatorListAST* ptr_operators = 0, **ptr_operators_tail = &ptr_operators;
    while (parsePtrOperator(*ptr_operators_tail))
        ptr_operators_tail = &(*ptr_operators_tail)->next;

    unsigned after_ptr_operators = cursor();

    if (LA() == T_LPAREN && LA(2) != T_RPAREN) {
        unsigned lparen_token = consumeToken();
        DeclaratorAST* declarator = 0;
        if (parseAbstractDeclarator(declarator, decl_specifier_list) && LA() == T_RPAREN) {
            NestedDeclaratorAST* nested_declarator = new (_pool) NestedDeclaratorAST;
            nested_declarator->lparen_token = lparen_token;
            nested_declarator->declarator = declarator;
            nested_declarator->rparen_token = consumeToken();
            DeclaratorAST* ast = new (_pool) DeclaratorAST;
            ast->ptr_operator_list = ptr_operators;
            ast->core_declarator = nested_declarator;
            node = ast;
            return true;
        }
    }

    rewind(after_ptr_operators);
    if (ptr_operators) {
        DeclaratorAST* ast = new (_pool) DeclaratorAST;
        ast->ptr_operator_list = ptr_operators;
        node = ast;
    }

    return true;
}

bool Parser::parseAbstractDeclarator(DeclaratorAST* &node, SpecifierListAST* decl_specifier_list)
{
    DEBUG_THIS_RULE();

    if (! parseAbstractCoreDeclarator(node, decl_specifier_list))
        return false;

    PostfixDeclaratorListAST* postfix_declarators = 0;
    PostfixDeclaratorListAST** postfix_ptr = &postfix_declarators;

    for (;;) {
        if (LA() == T_LPAREN) {
            FunctionDeclaratorAST* ast = new (_pool) FunctionDeclaratorAST;
            ast->lparen_token = consumeToken();
            if (LA() == T_RPAREN || parseParameterDeclarationClause(ast->parameter_declaration_clause)) {
                if (LA() == T_RPAREN) {
                    ast->rparen_token = consumeToken();
                    if (LA() == T_PSYCHEC_OMISSION_MARKER)
                        ast->psychec_omission_token = consumeToken();
                }
            }
            parseQualifiers(ast->cv_qualifier_list);
            parseRefQualifier(ast->ref_qualifier_token);
            parseExceptionSpecification(ast->exception_specification);
            *postfix_ptr = new (_pool) PostfixDeclaratorListAST(ast);
            postfix_ptr = &(*postfix_ptr)->next;
        } else if (LA() == T_LBRACKET) {
            ArrayDeclaratorAST* ast = new (_pool) ArrayDeclaratorAST;
            ast->lbracket_token = consumeToken();
            if (LA() == T_RBRACKET || parseConstantExpression(ast->expression)) {
                if (LA() == T_RBRACKET)
                    ast->rbracket_token = consumeToken();
            }
            *postfix_ptr = new (_pool) PostfixDeclaratorListAST(ast);
            postfix_ptr = &(*postfix_ptr)->next;
        } else
            break;
    }

    if (postfix_declarators) {
        if (! node)
            node = new (_pool) DeclaratorAST;

        node->postfix_declarator_list = postfix_declarators;
    }

    return true;
}

/**
 * @brief Reads enumeration type declaration, examples:
 * @code
    enum {
        debug = 1
    };
    enum class Format {
        FormatPNG,
        FormatJPEG
    };
 * @endcode
 */
bool Parser::parseEnumSpecifier(SpecifierListAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() == T_ENUM) {
        EnumSpecifierAST* ast = new (_pool) EnumSpecifierAST;

        ast->enum_token = consumeToken();
        if (_language.cpp11 && (LA() == T_CLASS || LA() == T_STRUCT))
            ast->key_token = consumeToken();


        if (token().isKeyword()) {
            error(cursor(), "expected identifier before '%s'", token().spell());
            return false;
        }
        parseName(ast->name);

        if (_language.cpp11 && LA() == T_COLON) {
            ast->colon_token = consumeToken();
            parseTypeSpecifier(ast->type_specifier_list);
        }
        if (LA() == T_LBRACE) {
            ast->lbrace_token = consumeToken();
            EnumeratorListAST** enumerator_ptr = &ast->enumerator_list;
            unsigned dummy;
            while (int tk = LA()) {
                if (tk == T_RBRACE)
                    break;

                if (LA() != T_IDENTIFIER) {
                    error(cursor(), "expected identifier before '%s'", token().spell());
                    skipUntil(T_IDENTIFIER);
                }

                auto comma_token = &dummy;
                if (parseEnumerator(*enumerator_ptr)) {
                    comma_token = &(*enumerator_ptr)->delim_token;
                    enumerator_ptr = &(*enumerator_ptr)->next;
                }

                if (LA() == T_COMMA && LA(2) == T_RBRACE)
                    ast->stray_comma_token = consumeToken();

                if (LA() != T_RBRACE)
                    match(T_COMMA, comma_token);
            }
            match(T_RBRACE, &ast->rbrace_token);
        } else if (!_language.cpp11) {
            return false;
        }

        node = new (_pool) SpecifierListAST(ast);
        return true;
    }
    return false;
}

bool Parser::parseTemplateParameterList(DeclarationListAST* &node)
{
    DEBUG_THIS_RULE();
    DeclarationListAST* *template_parameter_ptr = &node;
    DeclarationAST* declaration = 0;
    if (parseTemplateParameter(declaration)) {
        *template_parameter_ptr = new (_pool) DeclarationListAST;
        (*template_parameter_ptr)->value = declaration;
        template_parameter_ptr = &(*template_parameter_ptr)->next;

        while (LA() == T_COMMA) {
            consumeToken(); // XXX Store this token somewhere

            declaration = 0;
            if (parseTemplateParameter(declaration)) {
                *template_parameter_ptr = new (_pool) DeclarationListAST;
                (*template_parameter_ptr)->value = declaration;
                template_parameter_ptr = &(*template_parameter_ptr)->next;
            }
        }
        return true;
    }
    return false;
}

bool Parser::parseTemplateParameter(DeclarationAST* &node)
{
    DEBUG_THIS_RULE();
    if (parseTypeParameter(node))
        return true;
    bool previousTemplateArguments = switchTemplateArguments(true);
    ParameterDeclarationAST* ast = 0;
    bool parsed = parseParameterDeclaration(ast);
    node = ast;
    (void) switchTemplateArguments(previousTemplateArguments);
    return parsed;
}

bool Parser::parseTypenameTypeParameter(DeclarationAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() == T_CLASS || LA() == T_TYPENAME) {
        TypenameTypeParameterAST* ast = new (_pool) TypenameTypeParameterAST;
        ast->classkey_token = consumeToken();
        if (_language.cpp11 && LA() == T_DOT_DOT_DOT)
            ast->dot_dot_dot_token = consumeToken();
        parseName(ast->name);
        if (LA() == T_EQUAL) {
            ast->equal_token = consumeToken();
            parseTypeId(ast->type_id);
        }
        node = ast;
        return true;
    }
    return false;
}

bool Parser::parseTemplateTypeParameter(DeclarationAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() == T_TEMPLATE) {
        TemplateTypeParameterAST* ast = new (_pool) TemplateTypeParameterAST;
        ast->template_token = consumeToken();
        if (LA() == T_LESS)
            ast->less_token = consumeToken();
        parseTemplateParameterList(ast->template_parameter_list);
        if (maybeSplitGreaterGreaterToken() || LA() == T_GREATER)
            ast->greater_token = consumeToken();
        if (LA() == T_CLASS)
            ast->class_token = consumeToken();
        if (_language.cpp11 && LA() == T_DOT_DOT_DOT)
            ast->dot_dot_dot_token = consumeToken();

        // parse optional name
        parseName(ast->name);

        if (LA() == T_EQUAL) {
            ast->equal_token = consumeToken();
            parseTypeId(ast->type_id);
        }
        node = ast;
        return true;
    }
    return false;
}

bool Parser::lookAtTypeParameter()
{
    if (LA() == T_CLASS || LA() == T_TYPENAME) {
        if (LA(2) == T_IDENTIFIER) {
            switch (LA(3)) {
            case T_EQUAL:
            case T_COMMA:
            case T_GREATER:
                return true;

            default:
                return maybeSplitGreaterGreaterToken(3);
            }
        } else if (LA(2) == T_COLON_COLON) {
            // found something like template <typename ::foo::bar>...
            return false;
        }

        // recognized an anonymous template type parameter. e.g
        //    template <typename>
        return true;
    }

    return false;
}


bool Parser::parseTypeParameter(DeclarationAST* &node)
{
    DEBUG_THIS_RULE();

    if (lookAtTypeParameter())
        return parseTypenameTypeParameter(node);
    else if (LA() == T_TEMPLATE)
        return parseTemplateTypeParameter(node);
    else
        return false;
}

bool Parser::parseTypeId(ExpressionAST* &node)
{
    DEBUG_THIS_RULE();
    CHECK_CACHE(ASTCache::TypeId, ExpressionAST);

    SpecifierListAST* type_specifier = 0;
    if (parseTypeSpecifier(type_specifier)) {
        TypeIdAST* ast = new (_pool) TypeIdAST;
        ast->type_specifier_list = type_specifier;
        parseAbstractDeclarator(ast->declarator, type_specifier);
        node = ast;
        return true;
    }
    return false;
}

bool Parser::parseParameterDeclarationClause(ParameterDeclarationClauseAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() == T_RPAREN)
        return true; // nothing to do
    CHECK_CACHE(ASTCache::ParameterDeclarationClause, ParameterDeclarationClauseAST);
    const unsigned initialCursor = cursor();

    ParameterDeclarationListAST* parameter_declarations = 0;

    unsigned dot_dot_dot_token = 0;
    if (LA() == T_DOT_DOT_DOT)
        dot_dot_dot_token = consumeToken();
    else {
        parseParameterDeclarationList(parameter_declarations);

        if (LA() == T_DOT_DOT_DOT) {
            dot_dot_dot_token = consumeToken();
        } else if (LA() == T_COMMA && LA(2) == T_DOT_DOT_DOT) {
            consumeToken(); // skip comma
            dot_dot_dot_token = consumeToken();
        }
    }

    if (parameter_declarations || dot_dot_dot_token) {
        ParameterDeclarationClauseAST* ast = new (_pool) ParameterDeclarationClauseAST;
        ast->parameter_declaration_list = parameter_declarations;
        ast->dot_dot_dot_token = dot_dot_dot_token;
        node = ast;
    }

    const bool result = true;
    _astCache->insert(ASTCache::ParameterDeclarationClause, initialCursor, node, cursor(), result);
    return result;
}

bool Parser::parseParameterDeclarationList(ParameterDeclarationListAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() == T_DOT_DOT_DOT || (LA() == T_COMMA && LA(2) == T_DOT_DOT_DOT))
        return false; // nothing to do.

    ParameterDeclarationListAST* *parameter_declaration_ptr = &node;
    ParameterDeclarationAST* declaration = 0;
    if (parseParameterDeclaration(declaration)) {
        *parameter_declaration_ptr = new (_pool) ParameterDeclarationListAST;
        (*parameter_declaration_ptr)->value = declaration;
        auto comma_token = &(*parameter_declaration_ptr)->delim_token;
        parameter_declaration_ptr = &(*parameter_declaration_ptr)->next;
        while (LA() == T_COMMA) {
            *comma_token = consumeToken();

            if (LA() == T_DOT_DOT_DOT)
                break;

            declaration = 0;
            if (parseParameterDeclaration(declaration)) {
                *parameter_declaration_ptr = new (_pool) ParameterDeclarationListAST;
                (*parameter_declaration_ptr)->value = declaration;
                comma_token = &(*parameter_declaration_ptr)->delim_token;
                parameter_declaration_ptr = &(*parameter_declaration_ptr)->next;
            }
        }
        return true;
    }
    return false;
}

bool Parser::parseParameterDeclaration(ParameterDeclarationAST* &node)
{
    DEBUG_THIS_RULE();
    SpecifierListAST* decl_specifier_seq = 0;
    if (parseDeclSpecifierSeq(decl_specifier_seq)) {
        ParameterDeclarationAST* ast = new (_pool) ParameterDeclarationAST;
        ast->type_specifier_list = decl_specifier_seq;
        parseDeclaratorOrAbstractDeclarator(ast->declarator, decl_specifier_seq);
        if (LA() == T_EQUAL) {
            ast->equal_token = consumeToken();
            if (!_language.cpp11)
                parseLogicalOrExpression(ast->expression);
            else
                parseInitializerClause0x(ast->expression);
        }

        node = ast;
        return true;
    }
    return false;
}

const Identifier* Parser::className(ClassSpecifierAST* ast) const
{
  if (! ast)
    return 0;

  return identifier(ast->name);
}

const Identifier* Parser::identifier(NameAST* name) const
{
  if (! name)
    return 0;

  if (QualifiedNameAST* q = name->asQualifiedName())
    name = q->unqualified_name;

  if (name) {
    if (SimpleNameAST* simple = name->asSimpleName())
      return _translationUnit->identifier(simple->identifier_token);
    else if (TemplateIdAST* template_id = name->asTemplateId())
      return _translationUnit->identifier(template_id->identifier_token);
  }

  return 0;
}

bool Parser::parseClassSpecifier(SpecifierListAST* &node)
{
    DEBUG_THIS_RULE();
    if (! lookAtClassKey())
        return false;

    unsigned classkey_token = consumeToken();

    SpecifierListAST* attributes = 0;
    parseOptionalAttributeSpecifierSequence(attributes);

    if (LA(1) == T_IDENTIFIER && LA(2) == T_IDENTIFIER) {
        const Identifier* id = token(2).identifier;
        if (!id->equalTo(_control->cpp11Final())) {
            warning(cursor(), "skip identifier `%s'",
                                      token().spell());
            consumeToken();
        }
    }

    NameAST* name = 0;
    parseName(name);

    bool parsed = false;

    const bool previousInFunctionBody = _inFunctionBody;
    _inFunctionBody = false;

    unsigned colon_token = 0;
    unsigned dot_dot_dot_token = 0;
    unsigned final_token = 0;

    if (LA() == T_IDENTIFIER) {
        const Identifier* id = token().identifier;
        if (id->equalTo(_control->cpp11Final()))
            final_token = consumeToken();
    }

    if (LA() == T_COLON || LA() == T_LBRACE) {
        if (!name) {
            EmptyNameAST* anonName = new (_pool) EmptyNameAST;
            name = anonName;
        }

        BaseSpecifierListAST* base_clause_list = 0;

        if (LA() == T_COLON) {
            colon_token = cursor();

            parseBaseClause(base_clause_list);

            if (_language.cpp11 && LA() == T_DOT_DOT_DOT)
                dot_dot_dot_token = consumeToken();

            if (LA() != T_LBRACE) {
                error(cursor(), "expected `{' before `%s'", token().spell());

                const unsigned saved = cursor();

                for (int n = 0; n < 3 && LA() != T_EOF_SYMBOL; ++n, consumeToken()) {
                    if (LA() == T_LBRACE)
                        break;
                }

                if (LA() != T_LBRACE)
                    rewind(saved);
            }
        }

        ClassSpecifierAST* ast = new (_pool) ClassSpecifierAST;
        ast->classkey_token = classkey_token;
        ast->attribute_list = attributes;
        ast->final_token = final_token;
        ast->name = name;
        ast->colon_token = colon_token;
        ast->base_clause_list = base_clause_list;
        ast->dot_dot_dot_token = dot_dot_dot_token;

        if (LA() == T_LBRACE)
            ast->lbrace_token = consumeToken();

        DeclarationListAST* *declaration_ptr = &ast->member_specifier_list;
        while (int tk = LA()) {
            if (tk == T_RBRACE) {
                ast->rbrace_token = consumeToken();
                break;
            }

            unsigned start_declaration = cursor();
            DeclarationAST* declaration = 0;
            if (parseMemberSpecification(declaration, ast)) {
                if (declaration) {  // paranoia check
                    *declaration_ptr = new (_pool) DeclarationListAST;
                    (*declaration_ptr)->value = declaration;
                    declaration_ptr = &(*declaration_ptr)->next;
                }

                if (cursor() == start_declaration) { // more paranoia
                    rewind(start_declaration + 1);
                    skipUntilDeclaration();
                }
            } else {
                error(start_declaration, "expected a declaration");
                rewind(start_declaration + 1);
                skipUntilDeclaration();
            }
        }
        node = new (_pool) SpecifierListAST(ast);
        parsed = true;
    }

    _inFunctionBody = previousInFunctionBody;

    return parsed;
}

bool Parser::parseAccessDeclaration(DeclarationAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() == T_PUBLIC || LA() == T_PROTECTED || LA() == T_PRIVATE) {
        AccessDeclarationAST* ast = new (_pool) AccessDeclarationAST;
        ast->access_specifier_token = consumeToken();
        match(T_COLON, &ast->colon_token);
        node = ast;
        return true;
    }
    return false;
}

bool Parser::parseMemberSpecification(DeclarationAST* &node, ClassSpecifierAST* declaringClass)
{
    DEBUG_THIS_RULE();
    switch (LA()) {
    case T_SEMICOLON:
        return parseEmptyDeclaration(node);

    case T_USING:
        return parseUsing(node);

    case T_TEMPLATE:
        return parseTemplateDeclaration(node);

    case T_PUBLIC:
    case T_PROTECTED:
    case T_PRIVATE:
        return parseAccessDeclaration(node);

    case T_STATIC_ASSERT:
        if (_language.cpp11)
            return parseStaticAssertDeclaration(node);
        // fall-through

    default:
        return parseSimpleDeclaration(node, declaringClass);
    } // switch
}

bool Parser::parseCtorInitializer(CtorInitializerAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() == T_COLON) {
        unsigned colon_token = consumeToken();

        CtorInitializerAST* ast = new (_pool) CtorInitializerAST;
        ast->colon_token = colon_token;

        parseMemInitializerList(ast->member_initializer_list);

        if (_language.cpp11 && LA() == T_DOT_DOT_DOT)
            ast->dot_dot_dot_token = consumeToken();

        node = ast;
        return true;
    }
    return false;
}

bool Parser::parseQuantifiedTypeSpecifier(SpecifierListAST* &node)
{
    DEBUG_THIS_RULE();

    if (! (LA() == T_PSYCHEC_EXISTS || LA() == T_PSYCHEC_FORALL))
        return false;

    QuantifiedTypeSpecifierAST* ast = new (_pool) QuantifiedTypeSpecifierAST;
    node = new (_pool) SpecifierListAST(ast);

    ast->quantifier_token = consumeToken();
    match(T_LPAREN, &ast->lparen_token);

    unsigned classkey_token = 0;
    if (LA() == T_STRUCT)
        classkey_token = consumeToken();

    unsigned startOfName = cursor();
    NameAST* name = 0;
    if (parseName(name)) {
        match(T_RPAREN, &ast->rparen_token);
        if (classkey_token) {
            TaggedNameAST* elabName = new (_pool) TaggedNameAST;
            elabName->tag_token = classkey_token;
            elabName->core_name = name;
            ast->name = elabName;
        } else {
            ast->name = name;
        }
        return true;
    }

    error(startOfName, "invalid quantified name");
    if (skipUntil(T_RPAREN))
        consumeToken();

    return true;
}

bool Parser::parseElaboratedTypeSpecifier(SpecifierListAST* &node)
{
    DEBUG_THIS_RULE();

    if (lookAtClassKey() || LA() == T_ENUM || LA() == T_TYPENAME) {
        unsigned classkey_token = consumeToken();

        SpecifierListAST* attributes = 0;
        parseOptionalAttributeSpecifierSequence(attributes);
        NameAST* name = 0;
        if (parseName(name)) {
            ElaboratedTypeSpecifierAST* ast = new (_pool) ElaboratedTypeSpecifierAST;
            ast->attribute_list = attributes;
            TaggedNameAST* elabName = new (_pool) TaggedNameAST;
            elabName->tag_token = classkey_token;
            elabName->core_name = name;
            ast->name = elabName;
            node = new (_pool) SpecifierListAST(ast);
            return true;
        }
    }

    return false;
}

bool Parser::parseExceptionSpecification(ExceptionSpecificationAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() == T_THROW) {
        DynamicExceptionSpecificationAST* ast = new (_pool) DynamicExceptionSpecificationAST;
        ast->throw_token = consumeToken();
        if (LA() == T_LPAREN)
            ast->lparen_token = consumeToken();
        if (LA() == T_DOT_DOT_DOT)
            ast->dot_dot_dot_token = consumeToken();
        else
            parseTypeIdList(ast->type_id_list);
        if (LA() == T_RPAREN)
            ast->rparen_token = consumeToken();
        node = ast;
        return true;
    } else if (_language.cpp11 && LA() == T_NOEXCEPT) {
        NoExceptSpecificationAST* ast = new (_pool) NoExceptSpecificationAST;
        ast->noexcept_token = consumeToken();
        if (LA() == T_LPAREN) {
            ast->lparen_token = consumeToken();
            parseConstantExpression(ast->expression);
            match(T_RPAREN, &ast->rparen_token);
        }
        node = ast;
        return true;
    }
    return false;
}

bool Parser::parseEnumerator(EnumeratorListAST* &node)
{
    DEBUG_THIS_RULE();

    if (LA() == T_IDENTIFIER) {
        EnumeratorAST* ast = new (_pool) EnumeratorAST;
        ast->identifier_token = consumeToken();

        if (LA() == T___ATTRIBUTE__) {
            SpecifierListAST** attr = &ast->attribute_list;
            while (parseGnuAttributeSpecifier(*attr))
                attr = &(*attr)->next;
            if (!_language.ext_EnumeratorAttributes)
                    warning((*attr)->value->firstToken(), "enumerator-attributes are an extension");
        }

        if (LA() == T_EQUAL) {
            ast->equal_token = consumeToken();
            parseConstantExpression(ast->expression);
        }

        node = new (_pool) EnumeratorListAST;
        node->value = ast;
        return true;
    }
    return false;
}

bool Parser::parseInitDeclarator(DeclaratorAST* &node,
                                 SpecifierListAST* decl_specifier_list,
                                 ClassSpecifierAST* declaringClass) // ### rewrite me
{
    DEBUG_THIS_RULE();

    if (declaringClass && LA() == T_COLON) {
        // anonymous bit-field declaration.

    } else if (! parseDeclarator(node, decl_specifier_list, declaringClass)) {
        return false;
    }

    if (LA() == T_ASM && LA(2) == T_LPAREN) { // ### FIXME
        consumeToken();

        if (skip(T_LPAREN, T_RPAREN))
            consumeToken();
    }

    const bool isFunctionDeclarator = node
            && node->postfix_declarator_list
            && node->postfix_declarator_list->lastValue()
            && node->postfix_declarator_list->lastValue()->asFunctionDeclarator();
    if (declaringClass
            && LA() == T_COLON
            && (! node || ! node->postfix_declarator_list)) {
        unsigned colon_token = consumeToken();
        ExpressionAST* expression = 0;
        if (parseConstantExpression(expression)
                && (LA() == T_COMMA
                    || LA() == T_SEMICOLON)) {
            if (! node)
                node = new (_pool) DeclaratorAST;
            BitfieldDeclaratorAST* ast = new (_pool) BitfieldDeclaratorAST;
            ast->colon_token = colon_token;
            node->postfix_declarator_list = new (_pool) PostfixDeclaratorListAST(ast);
            node->initializer = expression;
            return true;
        }
        rewind(colon_token);
    } else if (isFunctionDeclarator && declaringClass && node->core_declarator && LA() == T_EQUAL && LA(3) == T_SEMICOLON) { // = 0, = delete, = default
        if (!_language.cpp11 || LA(2) == T_NUMERIC_LITERAL) {
            parseInitializer(node->initializer, &node->equal_token);
        } else {
            node->equal_token = consumeToken();

            IdExpressionAST* id_expr = new (_pool) IdExpressionAST;
            node->initializer = id_expr;

            SimpleNameAST* simple_name = new (_pool) SimpleNameAST;
            id_expr->name = simple_name;
            simple_name->identifier_token = consumeToken();
        }
    } else if (node->core_declarator && (LA() == T_EQUAL || (_language.cpp11 && !isFunctionDeclarator && LA() == T_LBRACE) || (! declaringClass && LA() == T_LPAREN))) {
        parseInitializer(node->initializer, &node->equal_token);
    }
    return true;
}

bool Parser::parseBaseClause(BaseSpecifierListAST* &node)
{
    DEBUG_THIS_RULE();

    if (LA() == T_COLON) {
        consumeToken(); // ### remove me

        BaseSpecifierListAST* *ast = &node;
        if (parseBaseSpecifier(*ast)) {
            ast = &(*ast)->next;

            while (LA() == T_COMMA) {
                consumeToken(); // consume T_COMMA

                if (parseBaseSpecifier(*ast))
                    ast = &(*ast)->next;
            }
        }

        return true;
    }
    return false;
}

bool Parser::parseInitializer(ExpressionAST* &node, unsigned *equals_token)
{
    DEBUG_THIS_RULE();

    return parseInitializer0x(node, equals_token);
}

bool Parser::parseInitializer0x(ExpressionAST* &node, unsigned *equals_token)
{
    DEBUG_THIS_RULE();

    if ((_language.cpp11 && LA() == T_LBRACE) || LA() == T_EQUAL) {
        if (LA() == T_EQUAL)
            *equals_token = cursor();

        return parseBraceOrEqualInitializer0x(node);
    }

    else if (LA() == T_LPAREN) {
        return parseExpressionListParen(node);
    }

    return false;
}

bool Parser::parseBraceOrEqualInitializer0x(ExpressionAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() == T_EQUAL) {
        consumeToken();
        parseInitializerClause0x(node);
        return true;

    } else if (LA() == T_LBRACE) {
        return parseBracedInitList0x(node);

    }

    return false;
}

/*
    initializer-clause:
        assignment-expression
        braced-init-list
        designated-initializer

    If the next token is a T_LBRACKET, it could be the begin of either

        * a C++11 lambda-introducer (parsed by parseAssignmentExpression)
        * or a C99 designator (parsed by parseDesignatedInitializer).

    Because currently C99 and C++11 Support is activated at the same time,
    first try to parse the assignment-expression. If this fails, try to
    parse a designated-initializer.

    TODO:
        As soon as there will be only "one active language", parse either
        the assignment-expression or the designated-initializer, not both.
 */
bool Parser::parseInitializerClause0x(ExpressionAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() == T_LBRACE)
        return parseBracedInitList0x(node);

    if (parseAssignmentExpression(node))
        return true;
    return parseDesignatedInitializer(node);
}

bool Parser::parseInitializerList0x(ExpressionListAST* &node)
{
    DEBUG_THIS_RULE();
    ExpressionListAST* *expression_list_ptr = &node;
    ExpressionAST* expression = 0;

    _initializerClauseDepth.push(1);

    if (parseInitializerClause0x(expression)) {
        *expression_list_ptr = new (_pool) ExpressionListAST;
        (*expression_list_ptr)->value = expression;
        expression_list_ptr = &(*expression_list_ptr)->next;

        if (_language.cpp11 && LA() == T_DOT_DOT_DOT && (LA(2) == T_COMMA || LA(2) == T_RBRACE || LA(2) == T_RPAREN))
            consumeToken(); // ### create an argument pack

        for (++_initializerClauseDepth.top();
                LA() == T_COMMA
                    && LA(2) != T_RBRACE
                    && _initializerClauseDepth.top() <= MAX_EXPRESSION_DEPTH;
             ++_initializerClauseDepth.top()) {
            consumeToken(); // consume T_COMMA

            if (parseInitializerClause0x(expression)) {
                *expression_list_ptr = new (_pool) ExpressionListAST;
                (*expression_list_ptr)->value = expression;

                if (_language.cpp11 && LA() == T_DOT_DOT_DOT && (LA(2) == T_COMMA || LA(2) == T_RBRACE || LA(2) == T_RPAREN))
                    consumeToken(); // ### create an argument pack

                expression_list_ptr = &(*expression_list_ptr)->next;
            }
        }
    }

    const bool result = _initializerClauseDepth.top() <= MAX_EXPRESSION_DEPTH;
    _initializerClauseDepth.pop();
    if (!result)
        warning(cursor(), "Reached parse limit for initializer clause");
    return result;
}

bool Parser::parseBracedInitList0x(ExpressionAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() != T_LBRACE)
        return false;

    BracedInitializerAST* ast = new (_pool) BracedInitializerAST;
    ast->lbrace_token = consumeToken();

    parseInitializerList0x(ast->expression_list);

    if (LA() == T_COMMA && LA(2) == T_RBRACE)
        ast->comma_token = consumeToken();

    match(T_RBRACE, &ast->rbrace_token);
    node = ast;
    return true;
}

bool Parser::parseMemInitializerList(MemInitializerListAST* &node)
{
    DEBUG_THIS_RULE();
    MemInitializerListAST* *initializer = &node;

    if (parseMemInitializer(*initializer)) {
        initializer = &(*initializer)->next;

        while (true) {

            if (LA() == T_LBRACE)
                break;

            else if (_language.cpp11 && LA() == T_DOT_DOT_DOT && LA(2) == T_LBRACE)
                break;

            else if (LA() == T_COMMA
                     || (LA() == T_IDENTIFIER
                         && (LA(2) == T_LPAREN || LA(2) == T_COLON_COLON || (_language.cpp11 && LA(2) == T_LBRACE)))) {
                if (LA() != T_COMMA)
                    error(cursor(), "expected `,'");
                else
                    consumeToken();

                if (parseMemInitializer(*initializer))
                    initializer = &(*initializer)->next;
                else
                    error(cursor(), "expected a member initializer");

            } else break;
        }

        if (_language.cpp11 && LA() == T_DOT_DOT_DOT) {
            if (LA(2) != T_LBRACE)
                error(cursor(), "expected `{'");

        } else if (LA() != T_LBRACE) {
            error(cursor(), "expected `{'");
        }

        return true;
    }

    return false;
}

bool Parser::parseMemInitializer(MemInitializerListAST* &node)
{
    DEBUG_THIS_RULE();
    NameAST* name = 0;
    if (! parseName(name))
        return false;

    MemInitializerAST* ast = new (_pool) MemInitializerAST;
    ast->name = name;

    if (LA() == T_LPAREN) {
        parseExpressionListParen(ast->expression);
    } else if (_language.cpp11 && LA() == T_LBRACE) {
        parseBracedInitList0x(ast->expression);
    } else {
        if (!_language.cpp11)
            error(cursor(), "expected '('");
        else
            error(cursor(), "expected '(' or '{'");
        return false;
    }

    node = new (_pool) MemInitializerListAST;
    node->value = ast;
    return true;
}

bool Parser::parseTypeIdList(ExpressionListAST* &node)
{
    DEBUG_THIS_RULE();
    ExpressionListAST* *expression_list_ptr = &node;
    ExpressionAST* typeId = 0;
    if (parseTypeId(typeId)) {
        *expression_list_ptr = new (_pool) ExpressionListAST;
        (*expression_list_ptr)->value = typeId;
        expression_list_ptr = &(*expression_list_ptr)->next;

        if (_language.cpp11 && LA() == T_DOT_DOT_DOT)
            consumeToken(); // ### store this token

        while (LA() == T_COMMA) {
            consumeToken();

            if (parseTypeId(typeId)) {
                *expression_list_ptr = new (_pool) ExpressionListAST;
                (*expression_list_ptr)->value = typeId;
                expression_list_ptr = &(*expression_list_ptr)->next;

                if (_language.cpp11 && LA() == T_DOT_DOT_DOT)
                    consumeToken(); // ### store this token
            }
        }
        return true;
    }

    return false;
}

bool Parser::parseExpressionList(ExpressionListAST* &node)
{
    DEBUG_THIS_RULE();
    CHECK_CACHE(ASTCache::ExpressionList, ExpressionListAST);
    unsigned initialCursor = cursor();

    if (_language.cpp11) {
        const bool result = parseInitializerList0x(node);
        _astCache->insert(ASTCache::ExpressionList, initialCursor, (AST* ) node, cursor(), result);
        return result;
    }

    ExpressionListAST* *expression_list_ptr = &node;
    ExpressionAST* expression = 0;
    if (parseAssignmentExpression(expression)) {
        *expression_list_ptr = new (_pool) ExpressionListAST;
        (*expression_list_ptr)->value = expression;
        auto comma_token = &(*expression_list_ptr)->delim_token;
        expression_list_ptr = &(*expression_list_ptr)->next;
        while (LA() == T_COMMA) {
            *comma_token = consumeToken();

            if (parseAssignmentExpression(expression)) {
                *expression_list_ptr = new (_pool) ExpressionListAST;
                (*expression_list_ptr)->value = expression;
                comma_token = &(*expression_list_ptr)->delim_token;
                expression_list_ptr = &(*expression_list_ptr)->next;
            }
        }
        const bool result = true;
        _astCache->insert(ASTCache::ExpressionList, initialCursor, (AST* ) node, cursor(), result);
        return result;
    }

    const bool result = false;
    _astCache->insert(ASTCache::ExpressionList, initialCursor, 0, cursor(), result);
    return result;
}

/*
 * GNU attributes are normally parsed as an expression list. However, the clang's availability
 * attribute doesn't fit into this rule: Clauses such as `introduced', `obsolete', etc. come
 * with a `version' specifier. This version information can be a tuple of three separated
 * integers, which doesn't make for an expression (had it been two integers only, handling it
 * as a double would work). Example:
 *
 *   __attribute__((availability(macosx,introduced=10.12.1))) void f();
 *
 * Reference: https://clang.llvm.org/docs/AttributeReference.html#availability
 */
bool Parser::parseAvailabilityAttribute(ExpressionListAST* &node)
{
    DEBUG_THIS_RULE();

    unsigned start = cursor();
    ExpressionListAST* *expression_list_ptr = &node;
    ExpressionAST* name = nullptr;
    if (parsePrimaryExpression(name)) {
        *expression_list_ptr = new (_pool) ExpressionListAST;
        (*expression_list_ptr)->value = name;
        auto comma_token = &(*expression_list_ptr)->delim_token;
        expression_list_ptr = &(*expression_list_ptr)->next;

        consumeToken(); // Platform-name separator.

        ExpressionAST* clause = nullptr;
        while (parsePrimaryExpression(clause)) {
            if (LA() == T_EQUAL) {
                BinaryExpressionAST* bin_expr = new (_pool) BinaryExpressionAST;
                bin_expr->left_expression = clause;
                bin_expr->binary_op_token = consumeToken();
                if (!parseStringLiteral(bin_expr->right_expression)) {
                    parseNumericLiteral(bin_expr->right_expression);
                    // Discard the remaining of a possible 3-tuple version number.
                    ExpressionAST* dummy;
                    parseNumericLiteral(dummy);
                }
                clause = bin_expr;
            }

            *expression_list_ptr = new (_pool) ExpressionListAST;
            (*expression_list_ptr)->value = clause;
            comma_token = &(*expression_list_ptr)->delim_token;
            expression_list_ptr = &(*expression_list_ptr)->next;

            if (LA() == T_COMMA)
                *comma_token = consumeToken();
        }
    } else {
        error(start, "expected platform-name");
        rewind(start + 1);
        skipUntilDeclaration();
    }

    return true;
}

bool Parser::parseBaseSpecifier(BaseSpecifierListAST* &node)
{
    DEBUG_THIS_RULE();
    BaseSpecifierAST* ast = new (_pool) BaseSpecifierAST;

    if (LA() == T_VIRTUAL) {
        ast->virtual_token = consumeToken();

        int tk = LA();
        if (tk == T_PUBLIC || tk == T_PROTECTED || tk == T_PRIVATE)
            ast->access_specifier_token = consumeToken();
    } else {
        int tk = LA();
        if (tk == T_PUBLIC || tk == T_PROTECTED || tk == T_PRIVATE)
            ast->access_specifier_token = consumeToken();

        if (LA() == T_VIRTUAL)
            ast->virtual_token = consumeToken();
    }

    parseName(ast->name);
    if (! ast->name)
        error(cursor(), "expected class-name");

    // a name can have ellipsis in case of C++11
    // note: the id must be unqualified then - TODO
    if (_language.cpp11 && LA() == T_DOT_DOT_DOT)
        ast->ellipsis_token = consumeToken();

    node = new (_pool) BaseSpecifierListAST;
    node->value = ast;
    return true;
}

bool Parser::parseInitializerList(ExpressionListAST* &node)
{
    DEBUG_THIS_RULE();
    ExpressionListAST* *initializer_ptr = &node;
    ExpressionAST* initializer = 0;
    if (parseInitializerClause(initializer)) {
        *initializer_ptr = new (_pool) ExpressionListAST;
        (*initializer_ptr)->value = initializer;
        initializer_ptr = &(*initializer_ptr)->next;
        while (LA() == T_COMMA) {
            consumeToken(); // consume T_COMMA
            initializer = 0;
            parseInitializerClause(initializer);
            *initializer_ptr = new (_pool) ExpressionListAST;
            (*initializer_ptr)->value = initializer;
            initializer_ptr = &(*initializer_ptr)->next;
        }
    }

    if (_language.cpp11 && LA() == T_DOT_DOT_DOT)
        consumeToken(); // ### store this token

    return true;
}

bool Parser::parseInitializerClause(ExpressionAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() == T_LBRACE) {
        ArrayInitializerAST* ast = new (_pool) ArrayInitializerAST;
        ast->lbrace_token = consumeToken();
        parseInitializerList(ast->expression_list);
        match(T_RBRACE, &ast->rbrace_token);
        node = ast;
        return true;
    }
    return parseAssignmentExpression(node);
}

bool Parser::parseUnqualifiedName(NameAST* &node, bool acceptTemplateId)
{
    DEBUG_THIS_RULE();
    if (LA() == T_TILDE && LA(2) == T_IDENTIFIER) {
        DestructorNameAST* ast = new (_pool) DestructorNameAST;
        ast->tilde_token = consumeToken();
        parseUnqualifiedName(ast->unqualified_name);
        node = ast;
        return true;
    } else if (LA() == T_OPERATOR) {
        unsigned operator_token = cursor();
        if (parseOperatorFunctionId(node))
            return true;
        rewind(operator_token);
        return parseConversionFunctionId(node);
     } else if (LA() == T_IDENTIFIER) {
        unsigned identifier_token = cursor();
        if (acceptTemplateId && LA(2) == T_LESS) {
            bool blocked = blockErrors(true);
            if (parseTemplateId(node)
                    && (! _templateArguments || (LA() == T_COMMA  || maybeSplitGreaterGreaterToken() || LA() == T_GREATER ||
                                                 LA() == T_LPAREN || LA() == T_RPAREN  ||
                                                 LA() == T_STAR || LA() == T_AMPER || // ptr-operators
                                                 LA() == T_COLON_COLON))) {
                blockErrors(blocked);
                return true;
            }
            blockErrors(blocked);
        }
        rewind(identifier_token);
        SimpleNameAST* ast = new (_pool) SimpleNameAST;
        ast->identifier_token = consumeToken();
        node = ast;
        return true;
    } else if (LA() == T_TEMPLATE) {
        unsigned template_token = consumeToken();
        if (parseTemplateId(node, template_token))
            return true;
        rewind(template_token);
    }
    return false;
}

bool Parser::parseStringLiteral(ExpressionAST* &node)
{
    DEBUG_THIS_RULE();

    if (! (LA() == T_STRING_LITERAL
           || LA() == T_WIDE_STRING_LITERAL
           || LA() == T_UTF8_STRING_LITERAL
           || LA() == T_UTF16_STRING_LITERAL
           || LA() == T_UTF32_STRING_LITERAL
           || LA() == T_RAW_STRING_LITERAL
           || LA() == T_RAW_WIDE_STRING_LITERAL
           || LA() == T_RAW_UTF8_STRING_LITERAL
           || LA() == T_RAW_UTF16_STRING_LITERAL
           || LA() == T_RAW_UTF32_STRING_LITERAL)) {
        return false;
    }

    StringLiteralAST* *ast = reinterpret_cast<StringLiteralAST* *> (&node);

    while (LA() == T_STRING_LITERAL
           || LA() == T_WIDE_STRING_LITERAL
           || LA() == T_UTF8_STRING_LITERAL
           || LA() == T_UTF16_STRING_LITERAL
           || LA() == T_UTF32_STRING_LITERAL
           || LA() == T_RAW_STRING_LITERAL
           || LA() == T_RAW_WIDE_STRING_LITERAL
           || LA() == T_RAW_UTF8_STRING_LITERAL
           || LA() == T_RAW_UTF16_STRING_LITERAL
           || LA() == T_RAW_UTF32_STRING_LITERAL) {
        *ast = new (_pool) StringLiteralAST;
        (*ast)->literal_token = consumeToken();
        ast = &(*ast)->next;
    }
    return true;
}

bool Parser::parseExpressionStatement(StatementAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() == T_SEMICOLON) {
        ExpressionStatementAST* ast = new (_pool) ExpressionStatementAST;
        match(T_SEMICOLON, &ast->semicolon_token);
        node = ast;
        return true;
    }

    const bool wasInExpressionStatement = _inExpressionStatement;
    _inExpressionStatement = true;

    // switch to the temp pool and cache
    MemoryPool *previousPool = _pool;
    _pool = &_expressionStatementTempPool;
    ASTCache *previousASTCache = _astCache;
    _astCache = _expressionStatementAstCache;

    bool parsed = false;

    ExpressionAST* expression = 0;
    if (parseExpression(expression)) {
        ExpressionStatementAST* ast = new (previousPool) ExpressionStatementAST;
        if (expression)
            ast->expression = expression->clone(previousPool);
        match(T_SEMICOLON, &ast->semicolon_token);
        node = ast;
        parsed = true;
    }

    _inExpressionStatement = wasInExpressionStatement;

    if (! _inExpressionStatement) {
        // rewind the memory pool and cache after parsing a toplevel expression statement.
        _expressionStatementTempPool.reset();
        _astCache->clear();
    }

    // restore the pool and cache
    _pool = previousPool;
    _astCache = previousASTCache;

    return parsed;
}

bool Parser::parseStatement(StatementAST* &node, bool blockLabeledStatement)
{
    DEBUG_THIS_RULE();
    switch (LA()) {
    case T_WHILE:
        return parseWhileStatement(node);

    case T_DO:
        return parseDoStatement(node);

    case T_FOR:
        return parseForStatement(node);

    case T_IF:
        return parseIfStatement(node);

    case T_SWITCH:
        return parseSwitchStatement(node);

    case T_TRY:
        return parseTryBlockStatement(node, 0);

    case T_CASE:
    case T_DEFAULT:
        if (blockLabeledStatement)
            return false;
        return parseLabeledStatement(node);

    case T_BREAK:
        return parseBreakStatement(node);

    case T_CONTINUE:
        return parseContinueStatement(node);

    case T_GOTO:
        return parseGotoStatement(node);

    case T_RETURN:
        return parseReturnStatement(node);

    case T_LBRACE:
        return parseCompoundStatement(node);

    case T_ASM:
    case T_NAMESPACE:
    case T_USING:
    case T_PSYCHEC_FORALL:
    case T_PSYCHEC_EXISTS:
    case T_TEMPLATE:
    case T_CLASS:
    case T_STRUCT:
    case T_UNION:
        return parseDeclarationStatement(node);

    case T_SEMICOLON: {
        ExpressionStatementAST* ast = new (_pool) ExpressionStatementAST;
        ast->semicolon_token = consumeToken();
        node = ast;
        return true;
    }

    default:
        if (LA() == T_IDENTIFIER && LA(2) == T_COLON) {
            if (blockLabeledStatement)
                return false;
            return parseLabeledStatement(node);
        }

        return parseExpressionOrDeclarationStatement(node);
    } // switch
    return false; //Avoid compiler warning
}

bool Parser::parseBreakStatement(StatementAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() == T_BREAK) {
        BreakStatementAST* ast = new (_pool) BreakStatementAST;
        ast->break_token = consumeToken();
        match(T_SEMICOLON, &ast->semicolon_token);
        node = ast;
        return true;
    }
    return false;
}

bool Parser::parseContinueStatement(StatementAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() == T_CONTINUE) {
        ContinueStatementAST* ast = new (_pool) ContinueStatementAST;
        ast->continue_token = consumeToken();
        match(T_SEMICOLON, &ast->semicolon_token);
        node = ast;
        return true;
    }
    return false;
}

bool Parser::parseGotoStatement(StatementAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() == T_GOTO) {
        GotoStatementAST* ast = new (_pool) GotoStatementAST;
        ast->goto_token = consumeToken();
        match(T_IDENTIFIER, &ast->identifier_token);
        match(T_SEMICOLON, &ast->semicolon_token);
        node = ast;
        return true;
    }
    return false;
}

bool Parser::parseReturnStatement(StatementAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() == T_RETURN) {
        ReturnStatementAST* ast = new (_pool) ReturnStatementAST;
        ast->return_token = consumeToken();
        if (_language.cpp11 && LA() == T_LBRACE)
            parseBracedInitList0x(ast->expression);
        else
            parseExpression(ast->expression);
        match(T_SEMICOLON, &ast->semicolon_token);
        node = ast;
        return true;
    }
    return false;
}

bool Parser::maybeAmbiguousStatement(DeclarationStatementAST* ast, StatementAST* &node)
{
  const unsigned start = ast->firstToken();
  const unsigned end = ast->lastToken();
  const bool blocked = blockErrors(true);

  bool maybeAmbiguous = false;

  StatementAST* stmt = 0;
  if (parseExpressionStatement(stmt)) {
    if (stmt->firstToken() == start && stmt->lastToken() == end) {
      maybeAmbiguous = true;
      node = stmt;
    }
  }

  rewind(end);
  (void) blockErrors(blocked);
  return maybeAmbiguous;
}

bool Parser::parseExpressionOrDeclarationStatement(StatementAST* &node)
{
    DEBUG_THIS_RULE();

    if (LA() == T_SEMICOLON)
        return parseExpressionStatement(node);

    const unsigned start = cursor();

    if (lookAtQualifier()
            || lookAtStorageClassSpecifier()
            || lookAtBuiltinTypeSpecifier()
            || LA() == T_TYPENAME
            || LA() == T_ENUM
            || lookAtClassKey()
            || (LA() == T_STATIC_ASSERT && _language.cpp11)) {
        return parseDeclarationStatement(node);
    }

    if (LA() == T_IDENTIFIER || (LA() == T_COLON_COLON && LA(2) == T_IDENTIFIER)) {
        const bool blocked = blockErrors(true);

        ExpressionAST* expression = 0;
        const bool hasExpression = parseExpression(expression);
        const unsigned afterExpression = cursor();

        if (hasExpression/* && LA() == T_SEMICOLON*/) {
            //const unsigned semicolon_token = consumeToken();
            unsigned semicolon_token = 0;
            if (LA() == T_SEMICOLON)
                semicolon_token = cursor();

            ExpressionStatementAST* as_expression = new (_pool) ExpressionStatementAST;
            as_expression->expression = expression;
            as_expression->semicolon_token = semicolon_token;
            node = as_expression; // well, at least for now.

            auto maybeDecl = [as_expression, start, &node, this] (bool resolveIt) -> bool {
                unsigned mark = cursor();
                rewind(start);

                // Despite being not conventional, the syntax a(b) may also be
                // a declaration, with extra parenthesis. Thus, we create an AST
                // ambiguity node. Notice that a(b,c) is unambiguously a call.
                DeclarationAST* declaration = 0;
                if (parseSimpleDeclaration(declaration)) {
                    unsigned line;
                    _translationUnit->getTokenPosition(start, &line);
                    //printDebug("Ambiguity call x decl found at %d\n", line);

                    AmbiguousStatementAST* ambig = new (_pool) AmbiguousStatementAST;
                    ambig->declarationStmt = new (_pool) DeclarationStatementAST;
                    ambig->declarationStmt->declaration = declaration;
                    ambig->expressionStmt = as_expression;
                    ambig->ambiguity = std::make_unique<SyntaxAmbiguity>(
                                SyntaxAmbiguity::Variety::OneArgCall_X_VarDecl,
                                line,
                                resolveIt ? SyntaxAmbiguity::Resolution::IsDecl
                                          : SyntaxAmbiguity::Resolution::Unknown);
                    node = ambig;
                    return true;
                }
                rewind(mark);
                return false;
            };

            bool invalidAssignment = false;
            if (BinaryExpressionAST* binary = expression->asBinaryExpression()) {
                const int binop = _translationUnit->tokenKind(binary->binary_op_token);
                if (binop == T_EQUAL) {
                    ExpressionAST* left = binary->left_expression;
                    if (left->asCall()
                            && left->asCall()->base_expression->asIdExpression()
                            && left->asCall()->expression_list->value
                            && left->asCall()->expression_list->value->asIdExpression()
                            && !left->asCall()->expression_list->next
                            && maybeDecl(true)) {
                        (void) blockErrors(blocked);
                        return true;
                    }

                    if (!left->asBinaryExpression()) {
                        (void) blockErrors(blocked);
                        node = as_expression;
                        match(T_SEMICOLON, &as_expression->semicolon_token);
                        return true;
                    }

                    invalidAssignment = true;
                }
            } else if (CallAST* call = expression->asCall()) {
                if (call->base_expression->asIdExpression() != 0) {
                    // If an initializer is present, then this is definitely
                    // a declaration, since a function call is not assignable
                    // in C (what's not true in C++). We can already mark the
                    // ambiguity resolution.
                    const bool isCPlusPlus = false;
                    if (call->expression_list
                            && call->expression_list->value
                            && call->expression_list->value->asIdExpression()
                            && !call->expression_list->next
                            && maybeDecl(!isCPlusPlus && (LA() == T_EQUAL))) {
                        (void) blockErrors(blocked);
                        return true;
                    }

                    (void) blockErrors(blocked);
                    node = as_expression;
                    match(T_SEMICOLON, &as_expression->semicolon_token);
                    return true;
                }
            }

            rewind(start);

            DeclarationAST* declaration = 0;
            if (parseSimpleDeclaration(declaration)) {
                DeclarationStatementAST* as_declaration = new (_pool) DeclarationStatementAST;
                as_declaration->declaration = declaration;

                SimpleDeclarationAST* simple = declaration->asSimpleDeclaration();
                if (! semicolon_token || invalidAssignment || semicolon_token != simple->semicolon_token ||
                        (simple->decl_specifier_list != 0 && simple->declarator_list != 0)) {

                    // If there's a ptr operator within the first declarator of
                    // a simple declaration, and no initializer or other declarator has been provided,
                    // this could be an expression `x * y`. When this is the case
                    // we create an AST ambiguity node that must be resolved later.
                    if (simple->declarator_list
                            && simple->declarator_list->value->ptr_operator_list
                            && !simple->declarator_list->value->initializer
                            && !simple->declarator_list->value->postfix_declarator_list) {
                        bool maybeExpr = true;
                        // Check other declarators, if any.
                        for (DeclaratorListAST* it = simple->declarator_list->next; it; it = it->next) {
                            if (it->value->ptr_operator_list) {
                                maybeExpr = false;
                                break;
                            }
                        }

                        if (maybeExpr) {
                            unsigned afterDeclaration = cursor();
                            rewind(start);
                            StatementAST* as_expression;
                            if (parseExpressionStatement(as_expression)) {
                                unsigned line;
                                _translationUnit->getTokenPosition(start, &line);
                                //printDebug("Ambiguity pointer decl x multiplication at %d\n", line);

                                AmbiguousStatementAST* ambig = new (_pool) AmbiguousStatementAST;
                                ambig->declarationStmt = as_declaration;
                                ambig->expressionStmt = as_expression->asExpressionStatement();
                                ambig->ambiguity = std::make_unique<SyntaxAmbiguity>(
                                            SyntaxAmbiguity::Variety::MulExpr_X_PointerDecl,
                                            line);
                                node = ambig;
                                (void) blockErrors(blocked);
                                return true;
                            }
                            rewind(afterDeclaration);
                        }
                    }

                    (void) blockErrors(blocked);
                    node = as_declaration;
                    return true;
                }

                ExpressionOrDeclarationStatementAST* ast = new (_pool) ExpressionOrDeclarationStatementAST;
                ast->declaration = as_declaration;
                ast->expression = as_expression;
                node = ast;
                (void) blockErrors(blocked);
                return true;
            }

            (void) blockErrors(blocked);

            rewind(afterExpression);
            match(T_SEMICOLON, &as_expression->semicolon_token);
            return true;
        }

        rewind(start);
        (void) blockErrors(blocked);

        return parseDeclarationStatement(node);
    }

    rewind(start);
    return parseExpressionStatement(node);
}

bool Parser::parseCondition(ExpressionAST* &node)
{
    DEBUG_THIS_RULE();
    unsigned start = cursor();

    bool blocked = blockErrors(true);
    SpecifierListAST* type_specifier = 0;
    if (parseTypeSpecifier(type_specifier)) {
        DeclaratorAST* declarator = 0;
        if (parseInitDeclarator(declarator, type_specifier, /*declaringClass=*/ 0)) {
            if (declarator->initializer && declarator->equal_token) {
                ConditionAST* ast = new (_pool) ConditionAST;
                ast->type_specifier_list = type_specifier;
                ast->declarator = declarator;
                node = ast;
                blockErrors(blocked);
                return true;
            }
        }
    }

    blockErrors(blocked);
    rewind(start);
    return parseExpression(node);
}

bool Parser::parseWhileStatement(StatementAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() == T_WHILE) {
        WhileStatementAST* ast = new (_pool) WhileStatementAST;
        ast->while_token = consumeToken();
        match(T_LPAREN, &ast->lparen_token);
        parseCondition(ast->condition);
        match(T_RPAREN, &ast->rparen_token);
        parseStatement(ast->statement);
        node = ast;
        return true;
    }
    return true;
}

bool Parser::parseDoStatement(StatementAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() == T_DO) {
        DoStatementAST* ast = new (_pool) DoStatementAST;
        ast->do_token = consumeToken();
        parseStatement(ast->statement);
        match(T_WHILE, &ast->while_token);
        match(T_LPAREN, &ast->lparen_token);
        parseExpression(ast->expression);
        match(T_RPAREN, &ast->rparen_token);
        match(T_SEMICOLON, &ast->semicolon_token);
        node = ast;
        return true;
    }
    return false;
}

bool Parser::parseForStatement(StatementAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() != T_FOR)
        return false;

    unsigned for_token = consumeToken();
    unsigned lparen_token = 0;
    match(T_LPAREN, &lparen_token);

    unsigned startOfTypeSpecifier = cursor();
    bool blocked = blockErrors(true);

    if (_language.cpp11) {
        RangeBasedForStatementAST* ast = new (_pool) RangeBasedForStatementAST;
        ast->for_token = for_token;
        ast->lparen_token = lparen_token;

        if (parseTypeSpecifier(ast->type_specifier_list))
            parseDeclarator(ast->declarator, ast->type_specifier_list);

        if ((ast->type_specifier_list || ast->declarator) && LA() == T_COLON) {
            ast->colon_token = consumeToken();
            blockErrors(blocked);

            if (LA() == T_LBRACE)
                parseBracedInitList0x(ast->expression);
            else
                parseExpression(ast->expression);
            match(T_RPAREN, &ast->rparen_token);
            parseStatement(ast->statement);

            // We need both type specifier and declarator for C++11 range-based for
            if (!ast->type_specifier_list || !ast->declarator)
               error(for_token, "expected declaration with type specifier");

            node = ast;
            return true;
        }
        rewind(startOfTypeSpecifier);
    }

    blockErrors(blocked);

    // Normal C/C++ for-statement parsing
    ForStatementAST* ast = new (_pool) ForStatementAST;

    ast->for_token = for_token;
    ast->lparen_token = lparen_token;
    parseForInitStatement(ast->initializer);
    parseCondition(ast->condition);
    match(T_SEMICOLON, &ast->semicolon_token);
    parseExpression(ast->expression);
    match(T_RPAREN, &ast->rparen_token);
    parseStatement(ast->statement);

    node = ast;
    return true;
}

bool Parser::parseForInitStatement(StatementAST* &node)
{
    DEBUG_THIS_RULE();

    return parseExpressionOrDeclarationStatement(node);
}

bool Parser::parseCompoundStatement(StatementAST* &node)
{
    DEBUG_THIS_RULE();

    if (LA() == T_LBRACE) {
        if (_statementDepth > MAX_STATEMENT_DEPTH)
            return false;
        ++_statementDepth;

        CompoundStatementAST* ast = new (_pool) CompoundStatementAST;
        ast->lbrace_token = consumeToken();

        StatementListAST* *statement_ptr = &ast->statement_list;
        while (int tk = LA()) {
            if (tk == T_RBRACE)
                break;

            unsigned start_statement = cursor();
            StatementAST* statement = 0;
            if (! parseStatement(statement)) {
                rewind(start_statement + 1);
                skipUntilStatement();
            } else {
                *statement_ptr = new (_pool) StatementListAST;
                (*statement_ptr)->value = statement;
                statement_ptr = &(*statement_ptr)->next;
            }
        }
        match(T_RBRACE, &ast->rbrace_token);
        node = ast;
        --_statementDepth;
        return true;
    }
    return false;
}

bool Parser::parseIfStatement(StatementAST* &node)
{
    DEBUG_THIS_RULE();

    if (LA() == T_IF) {
        IfStatementAST* ast = new (_pool) IfStatementAST;
        ast->if_token = consumeToken();
        match(T_LPAREN, &ast->lparen_token);
        parseCondition(ast->condition);
        match(T_RPAREN, &ast->rparen_token);
        if (! parseStatement(ast->statement))
            error(cursor(), "expected statement");
        if (LA() == T_ELSE) {
            ast->else_token = consumeToken();
            if (! parseStatement(ast->else_statement))
                error(cursor(), "expected statement");
        }
        node = ast;
        return true;
    }
    return false;
}

bool Parser::parseSwitchStatement(StatementAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() == T_SWITCH) {
        SwitchStatementAST* ast = new (_pool) SwitchStatementAST;
        ast->switch_token = consumeToken();
        match(T_LPAREN, &ast->lparen_token);
        parseCondition(ast->condition);
        match(T_RPAREN, &ast->rparen_token);
        parseStatement(ast->statement);
        node = ast;
        return true;
    }
    return false;
}

bool Parser::parseLabeledStatement(StatementAST* &node)
{
    DEBUG_THIS_RULE();
    switch (LA()) {
    case T_IDENTIFIER:
        if (LA(2) == T_COLON) {
            LabeledStatementAST* ast = new (_pool) LabeledStatementAST;
            ast->label_token = consumeToken();
            ast->colon_token = consumeToken();
            parseStatement(ast->statement, /*blockLabeledStatement =*/ true);
            node = ast;
            return true;
        }
        break;

    case T_DEFAULT: {
        LabeledStatementAST* ast = new (_pool) LabeledStatementAST;
        ast->label_token = consumeToken();
        match(T_COLON, &ast->colon_token);
        parseStatement(ast->statement, /*blockLabeledStatement =*/ true);
        node = ast;
        return true;
    }

    case T_CASE: {
        CaseStatementAST* ast = new (_pool) CaseStatementAST;
        ast->case_token = consumeToken();
        parseConstantExpression(ast->expression);
        match(T_COLON, &ast->colon_token);
        parseStatement(ast->statement, /*blockLabeledStatement =*/ true);
        node = ast;
        return true;
    }

    default:
        break;
    } // switch
    return false;
}

bool Parser::parseBlockDeclaration(DeclarationAST* &node)
{
    DEBUG_THIS_RULE();
    switch (LA()) {
    case T_USING:
        return parseUsing(node);

    case T_ASM:
        return parseAsmDefinition(node);

    case T_NAMESPACE:
        return parseNamespaceAliasDefinition(node);

    case T_STATIC_ASSERT:
        if (_language.cpp11)
            return parseStaticAssertDeclaration(node);
        // fall-through

    default:
        return parseSimpleDeclaration(node);
    } // switch

}

bool Parser::parseNamespaceAliasDefinition(DeclarationAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() == T_NAMESPACE && LA(2) == T_IDENTIFIER && LA(3) == T_EQUAL) {
        NamespaceAliasDefinitionAST* ast = new (_pool) NamespaceAliasDefinitionAST;
        ast->namespace_token = consumeToken();
        ast->namespace_name_token = consumeToken();
        ast->equal_token = consumeToken();
        parseName(ast->name);
        match(T_SEMICOLON, &ast->semicolon_token);
        node = ast;
        return true;
    }
    return false;
}

bool Parser::parseDeclarationStatement(StatementAST* &node)
{
    DEBUG_THIS_RULE();
    unsigned start = cursor();
    DeclarationAST* declaration = 0;
    if (! parseBlockDeclaration(declaration))
        return false;

    if (SimpleDeclarationAST* simpleDeclaration = declaration->asSimpleDeclaration()) {
        if (! simpleDeclaration->decl_specifier_list) {
            rewind(start);
            return false;
        }
    }

    DeclarationStatementAST* ast = new (_pool) DeclarationStatementAST;
    ast->declaration = declaration;
    node = ast;
    return true;
}

bool Parser::lookAtQualifier() const
{
    switch (LA()) {
    case T_CONST:
    case T_VOLATILE:
    case T_RESTRICT:
        return true;
    default:
        return false;
    }
}

bool Parser::lookAtFunctionSpecifier() const
{
    switch (LA()) {
    case T_INLINE:
    case T_VIRTUAL:
    case T_EXPLICIT:
        return true;
    default:
        return false;
    }
}

bool Parser::lookAtStorageClassSpecifier() const
{
    switch (LA()) {
    case T_FRIEND:
    case T_REGISTER:
    case T_STATIC:
    case T_EXTERN:
    case T_MUTABLE:
    case T_TYPEDEF:
    case T___THREAD:
        return true;
    case T_THREAD_LOCAL:
        return _language.cpp11;
    case T_CONSTEXPR:
        if (_language.cpp11)
            return true;
        // fall-through
    default:
        return false;
    }
}

bool Parser::lookAtBuiltinTypeSpecifier() const
{
    switch (LA()) {
    case T_CHAR:
    case T_CHAR16_T:
    case T_CHAR32_T:
    case T_WCHAR_T:
    case T_BOOL:
    case T_SHORT:
    case T_INT:
    case T_LONG:
    case T_SIGNED:
    case T_UNSIGNED:
    case T_FLOAT:
    case T_DOUBLE:
    case T_VOID:
    case T_AUTO:
    case T_DECLTYPE:
        return true;
    // [gcc] extensions
    case T___TYPEOF__:
    case T___ATTRIBUTE__:
        return true;
    default:
        return false;
    }
}

bool Parser::lookAtClassKey() const
{
    switch (LA()) {
    case T_CLASS:
    case T_STRUCT:
    case T_UNION:
        return true;
    default:
        return false;
    }
}

bool Parser::parseOptionalAttributeSpecifierSequence(SpecifierListAST* &attribute_list)
{
    DEBUG_THIS_RULE();
    bool didRead = false;
    while (parseAttributeSpecifier(attribute_list))
        didRead = true;
    return didRead;
}

bool Parser::parseAttributeSpecifier(SpecifierListAST* &attribute_list)
{
    DEBUG_THIS_RULE();
    SpecifierListAST* *attr_ptr = &attribute_list;
    switch (LA()) {
    case T_ALIGNAS: {
        AlignmentSpecifierAST* ast = new (_pool) AlignmentSpecifierAST;
        ast->align_token = consumeToken();
        match(T_LPAREN, &ast->lparen_token);

        const unsigned saved = cursor();
        if (!parseTypeId(ast->typeIdExprOrAlignmentExpr) ||
                (LA() != T_RPAREN &&
                (LA(1) != T_DOT_DOT_DOT || LA(2) != T_RPAREN))) {
            rewind(saved);
            parseExpression(ast->typeIdExprOrAlignmentExpr);
        }

        if (LA() == T_DOT_DOT_DOT)
            ast->ellipses_token = consumeToken();
        match(T_RPAREN, &ast->rparen_token);
        attribute_list = new (_pool) SpecifierListAST(ast);
        return true;
    }
        //### TODO: C++11-style attributes
//    case T_LBRACKET:
    case T___ATTRIBUTE__:
        while (LA() == T___ATTRIBUTE__) {
            parseGnuAttributeSpecifier(*attr_ptr);
            attr_ptr = &(*attr_ptr)->next;
        }
        return true;
    default:
        return false;
    }
}

bool Parser::parseGnuAttributeSpecifier(SpecifierListAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() != T___ATTRIBUTE__)
        return false;

    GnuAttributeSpecifierAST* ast = new (_pool) GnuAttributeSpecifierAST;
    ast->attribute_token = consumeToken();
    match(T_LPAREN, &ast->first_lparen_token);
    match(T_LPAREN, &ast->second_lparen_token);
    parseGnuAttributeList(ast->attribute_list);
    match(T_RPAREN, &ast->first_rparen_token);
    match(T_RPAREN, &ast->second_rparen_token);
    node = new (_pool) SpecifierListAST(ast);
    return true;
}

bool Parser::parseGnuAttributeList(GnuAttributeListAST* &node)
{
    DEBUG_THIS_RULE();

    GnuAttributeListAST* *iter = &node;
    while (LA() == T_CONST || LA() == T_IDENTIFIER) {
        *iter = new (_pool) GnuAttributeListAST;

        if (LA() == T_CONST) {
            GnuAttributeAST* attr = new (_pool) GnuAttributeAST;
            attr->identifier_token = consumeToken();

            (*iter)->value = attr;
            iter = &(*iter)->next;
        } else if (LA() == T_IDENTIFIER) {
            const Identifier* ident = _translationUnit->tokenAt(cursor()).identifier;
            GnuAttributeAST* attr = new (_pool) GnuAttributeAST;
            attr->identifier_token = consumeToken();
            if (LA() == T_LPAREN) {
                attr->lparen_token = consumeToken();
                if (ident->equalTo(_control->attrAvailability())) {
                    parseAvailabilityAttribute(attr->expression_list);
                    if (!_language.ext_AvailabilityAttribute)
                        warning(attr->identifier_token, "availability-attribute is an extension");
                } else {
                    parseExpressionList(attr->expression_list);
                }
                match(T_RPAREN, &attr->rparen_token);
            }

            (*iter)->value = attr;
            iter = &(*iter)->next;
        }

        if (LA() != T_COMMA)
            break;

        consumeToken(); // skip T_COMMA
    }

    return true;
}

bool Parser::parseBuiltinTypeSpecifier(SpecifierListAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() == T___ATTRIBUTE__) {
        return parseGnuAttributeSpecifier(node);
    } else if (LA() == T___TYPEOF__) {
        TypeofSpecifierAST* ast = new (_pool) TypeofSpecifierAST;
        ast->typeof_token = consumeToken();
        if (LA() == T_LPAREN) {
            unsigned lparen_token = consumeToken();
            if (parseTypeId(ast->expression) && LA() == T_RPAREN) {
                ast->lparen_token = lparen_token;
                ast->rparen_token = consumeToken();
                node = new (_pool) SpecifierListAST(ast);
                return true;
            }
            rewind(lparen_token);
        }
        parseUnaryExpression(ast->expression);
        node = new (_pool) SpecifierListAST(ast);
        return true;
    } else if (LA() == T_DECLTYPE) {
        DecltypeSpecifierAST* ast = new (_pool) DecltypeSpecifierAST;
        ast->decltype_token = consumeToken();
        match(T_LPAREN, &ast->lparen_token);
        if (parseExpression(ast->expression))
            match(T_RPAREN, &ast->rparen_token);
        node = new (_pool) SpecifierListAST(ast);
        return true;
    } else if (lookAtBuiltinTypeSpecifier()) {
        SimpleSpecifierAST* ast = new (_pool) SimpleSpecifierAST;
        ast->specifier_token = consumeToken();
        node = new (_pool) SpecifierListAST(ast);
        return true;
    }
    return false;
}

bool Parser::parseSimpleDeclaration(DeclarationAST* &node, ClassSpecifierAST* declaringClass)
{
    DEBUG_THIS_RULE();

    // parse a simple declaration, a function definition,
    // or a contructor declaration.
    bool has_type_specifier = false;
    bool has_complex_type_specifier = false;
    unsigned startOfNamedTypeSpecifier = 0;
    NameAST* named_type_specifier = 0;
    SpecifierListAST* decl_specifier_seq = 0,
         **decl_specifier_seq_ptr = &decl_specifier_seq;
    for (;;) {
        if (lookAtQualifier() || lookAtFunctionSpecifier()
                || lookAtStorageClassSpecifier()) {
            SimpleSpecifierAST* spec = new (_pool) SimpleSpecifierAST;
            spec->specifier_token = consumeToken();
            *decl_specifier_seq_ptr = new (_pool) SpecifierListAST(spec);
            decl_specifier_seq_ptr = &(*decl_specifier_seq_ptr)->next;
        } else if (parseAttributeSpecifier(*decl_specifier_seq_ptr)) {
            decl_specifier_seq_ptr = &(*decl_specifier_seq_ptr)->next;
        } else if (! named_type_specifier && ! has_complex_type_specifier && lookAtBuiltinTypeSpecifier()) {
            parseBuiltinTypeSpecifier(*decl_specifier_seq_ptr);
            decl_specifier_seq_ptr = &(*decl_specifier_seq_ptr)->next;
            has_type_specifier = true;
        } else if (! has_type_specifier
                    && (LA() == T_COLON_COLON
                        || LA() == T_IDENTIFIER)) {
            startOfNamedTypeSpecifier = cursor();
            if (parseName(named_type_specifier)) {

                const Identifier* classIdentifier = className(declaringClass);
                if (QualifiedNameAST* qn = named_type_specifier->asQualifiedName())
                    if (NestedNameSpecifierListAST* namesList = qn->nested_name_specifier_list)
                        if (NestedNameSpecifierAST* lastName = namesList->lastValue())
                            classIdentifier = identifier(lastName->class_or_namespace_name);

                if (LA() == T_LPAREN && identifier(named_type_specifier) == classIdentifier) {
                    // looks like a constructor declaration
                    rewind(startOfNamedTypeSpecifier);
                    break;
                }

                NamedTypeSpecifierAST* spec = new (_pool) NamedTypeSpecifierAST;
                spec->name = named_type_specifier;
                *decl_specifier_seq_ptr = new (_pool) SpecifierListAST(spec);
                decl_specifier_seq_ptr = &(*decl_specifier_seq_ptr)->next;
                has_type_specifier = true;
            } else {
                rewind(startOfNamedTypeSpecifier);
                break;
            }
        } else if (! has_type_specifier && LA() == T_ENUM) {
            unsigned startOfTypeSpecifier = cursor();
            if (! parseElaboratedTypeSpecifier(*decl_specifier_seq_ptr)
                    || LA() == T_LBRACE
                    || (_language.cpp11 && LA() == T_COLON)) {
                rewind(startOfTypeSpecifier);
                if (! parseEnumSpecifier(*decl_specifier_seq_ptr)) {
                    error(startOfTypeSpecifier, "expected an enum specifier");
                    break;
                }
                has_complex_type_specifier = true;
            }
            decl_specifier_seq_ptr = &(*decl_specifier_seq_ptr)->next;
            has_type_specifier = true;
        } else if (! has_type_specifier && LA() == T_TYPENAME) {
            unsigned startOfElaboratedTypeSpecifier = cursor();
            if (! parseElaboratedTypeSpecifier(*decl_specifier_seq_ptr)) {
                error(startOfElaboratedTypeSpecifier, "expected an elaborated type specifier");
                break;
            }
            decl_specifier_seq_ptr = &(*decl_specifier_seq_ptr)->next;
            has_type_specifier = true;
        } else if (! has_type_specifier && lookAtClassKey()) {
            unsigned startOfTypeSpecifier = cursor();
            if (! parseElaboratedTypeSpecifier(*decl_specifier_seq_ptr) ||
                (LA() == T_COLON || LA() == T_LBRACE
                 || (LA(0) == T_IDENTIFIER && LA(1) == T_IDENTIFIER // MACRO Name followed by : or {
                     && (LA(2) == T_COLON || LA(2) == T_LBRACE))
                 || (LA(0) == T_IDENTIFIER && LA(1) == T_IDENTIFIER && LA(2) == T_IDENTIFIER && // MACRO Name final followed by : or {
                     (LA(3) == T_COLON || LA(3) == T_LBRACE)))) {
                rewind(startOfTypeSpecifier);
                if (! parseClassSpecifier(*decl_specifier_seq_ptr)) {
                    error(startOfTypeSpecifier, "wrong type specifier");
                    break;
                }
                has_complex_type_specifier = true;
            }
            decl_specifier_seq_ptr = &(*decl_specifier_seq_ptr)->next;
            has_type_specifier = true;
        } else if (! has_type_specifier
                        && (LA() == T_PSYCHEC_FORALL || LA() == T_PSYCHEC_EXISTS)) {
            unsigned startOfTypeSpecifier = cursor();
            if (! parseQuantifiedTypeSpecifier(*decl_specifier_seq_ptr)) {
                has_complex_type_specifier = true;
                consumeToken();
                error(startOfTypeSpecifier, "expected a quantified type specifier");
                break;
            }
            decl_specifier_seq_ptr = &(*decl_specifier_seq_ptr)->next;
            has_type_specifier = true;
        } else {
            break;
        }
    }

    DeclaratorListAST* declarator_list = 0;
    DeclaratorListAST** declarator_ptr = &declarator_list;
    DeclaratorAST* declarator = 0;

    if (LA() != T_SEMICOLON) {
        const bool maybeCtor = (LA() == T_LPAREN && named_type_specifier);
        bool didParseInitDeclarator = parseInitDeclarator(declarator, decl_specifier_seq, declaringClass);
        if ((! didParseInitDeclarator && maybeCtor) || (didParseInitDeclarator && maybeCtor && LA() == T_COLON)){
            rewind(startOfNamedTypeSpecifier);
            named_type_specifier = 0;
            // pop the named type specifier from the decl-specifier-seq
            SpecifierListAST* *spec_ptr = &decl_specifier_seq;
            for (; *spec_ptr; spec_ptr = &(*spec_ptr)->next) {
                if (! (*spec_ptr)->next) {
                    *spec_ptr = 0;
                    break;
                }
            }
            if (! parseInitDeclarator(declarator, decl_specifier_seq, declaringClass))
                return false;
        }
    }

    // if there is no valid declarator
    // and it doesn't look like a fwd or a class declaration
    // then it's not a declarations
    if (! declarator && ! maybeForwardOrClassDeclaration(decl_specifier_seq))
        return false;

    DeclaratorAST* firstDeclarator = declarator;

    unsigned* comma_token;
    if (declarator) {
        *declarator_ptr = new (_pool) DeclaratorListAST;
        (*declarator_ptr)->value = declarator;
        comma_token = &(*declarator_ptr)->delim_token;
        declarator_ptr = &(*declarator_ptr)->next;
    }

    if (LA() == T_COMMA || LA() == T_SEMICOLON || has_complex_type_specifier) {
        while (LA() == T_COMMA) {
            *comma_token = consumeToken();

            declarator = 0;
            if (parseInitDeclarator(declarator, decl_specifier_seq, declaringClass)) {
                *declarator_ptr = new (_pool) DeclaratorListAST;
                (*declarator_ptr)->value = declarator;
                comma_token = &(*declarator_ptr)->delim_token;
                declarator_ptr = &(*declarator_ptr)->next;
            }
        }
        SimpleDeclarationAST* ast = new (_pool) SimpleDeclarationAST;
        ast->decl_specifier_list = decl_specifier_seq;
        ast->declarator_list = declarator_list;
        match(T_SEMICOLON, &ast->semicolon_token);
        node = ast;
        return true;
    }

    if (!_inFunctionBody
            && declarator
            && (LA() == T_COLON
                || LA() == T_LBRACE
                || LA() == T_TRY)) {
        if (LA() == T_TRY) {
            FunctionDefinitionAST* ast = new (_pool) FunctionDefinitionAST;
            ast->decl_specifier_list = decl_specifier_seq;
            ast->declarator = firstDeclarator;
            parseTryBlockStatement(ast->function_body, &ast->ctor_initializer);
            node = ast;
            return true; // recognized a function definition.
        }

        CtorInitializerAST* ctor_initializer = 0;
        bool hasCtorInitializer = false;
        if (LA() == T_COLON) {
            hasCtorInitializer = true;
            parseCtorInitializer(ctor_initializer);

            if (LA() != T_LBRACE) {
                const unsigned pos = cursor();

                for (int n = 0; n < 3 && LA(); consumeToken(), ++n)
                    if (LA() == T_LBRACE)
                        break;

                if (LA() != T_LBRACE) {
                    error(pos, "unexpected token `%s'", _translationUnit->spell(pos));
                    rewind(pos);
                }
            }
        }

        if (LA() == T_LBRACE || hasCtorInitializer) {
            FunctionDefinitionAST* ast = new (_pool) FunctionDefinitionAST;
            ast->decl_specifier_list = decl_specifier_seq;
            ast->declarator = firstDeclarator;
            ast->ctor_initializer = ctor_initializer;
            parseFunctionBody(ast->function_body);
            node = ast;
            return true; // recognized a function definition.
        }
    }

    error(cursor(), "unexpected token `%s'", token().spell());
    return false;
}

bool Parser::maybeForwardOrClassDeclaration(SpecifierListAST* decl_specifier_seq) const
{
    // look at the decl_specifier for possible fwd or class declarations.
    if (SpecifierListAST* it = decl_specifier_seq) {
        while (it) {
            SimpleSpecifierAST* spec = it->value->asSimpleSpecifier();
            if (spec && _translationUnit->tokenKind(spec->specifier_token) == T_FRIEND)
                it = it->next;
            else
                break;
        }

        if (it) {
            SpecifierAST* spec = it->value;

            if (spec->asElaboratedTypeSpecifier() ||
                    spec->asEnumSpecifier() ||
                    spec->asClassSpecifier()) {
                for (it = it->next; it; it = it->next)
                    if (it->value->asAttributeSpecifier() == 0)
                        return false;
                return true;
            }
        }
    }

    return false;
}

bool Parser::parseFunctionBody(StatementAST* &node)
{
    DEBUG_THIS_RULE();

    if (_translationUnit->skipFunctionBody()) {
        unsigned token_lbrace = 0;
        match(T_LBRACE, &token_lbrace);
        if (! token_lbrace)
            return false;

        const Token &tk = _translationUnit->tokenAt(token_lbrace);
        if (tk.close_brace)
            rewind(tk.close_brace);
        unsigned token_rbrace = 0;
        match(T_RBRACE, &token_rbrace);
        return true;
    }

    _inFunctionBody = true;
    const bool parsed = parseCompoundStatement(node);
    _inFunctionBody = false;
    return parsed;
}

/**
 * Parses both try-block and function-try-block
 * @param placeholder Non-null for function-try-block in around constructor
 *
 *  try-block:
 *      try compound-statement handler-seq
 *  function-try-block:
 *      try [ctor-initializer] compound-statement handler-seq
 */
bool Parser::parseTryBlockStatement(StatementAST* &node, CtorInitializerAST* *placeholder)
{
    DEBUG_THIS_RULE();

    if (LA() == T_TRY) {
        TryBlockStatementAST* ast = new (_pool) TryBlockStatementAST;
        // try
        ast->try_token = consumeToken();
        // [ctor-initializer]
        if (LA() == T_COLON) {
            const unsigned colonPos = cursor();
            CtorInitializerAST* ctor_initializer = 0;
            parseCtorInitializer(ctor_initializer);

            if (LA() != T_LBRACE) {
                const unsigned pos = cursor();

                for (int n = 0; n < 3 && LA(); consumeToken(), ++n)
                    if (LA() == T_LBRACE)
                        break;

                if (LA() != T_LBRACE) {
                    error(pos, "unexpected token `%s'", _translationUnit->spell(pos));
                    rewind(pos);
                }
            }

            if (placeholder)
                *placeholder = ctor_initializer;
            else
                error(colonPos, "constructor initializer not allowed inside function body");
        }
        // compound-statement
        parseCompoundStatement(ast->statement);
        // handler-seq
        CatchClauseListAST* *catch_clause_ptr = &ast->catch_clause_list;
        while (parseCatchClause(*catch_clause_ptr))
            catch_clause_ptr = &(*catch_clause_ptr)->next;
        node = ast;
        return true;
    }
    return false;
}

bool Parser::parseCatchClause(CatchClauseListAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() == T_CATCH) {
        CatchClauseAST* ast = new (_pool) CatchClauseAST;
        ast->catch_token = consumeToken();
        match(T_LPAREN, &ast->lparen_token);
        parseExceptionDeclaration(ast->exception_declaration);
        match(T_RPAREN, &ast->rparen_token);
        parseCompoundStatement(ast->statement);
        node = new (_pool) CatchClauseListAST(ast);
        return true;
    }
    return false;
}

bool Parser::parseExceptionDeclaration(ExceptionDeclarationAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() == T_DOT_DOT_DOT) {
        ExceptionDeclarationAST* ast = new (_pool) ExceptionDeclarationAST;
        ast->dot_dot_dot_token = consumeToken();
        node = ast;
        return true;
    }

    SpecifierListAST* type_specifier = 0;
    if (parseTypeSpecifier(type_specifier)) {
        ExceptionDeclarationAST* ast = new (_pool) ExceptionDeclarationAST;
        ast->type_specifier_list = type_specifier;
        parseDeclaratorOrAbstractDeclarator(ast->declarator, type_specifier);
        node = ast;
        return true;
    }
    return false;
}

bool Parser::parseBoolLiteral(ExpressionAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() == T_TRUE || LA() == T_FALSE) {
        BoolLiteralAST* ast = new (_pool) BoolLiteralAST;
        ast->literal_token = consumeToken();
        node = ast;
        return true;
    }
    return false;
}

bool Parser::parseNumericLiteral(ExpressionAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() == T_NUMERIC_LITERAL  ||
        LA() == T_CHAR_LITERAL     ||
        LA() == T_WIDE_CHAR_LITERAL ||
        LA() == T_UTF16_CHAR_LITERAL ||
        LA() == T_UTF32_CHAR_LITERAL) {
        NumericLiteralAST* ast = new (_pool) NumericLiteralAST;
        ast->literal_token = consumeToken();
        node = ast;
        return true;
    }
    return false;
}

bool Parser::parsePointerLiteral(ExpressionAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() == T_NULLPTR) {
        PointerLiteralAST* ast = new (_pool) PointerLiteralAST;
        ast->literal_token = consumeToken();
        node = ast;
        return true;
    }
    return false;
}

bool Parser::parseThisExpression(ExpressionAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() == T_THIS) {
        ThisExpressionAST* ast = new (_pool) ThisExpressionAST;
        ast->this_token = consumeToken();
        node = ast;
        return true;
    }
    return false;
}

bool Parser::parsePrimaryExpression(ExpressionAST* &node)
{
    DEBUG_THIS_RULE();
    switch (LA()) {
    case T_STRING_LITERAL:
    case T_WIDE_STRING_LITERAL:
    case T_UTF8_STRING_LITERAL:
    case T_UTF16_STRING_LITERAL:
    case T_UTF32_STRING_LITERAL:
    case T_RAW_STRING_LITERAL:
    case T_RAW_WIDE_STRING_LITERAL:
    case T_RAW_UTF8_STRING_LITERAL:
    case T_RAW_UTF16_STRING_LITERAL:
    case T_RAW_UTF32_STRING_LITERAL:
        return parseStringLiteral(node);

    case T_NULLPTR:
        if (_language.cpp11 || _language.nullptrOnNULL)
            return parsePointerLiteral(node);
        // fall-through

    case T_CHAR_LITERAL: // ### FIXME don't use NumericLiteral for chars
    case T_WIDE_CHAR_LITERAL:
    case T_UTF16_CHAR_LITERAL:
    case T_UTF32_CHAR_LITERAL:
    case T_NUMERIC_LITERAL:
        return parseNumericLiteral(node);

    case T_TRUE:
    case T_FALSE:
        return parseBoolLiteral(node);

    case T_THIS:
        return parseThisExpression(node);

    case T_LPAREN:
        if (LA(2) == T_LBRACE) {
            // GNU extension: '(' '{' statement-list '}' ')'
            CompoundExpressionAST* ast = new (_pool) CompoundExpressionAST;
            ast->lparen_token = consumeToken();
            StatementAST* statement = 0;
            parseCompoundStatement(statement);
            ast->statement = statement->asCompoundStatement();
            match(T_RPAREN, &ast->rparen_token);
            node = ast;
            return true;
        } else {
            return parseNestedExpression(node);
        }

    case T_LBRACKET: {
        if (_language.cpp11) {
            if (parseLambdaExpression(node))
                return true;
        }
    } break;

    default: {
        NameAST* name = 0;
        if (parseNameId(name)) {
            IdExpressionAST* ast = new (_pool) IdExpressionAST;
            ast->name = name;
            node = ast;
            return true;
        }
        break;
    } // default

    } // switch

    return false;
}

bool Parser::parseNameId(NameAST* &name)
{
    DEBUG_THIS_RULE();
    unsigned start = cursor();
    if (! parseName(name))
        return false;

    if (LA() == T_RPAREN || LA() == T_COMMA)
        return true;

    QualifiedNameAST* qualified_name_id = name->asQualifiedName();

    TemplateIdAST* template_id = 0;
    if (qualified_name_id) {
        if (NameAST* unqualified_name = qualified_name_id->unqualified_name)
            template_id = unqualified_name->asTemplateId();
    } else {
        template_id = name->asTemplateId();
    }

    if (! template_id)
        return true; // it's not a template-id, there's nothing to rewind.

    else if (LA() == T_LPAREN) {
        // a template-id followed by a T_LPAREN
        if (ExpressionListAST* template_arguments = template_id->template_argument_list) {
            if (! template_arguments->next && template_arguments->value &&
                    template_arguments->value->asBinaryExpression()) {

                unsigned saved = cursor();
                ExpressionAST* expr = 0;

                bool blocked = blockErrors(true);
                bool lookAtCastExpression = parseCastExpression(expr);
                (void) blockErrors(blocked);

                if (lookAtCastExpression) {
                    if (CastExpressionAST* cast_expression = expr->asCastExpression()) {
                        if (cast_expression->lparen_token && cast_expression->rparen_token
                                && cast_expression->type_id && cast_expression->expression) {
                            rewind(start);

                            name = 0;
                            return parseName(name, false);
                        }
                    }
                }
                rewind(saved);
            }
        }
    }

    switch (LA()) {
    case T_COMMA:
    case T_SEMICOLON:
    case T_LBRACKET:
    case T_LPAREN:
    case T_LBRACE:
        return true;

    case T_THIS:
    case T_IDENTIFIER:
    case T_STATIC_CAST:
    case T_DYNAMIC_CAST:
    case T_REINTERPRET_CAST:
    case T_CONST_CAST:
        rewind(start);
        return parseName(name, false);

    default:
        if (token().isLiteral() || token().isOperator()) {
            rewind(start);
            return parseName(name, false);
        }
    } // switch

    return true;
}

bool Parser::parseNestedExpression(ExpressionAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() == T_LPAREN) {
        unsigned lparen_token = consumeToken();
        bool previousTemplateArguments = switchTemplateArguments(false);

        ExpressionAST* expression = 0;
        if (parseExpression(expression) && LA() == T_RPAREN) {
            NestedExpressionAST* ast = new (_pool) NestedExpressionAST;
            ast->lparen_token = lparen_token;
            ast->expression = expression;
            ast->rparen_token = consumeToken();
            node = ast;
            (void) switchTemplateArguments(previousTemplateArguments);
            return true;
        }
        (void) switchTemplateArguments(previousTemplateArguments);
    }
    return false;
}

bool Parser::parseCppCastExpression(ExpressionAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() == T_DYNAMIC_CAST     || LA() == T_STATIC_CAST ||
        LA() == T_REINTERPRET_CAST || LA() == T_CONST_CAST) {
        CppCastExpressionAST* ast = new (_pool) CppCastExpressionAST;
        ast->cast_token = consumeToken();
        match(T_LESS, &ast->less_token);
        parseTypeId(ast->type_id);
        match(T_GREATER, &ast->greater_token);
        match(T_LPAREN, &ast->lparen_token);
        parseExpression(ast->expression);
        match(T_RPAREN, &ast->rparen_token);
        node = ast;
        return true;
    }
    return false;
}

// typename ::opt  nested-name-specifier identifier ( expression-listopt )
// typename ::opt  nested-name-specifier templateopt  template-id ( expression-listopt )
bool Parser::parseTypenameCallExpression(ExpressionAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() == T_TYPENAME) {
        unsigned typename_token = consumeToken();
        NameAST* name = 0;
        if (parseName(name)
                && (LA() == T_LPAREN || (_language.cpp11 && LA() == T_LBRACE))) {
            TypenameCallExpressionAST* ast = new (_pool) TypenameCallExpressionAST;
            ast->typename_token = typename_token;
            ast->name = name;
            if (LA() == T_LPAREN) {
                parseExpressionListParen(ast->expression);
            } else { // T_LBRACE
                parseBracedInitList0x(ast->expression);
            }
            node = ast;
            return true;
        }
    }
    return false;
}

// typeid ( expression )
// typeid ( type-id )
bool Parser::parseTypeidExpression(ExpressionAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() == T_TYPEID) {
        TypeidExpressionAST* ast = new (_pool) TypeidExpressionAST;
        ast->typeid_token = consumeToken();
        if (LA() == T_LPAREN)
            ast->lparen_token = consumeToken();
        unsigned saved = cursor();
        if (! (parseTypeId(ast->expression) && LA() == T_RPAREN)) {
            rewind(saved);
            parseExpression(ast->expression);
        }
        match(T_RPAREN, &ast->rparen_token);
        node = ast;
        return true;
    }
    return false;
}

bool Parser::parseCorePostfixExpression(ExpressionAST* &node)
{
    DEBUG_THIS_RULE();

    switch (LA()) {
    case T_DYNAMIC_CAST:
    case T_STATIC_CAST:
    case T_REINTERPRET_CAST:
    case T_CONST_CAST:
        return parseCppCastExpression(node);

    case T_TYPENAME:
        return parseTypenameCallExpression(node);

    case T_TYPEID:
        return parseTypeidExpression(node);

    default: {
        unsigned start = cursor();
        SpecifierListAST* type_specifier = 0;
        bool blocked = blockErrors(true);
        if (lookAtBuiltinTypeSpecifier() &&
                parseSimpleTypeSpecifier(type_specifier) &&
                (LA() == T_LPAREN || (_language.cpp11 && LA() == T_LBRACE))) {
            ExpressionAST* expr = 0;
            if (LA() == T_LPAREN) {
                parseExpressionListParen(expr);
            } else { // T_LBRACE
                parseBracedInitList0x(expr);
            }
            TypeConstructorCallAST* ast = new (_pool) TypeConstructorCallAST;
            ast->type_specifier_list = type_specifier;
            ast->expression = expr;
            node = ast;
            blockErrors(blocked);
            return true;
        }
        rewind(start);

        // look for compound literals
        if (LA() == T_LPAREN) {
            unsigned lparen_token = consumeToken();
            ExpressionAST* type_id = 0;
            if (parseTypeId(type_id) && LA() == T_RPAREN) {
                unsigned rparen_token = consumeToken();
                if (LA() == T_LBRACE) {
                    blockErrors(blocked);

                    CompoundLiteralAST* ast = new (_pool) CompoundLiteralAST;
                    ast->lparen_token = lparen_token;
                    ast->type_id = type_id;
                    ast->rparen_token = rparen_token;
                    parseInitializerClause(ast->initializer);
                    node = ast;
                    return true;
                }
            }
            rewind(start);
        }

        blockErrors(blocked);
        return parsePrimaryExpression(node);
    } // default
    } // switch
}

bool Parser::parsePostfixExpression(ExpressionAST* &node)
{
    DEBUG_THIS_RULE();
    if (parseCorePostfixExpression(node)) {
        while (LA()) {
            if (LA() == T_LPAREN) {
                CallAST* ast = new (_pool) CallAST;
                ast->lparen_token = consumeToken();
                parseExpressionList(ast->expression_list);
                match(T_RPAREN, &ast->rparen_token);
                ast->base_expression = node;
                node = ast;
            } else if (LA() == T_LBRACKET) {
                ArrayAccessAST* ast = new (_pool) ArrayAccessAST;
                ast->lbracket_token = consumeToken();
                parseExpression(ast->expression);
                match(T_RBRACKET, &ast->rbracket_token);
                ast->base_expression = node;
                node = ast;
            } else if (_language.cpp11 && LA() == T_LBRACE && node->asIdExpression()) {
                // this is slightly inconsistent: simple-type-specifier '(' expression-list ')'
                // gets parsed as a CallAST while simple-type-specifier brace-init-list
                // is a TypenameCallExpressionAST
                TypenameCallExpressionAST* ast = new (_pool) TypenameCallExpressionAST;
                ast->name = node->asIdExpression()->name;
                parseBracedInitList0x(ast->expression);
                node = ast;
            } else if (LA() == T_PLUS_PLUS || LA() == T_MINUS_MINUS) {
                PostIncrDecrAST* ast = new (_pool) PostIncrDecrAST;
                ast->incr_decr_token = consumeToken();
                ast->base_expression = node;
                node = ast;
            } else if (LA() == T_DOT || LA() == T_ARROW) {
                MemberAccessAST* ast = new (_pool) MemberAccessAST;
                ast->access_token = consumeToken();
                if (LA() == T_TEMPLATE)
                    ast->template_token = consumeToken();
                if (! parseNameId(ast->member_name))
                    error(cursor(), "expected unqualified-id before token `%s'", token().spell());
                ast->base_expression = node;
                node = ast;
            } else break;
        } // while

        return true;
    }
    return false;
}

bool Parser::parseUnaryExpression(ExpressionAST* &node)
{
    DEBUG_THIS_RULE();
    switch (LA()) {
    case T_PLUS_PLUS:
    case T_MINUS_MINUS:
    case T_STAR:
    case T_AMPER:
    case T_PLUS:
    case T_MINUS:
    case T_EXCLAIM: {
        unsigned op = cursor();
        UnaryExpressionAST* ast = new (_pool) UnaryExpressionAST;
        ast->unary_op_token = consumeToken();
        if (! parseCastExpression(ast->expression))
            error(op, "expected expression after token `%s'", _translationUnit->spell(op));
        node = ast;
        return true;
    }

    case T_TILDE: {
        if (LA(2) == T_IDENTIFIER && LA(3) == T_LPAREN)
            break; // prefer destructor names

        UnaryExpressionAST* ast = new (_pool) UnaryExpressionAST;
        ast->unary_op_token = consumeToken();
        (void) parseCastExpression(ast->expression);
        node = ast;
        return true;
    }

    case T_SIZEOF: {
        SizeofExpressionAST* ast = new (_pool) SizeofExpressionAST;
        ast->sizeof_token = consumeToken();

        // sizeof...(Args)
        if (_language.cpp11 && LA() == T_DOT_DOT_DOT && (LA(2) == T_IDENTIFIER || (LA(2) == T_LPAREN && LA(3) == T_IDENTIFIER
                                                                                 && LA(4) == T_RPAREN)))
            ast->dot_dot_dot_token = consumeToken();

        if (LA() == T_LPAREN) {
            unsigned lparen_token = consumeToken();
            const bool blocked = blockErrors(true);
            const bool hasTypeId = parseTypeId(ast->expression);
            (void) blockErrors(blocked);
            if (hasTypeId && LA() == T_RPAREN) {
                ast->lparen_token = lparen_token;
                ast->rparen_token = consumeToken();
                node = ast;
                return true;
            } else {
                rewind(lparen_token);
            }
        }

        parseUnaryExpression(ast->expression);

        node = ast;
        return true;
    }

    case T_ALIGNOF: {
        if (!_language.cpp11)
            break;

        AlignofExpressionAST* ast = new (_pool) AlignofExpressionAST;
        ast->alignof_token = consumeToken();

        match(T_LPAREN, &ast->lparen_token);
        ExpressionAST* temp = 0;
        parseTypeId(temp);
        if (temp)
            ast->typeId = temp->asTypeId();
        match(T_RPAREN, &ast->rparen_token);

        node = ast;
        return true;
    }


    case T_NOEXCEPT: {
        if (!_language.cpp11)
            break;

        return parseNoExceptOperatorExpression(node);
    }

    default:
        break;
    } // switch

    if (LA() == T_NEW || (LA(1) == T_COLON_COLON &&
                          LA(2) == T_NEW))
        return parseNewExpression(node);
    else if (LA() == T_DELETE || (LA(1) == T_COLON_COLON &&
                                  LA(2) == T_DELETE))
        return parseDeleteExpression(node);
    else
        return parsePostfixExpression(node);
}

// new-placement ::= T_LPAREN expression-list T_RPAREN
bool Parser::parseExpressionListParen(ExpressionAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() == T_LPAREN) {
        unsigned lparen_token = consumeToken();
        ExpressionListAST* expression_list = 0;
        if (parseExpressionList(expression_list) && LA() == T_RPAREN) {
            unsigned rparen_token = consumeToken();
            ExpressionListParenAST* ast = new (_pool) ExpressionListParenAST;
            ast->lparen_token = lparen_token;
            ast->expression_list = expression_list;
            ast->rparen_token = rparen_token;
            node = ast;
            return true;
        }
    }

    return false;
}


// new-expression ::= T_COLON_COLON? T_NEW new-placement.opt
//                    new-type-id new-initializer.opt
// new-expression ::= T_COLON_COLON? T_NEW new-placement.opt
//                    T_LPAREN type-id T_RPAREN new-initializer.opt
bool Parser::parseNewExpression(ExpressionAST* &node)
{
    DEBUG_THIS_RULE();
    if (! (LA() == T_NEW || (LA() == T_COLON_COLON && LA(2) == T_NEW)))
        return false;

    NewExpressionAST* ast = new (_pool) NewExpressionAST;
    if (LA() == T_COLON_COLON)
        ast->scope_token = consumeToken();

    ast->new_token = consumeToken();

    ExpressionAST* parenExpressionList = 0;

    if (parseExpressionListParen(parenExpressionList)) {
        unsigned after_new_placement = cursor();

        NewTypeIdAST* new_type_id = 0;
        if (parseNewTypeId(new_type_id)) {
            ast->new_placement = parenExpressionList->asExpressionListParen();
            ast->new_type_id = new_type_id;
            parseNewInitializer(ast->new_initializer);
            // recognized new-placement.opt new-type-id new-initializer.opt
            node = ast;
            return true;
        }

        rewind(after_new_placement);
        if (LA() == T_LPAREN) {
            unsigned lparen_token = consumeToken();
            ExpressionAST* type_id = 0;
            if (parseTypeId(type_id) && LA() == T_RPAREN) {
                ast->new_placement = parenExpressionList->asExpressionListParen();
                ast->lparen_token = lparen_token;
                ast->type_id = type_id;
                ast->rparen_token = consumeToken();
                parseNewInitializer(ast->new_initializer);
                node = ast;
                return true;
            }
        }
    }

    rewind(ast->new_token + 1);

    if (LA() == T_LPAREN) {
        unsigned lparen_token = consumeToken();
        ExpressionAST* type_id = 0;
        if (parseTypeId(type_id) && LA() == T_RPAREN) {
            ast->lparen_token = lparen_token;
            ast->type_id = type_id;
            ast->rparen_token = consumeToken();
            parseNewInitializer(ast->new_initializer);
            node = ast;
            return true;
        }
    }

    parseNewTypeId(ast->new_type_id);
    parseNewInitializer(ast->new_initializer);
    node = ast;
    return true;
}

bool Parser::parseNewTypeId(NewTypeIdAST* &node)
{
    DEBUG_THIS_RULE();
    SpecifierListAST* typeSpec = 0;
    if (! parseTypeSpecifier(typeSpec))
        return false;

    NewTypeIdAST* ast = new (_pool) NewTypeIdAST;
    ast->type_specifier_list = typeSpec;

    PtrOperatorListAST* *ptrop_it = &ast->ptr_operator_list;
    while (parsePtrOperator(*ptrop_it))
        ptrop_it = &(*ptrop_it)->next;

    NewArrayDeclaratorListAST* *it = &ast->new_array_declarator_list;
    while (parseNewArrayDeclarator(*it))
        it = &(*it)->next;

    node = ast;
    return true;
}


bool Parser::parseNewArrayDeclarator(NewArrayDeclaratorListAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() != T_LBRACKET)
        return false;

    NewArrayDeclaratorAST* ast = new (_pool) NewArrayDeclaratorAST;
    ast->lbracket_token = consumeToken();
    parseExpression(ast->expression);
    match(T_RBRACKET, &ast->rbracket_token);

    node = new (_pool) NewArrayDeclaratorListAST;
    node->value = ast;
    return true;
}

bool Parser::parseNewInitializer(ExpressionAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() == T_LPAREN)
        return parseExpressionListParen(node);
    else if (_language.cpp11 && LA() == T_LBRACE)
        return parseBracedInitList0x(node);
    return false;
}

bool Parser::parseDeleteExpression(ExpressionAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() == T_DELETE || (LA() == T_COLON_COLON && LA(2) == T_DELETE)) {
        DeleteExpressionAST* ast = new (_pool) DeleteExpressionAST;

        if (LA() == T_COLON_COLON)
            ast->scope_token = consumeToken();

        ast->delete_token = consumeToken();

        if (LA() == T_LBRACKET) {
            ast->lbracket_token = consumeToken();
            match(T_RBRACKET, &ast->rbracket_token);
        }

        (void) parseCastExpression(ast->expression);
        node = ast;
        return true;
    }
    return false;
}

bool Parser::parseCastExpression(ExpressionAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() == T_LPAREN) {
        unsigned lparen_token = consumeToken();
        unsigned initialCursor = cursor();
        ExpressionAST* type_id = 0;
        if (parseTypeId(type_id) && LA() == T_RPAREN) {

            if (TypeIdAST* tid = type_id->asTypeId()) {
                if (tid->type_specifier_list && ! tid->type_specifier_list->next) {
                    if (tid->type_specifier_list->value->asNamedTypeSpecifier()) {
                        switch (LA(2)) {
                        case T_LBRACKET: // ... it's definitely a unary expression followed by an array access.
                            goto parse_as_unary_expression;

                        case T_PLUS_PLUS:
                        case T_MINUS_MINUS: {
                            const unsigned rparen_token = consumeToken();

                            const bool blocked = blockErrors(true);
                            ExpressionAST* unary = 0;
                            bool followedByUnaryExpression = parseUnaryExpression(unary);
                            blockErrors(blocked);
                            rewind(rparen_token);

                            if (followedByUnaryExpression) {
                                if (! unary)
                                    followedByUnaryExpression = false;
                                else if (UnaryExpressionAST* u = unary->asUnaryExpression())
                                    followedByUnaryExpression = u->expression != 0;
                            }

                            if (! followedByUnaryExpression)
                                goto parse_as_unary_expression;

                        }   break;

                        case T_LPAREN: // .. it can be parsed as a function call.
                            // ### TODO: check if it is followed by a parenthesized expression list.
                            break;
                        }
                    }
                }
            }

            unsigned rparen_token = consumeToken();
            ExpressionAST* expression = 0;
            if (parseCastExpression(expression)) {
                CastExpressionAST* ast = new (_pool) CastExpressionAST;
                ast->lparen_token = lparen_token;
                ast->type_id = type_id;
                ast->rparen_token = rparen_token;
                ast->expression = expression;
                node = ast;
                return true;
            }
        }

parse_as_unary_expression:
        _astCache->insert(ASTCache::TypeId, initialCursor, 0, cursor(), false);
        rewind(lparen_token);
    }

    return parseUnaryExpression(node);
}

bool Parser::parsePmExpression(ExpressionAST* &node)
{
    PARSE_EXPRESSION_WITH_OPERATOR_PRECEDENCE(node, Prec::PointerToMember)
}

bool Parser::parseMultiplicativeExpression(ExpressionAST* &node)
{
    PARSE_EXPRESSION_WITH_OPERATOR_PRECEDENCE(node, Prec::Multiplicative)
}

bool Parser::parseAdditiveExpression(ExpressionAST* &node)
{
    PARSE_EXPRESSION_WITH_OPERATOR_PRECEDENCE(node, Prec::Additive)
}

bool Parser::parseShiftExpression(ExpressionAST* &node)
{
    PARSE_EXPRESSION_WITH_OPERATOR_PRECEDENCE(node, Prec::Shift)
}

bool Parser::parseRelationalExpression(ExpressionAST* &node)
{
    PARSE_EXPRESSION_WITH_OPERATOR_PRECEDENCE(node, Prec::Relational)
}

bool Parser::parseEqualityExpression(ExpressionAST* &node)
{
    PARSE_EXPRESSION_WITH_OPERATOR_PRECEDENCE(node, Prec::Equality)
}

bool Parser::parseAndExpression(ExpressionAST* &node)
{
    PARSE_EXPRESSION_WITH_OPERATOR_PRECEDENCE(node, Prec::And)
}

bool Parser::parseExclusiveOrExpression(ExpressionAST* &node)
{
    PARSE_EXPRESSION_WITH_OPERATOR_PRECEDENCE(node, Prec::ExclusiveOr)
}

bool Parser::parseInclusiveOrExpression(ExpressionAST* &node)
{
    PARSE_EXPRESSION_WITH_OPERATOR_PRECEDENCE(node, Prec::InclusiveOr)
}

bool Parser::parseLogicalAndExpression(ExpressionAST* &node)
{
    PARSE_EXPRESSION_WITH_OPERATOR_PRECEDENCE(node, Prec::LogicalAnd)
}

bool Parser::parseLogicalOrExpression(ExpressionAST* &node)
{
    PARSE_EXPRESSION_WITH_OPERATOR_PRECEDENCE(node, Prec::LogicalOr)
}

bool Parser::parseConditionalExpression(ExpressionAST* &node)
{
    PARSE_EXPRESSION_WITH_OPERATOR_PRECEDENCE(node, Prec::Conditional)
}

bool Parser::parseAssignmentExpression(ExpressionAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() == T_THROW)
        return parseThrowExpression(node);
    else
        PARSE_EXPRESSION_WITH_OPERATOR_PRECEDENCE(node, Prec::Assignment)
}

bool Parser::parseConstantExpression(ExpressionAST* &node)
{
    DEBUG_THIS_RULE();
    return parseConditionalExpression(node);
}

bool Parser::parseExpression(ExpressionAST* &node)
{
    DEBUG_THIS_RULE();
    CHECK_CACHE(ASTCache::Expression, ExpressionAST);
    unsigned initialCursor = cursor();

    if (_expressionDepth > MAX_EXPRESSION_DEPTH)
        return false;

    ++_expressionDepth;
    bool success = parseCommaExpression(node);
    --_expressionDepth;

    _astCache->insert(ASTCache::Expression, initialCursor, node, cursor(), success);
    return success;
}

void Parser::parseExpressionWithOperatorPrecedence(ExpressionAST* &lhs, int minPrecedence)
{
    DEBUG_THIS_RULE();

    unsigned iterations = 0;
    while (precedence(token().kind(), _templateArguments) >= minPrecedence) {
        if (++iterations > MAX_EXPRESSION_DEPTH) {
            warning(cursor(), "Reached parse limit for expression");
            return;
        }

        const int operPrecedence = precedence(token().kind(), _templateArguments);
        const int oper = consumeToken();

        ConditionalExpressionAST* condExpr = 0;
        if (operPrecedence == Prec::Conditional) {
            condExpr = new (_pool) ConditionalExpressionAST;
            condExpr->question_token = oper;
            if (token().kind() == T_COLON) {
                // GNU extension:
                //   logical-or-expression '?' ':' conditional-expression
                condExpr->left_expression = 0;
            } else {
                parseExpression(condExpr->left_expression);
            }
            match(T_COLON, &condExpr->colon_token);
        }

        ExpressionAST* rhs = 0;
        const bool isCPlusPlus = true;
        if (operPrecedence <= Prec::Conditional && isCPlusPlus) {
            // in C++ you can put a throw in the right-most expression of a conditional expression,
            // or an assignment, so some special handling:
            if (_language.cpp11) {
                if (!parseInitializerClause0x(rhs))
                    return;
            } else {
                if (!parseAssignmentExpression(rhs))
                    return;
            }
        } else {
            // for C & all other expressions:
            if (!parseCastExpression(rhs))
                return;
        }

        for (int tokenKindAhead = token().kind(), precedenceAhead = precedence(tokenKindAhead, _templateArguments);
                (precedenceAhead > operPrecedence && isBinaryOperator(tokenKindAhead))
                        || (precedenceAhead == operPrecedence && isRightAssociative(tokenKindAhead));
                tokenKindAhead = token().kind(), precedenceAhead = precedence(tokenKindAhead, _templateArguments)) {
            parseExpressionWithOperatorPrecedence(rhs, precedenceAhead);
        }

        if (condExpr) { // we were parsing a ternairy conditional expression
            condExpr->condition = lhs;
            condExpr->right_expression = rhs;
            lhs = condExpr;
        } else {
            BinaryExpressionAST* expr = new (_pool) BinaryExpressionAST;
            expr->left_expression = lhs;
            expr->binary_op_token = oper;
            expr->right_expression = rhs;
            lhs = expr;
        }
    }
}

bool Parser::parseCommaExpression(ExpressionAST* &node)
{
    PARSE_EXPRESSION_WITH_OPERATOR_PRECEDENCE(node, Prec::Comma)
}

bool Parser::parseThrowExpression(ExpressionAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() == T_THROW) {
        ThrowExpressionAST* ast = new (_pool) ThrowExpressionAST;
        ast->throw_token = consumeToken();
        parseAssignmentExpression(ast->expression);
        node = ast;
        return true;
    }
    return false;
}

bool Parser::parseNoExceptOperatorExpression(ExpressionAST* &node)
{
    DEBUG_THIS_RULE();
    if (_language.cpp11 && LA() == T_NOEXCEPT) {
        NoExceptOperatorExpressionAST* ast = new (_pool) NoExceptOperatorExpressionAST;
        ast->noexcept_token = consumeToken();
        parseExpression(ast->expression);
        node = ast;
        return true;
    }
    return false;
}

// designated-initializer ::= designator* T_EQUAL initializer-clause
//
bool Parser::parseDesignatedInitializer(ExpressionAST* &node)
{
    DEBUG_THIS_RULE();
    if (!_language.c99 || (LA() != T_DOT && LA() != T_LBRACKET))
        return false;

    DesignatedInitializerAST* ast = new (_pool) DesignatedInitializerAST;
    DesignatorListAST* *designator_list_ptr = &ast->designator_list;
    DesignatorAST* designator = 0;
    const unsigned start = cursor();
    while (parseDesignator(designator)) {
        *designator_list_ptr = new (_pool) DesignatorListAST;
        (*designator_list_ptr)->value = designator;
        designator_list_ptr = &(*designator_list_ptr)->next;
    }
    if (start == cursor())
        return false;

    if (LA() == T_EQUAL) {
        ast->equal_token = consumeToken();

        if (LA() == T_LBRACE) {
            if (parseBracedInitList0x(node)) {
                node = ast;
                return true;
            }
        } else if (parseAssignmentExpression(ast->initializer)) {
            node = ast;
            return true;
        }
    }

    rewind(start);
    return false;
}

// designator ::= T_DOT T_IDENTIFIER
//                T_LBRACKET constant-expression T_BRACKET
//
bool Parser::parseDesignator(DesignatorAST* &node)
{
    DEBUG_THIS_RULE();
    const unsigned start = cursor();
    if (LA() == T_DOT) {
        DotDesignatorAST* ast = new (_pool) DotDesignatorAST;
        ast->dot_token = consumeToken();
        match(T_IDENTIFIER, &ast->identifier_token);
        node = ast;
        return true;
    } else if (LA() == T_LBRACKET) {
        BracketDesignatorAST* ast = new (_pool) BracketDesignatorAST;
        ast->lbracket_token = consumeToken();
        if (parseConditionalExpression(ast->expression)) {
            match(T_RBRACKET, &ast->rbracket_token);
            node = ast;
            return true;
        }
    }

    rewind(start);
    return false;
}

bool Parser::parseLambdaExpression(ExpressionAST* &node)
{
    DEBUG_THIS_RULE();

    LambdaIntroducerAST* lambda_introducer = 0;
    if (parseLambdaIntroducer(lambda_introducer)) {
        LambdaExpressionAST* ast = new (_pool) LambdaExpressionAST;
        ast->lambda_introducer = lambda_introducer;
        parseLambdaDeclarator(ast->lambda_declarator);
        parseCompoundStatement(ast->statement);
        node = ast;
        return true;
    }

    return false;
}

bool Parser::parseLambdaIntroducer(LambdaIntroducerAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() != T_LBRACKET)
        return false;

    LambdaIntroducerAST* ast = new (_pool) LambdaIntroducerAST;
    ast->lbracket_token = consumeToken();

    if (LA() != T_RBRACKET)
        parseLambdaCapture(ast->lambda_capture);

    if (LA() == T_RBRACKET) {
        ast->rbracket_token = consumeToken();

        if (LA() == T_LPAREN || LA() == T_LBRACE) {
            node = ast;
            return true;
        }
    }

    return false;
}

bool Parser::parseLambdaCapture(LambdaCaptureAST* &node)
{
    DEBUG_THIS_RULE();
    bool startsWithDefaultCapture = false;

    unsigned default_capture = 0;
    CaptureListAST* capture_list = 0;

    if (LA() == T_AMPER || LA() == T_EQUAL) {
        if (LA(2) == T_COMMA || LA(2) == T_RBRACKET) {
            startsWithDefaultCapture = true;
            default_capture = consumeToken(); // consume capture-default
        }
    }

    if (startsWithDefaultCapture && LA() == T_COMMA) {
        consumeToken(); // consume ','
        parseCaptureList(capture_list); // required

    } else if (LA() != T_RBRACKET) {
        parseCaptureList(capture_list); // optional

    }

    LambdaCaptureAST* ast = new (_pool) LambdaCaptureAST;
    ast->default_capture_token = default_capture;
    ast->capture_list = capture_list;
    node = ast;

    return true;
}

bool Parser::parseCapture(CaptureAST* &node)
{
    DEBUG_THIS_RULE();

    if (LA() == T_THIS) {
        consumeToken();
        return true;
    }

    if (LA() == T_AMPER)
        consumeToken();

    if (LA() == T_IDENTIFIER) {
        SimpleNameAST* ast = new (_pool) SimpleNameAST;
        ast->identifier_token = consumeToken();

        node = new (_pool) CaptureAST;
        node->identifier = ast;
        return true;
    }

    return false;
}

bool Parser::parseCaptureList(CaptureListAST* &node)
{
    DEBUG_THIS_RULE();

    CaptureAST* capture = 0;

    if (parseCapture(capture)) {
        node = new (_pool) CaptureListAST;
        node->value = capture;

        CaptureListAST* *l = &node->next;
        while (LA() == T_COMMA) {
            consumeToken(); // consume `,'
            CaptureAST* capture = 0;
            parseCapture(capture);
            if (capture) {
                *l = new (_pool) CaptureListAST;
                (*l)->value = capture;
                l = &(*l)->next;
            }
        }

        return true;
    }

    return false;
}

bool Parser::parseLambdaDeclarator(LambdaDeclaratorAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() != T_LPAREN)
        return false;

    LambdaDeclaratorAST* ast = new (_pool) LambdaDeclaratorAST;

    ast->lparen_token = consumeToken(); // consume `('
    parseParameterDeclarationClause(ast->parameter_declaration_clause);
    match(T_RPAREN, &ast->rparen_token);

    SpecifierListAST* *attr = &ast->attributes;
    while (parseGnuAttributeSpecifier(*attr))
        attr = &(*attr)->next;

    if (LA() == T_MUTABLE)
        ast->mutable_token = consumeToken();

    parseExceptionSpecification(ast->exception_specification);
    parseTrailingReturnType(ast->trailing_return_type);
    node = ast;

    return true;
}

bool Parser::parseTrailingReturnType(TrailingReturnTypeAST* &node)
{
    DEBUG_THIS_RULE();
    if (LA() != T_ARROW)
        return false;

    TrailingReturnTypeAST* ast = new (_pool) TrailingReturnTypeAST;

    ast->arrow_token = consumeToken();

    SpecifierListAST* *attr = &ast->attributes;
    while (parseGnuAttributeSpecifier(*attr))
        attr = &(*attr)->next;

    parseTrailingTypeSpecifierSeq(ast->type_specifier_list);
    parseAbstractDeclarator(ast->declarator, ast->type_specifier_list);
    node = ast;
    return true;
}

void Parser::rewind(unsigned cursor)
{
#ifndef PSYCHE_NO_DEBUG_RULE
    if (cursor != _tokenIndex)
        fprintf(stderr, "! rewinding from token %d to token %d\n", _tokenIndex, cursor);
#endif

    const unsigned n = _translationUnit->tokenCount();
    if (cursor < n)
        _tokenIndex = cursor;
    else
        _tokenIndex = n - 1;
}

void Parser::warning(unsigned index, const char *format, ...)
{
    va_list args, ap;
    va_start(args, format);
    va_copy(ap, args);
    _translationUnit->message(DiagnosticCollector::Warning, index, format, ap);
    va_end(ap);
    va_end(args);
}

void Parser::error(unsigned index, const char *format, ...)
{
    va_list args, ap;
    va_start(args, format);
    va_copy(ap, args);
    _translationUnit->message(DiagnosticCollector::Error, index, format, ap);
    va_end(ap);
    va_end(args);
}

void Parser::fatal(unsigned index, const char *format, ...)
{
    va_list args, ap;
    va_start(args, format);
    va_copy(ap, args);
    _translationUnit->message(DiagnosticCollector::Fatal, index, format, ap);
    va_end(ap);
    va_end(args);
}
