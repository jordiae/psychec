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

#include "Control.h"

#include "CoreTypes.h"
#include "Literals.h"
#include "LiteralTable.h"
#include "Names.h"
#include "Symbols.h"
#include "TranslationUnit.h"
#include <algorithm>
#include <map>
#include <set>
#include <unordered_set>

using namespace psyche;

namespace {

template <typename T>
struct Compare;

template <> struct Compare<IntegerType>
{
    bool operator()(const IntegerType& ty, const IntegerType& otherTy) const
    { return ty.kind() < otherTy.kind(); }
};

template <> struct Compare<FloatType>
{
    bool operator()(const FloatType& ty, const FloatType& otherTy) const
    { return ty.kind() < otherTy.kind(); }
};

template <> struct Compare<PointerToMemberType>
{
    bool operator()(const PointerToMemberType& ty, const PointerToMemberType& otherTy) const
    {
        if (ty.memberName() < otherTy.memberName())
            return true;

        else if (ty.memberName() == otherTy.memberName())
            return ty.elementType() < otherTy.elementType();

        return false;
    }
};

template <> struct Compare<PointerType>
{
    bool operator()(const PointerType& ty, const PointerType& otherTy) const
    {
        return ty.elementType() < otherTy.elementType();
    }
};

template <> struct Compare<ReferenceType>
{
    bool operator()(const ReferenceType& ty, const ReferenceType& otherTy) const
    {
        return ty.elementType() < otherTy.elementType();
    }
};

template <> struct Compare<NamedType>
{
    bool operator()(const NamedType& ty, const NamedType& otherTy) const
    {
        return ty.name() < otherTy.name();
    }
};

template <> struct Compare<QuantifiedType>
{
    bool operator()(const QuantifiedType& ty, const QuantifiedType& otherTy) const
    {
        if (ty.label() < otherTy.label())
            return true;

        if (ty.kind() < otherTy.kind())
            return true;

        return ty.name() < otherTy.name();
    }
};

template <> struct Compare<ArrayType>
{
    bool operator()(const ArrayType& ty, const ArrayType& otherTy) const
    {
        if (ty.size() < otherTy.size())
            return true;

        else if (ty.size() == otherTy.size())
            return ty.elementType() < otherTy.elementType();

        return false;
    }
};

template <> struct Compare<DestructorNameId>
{
    bool operator()(const DestructorNameId &name, const DestructorNameId &otherName) const
    {
        return name.identifier() < otherName.identifier();
    }
};

template <> struct Compare<OperatorNameId>
{
    bool operator()(const OperatorNameId &name, const OperatorNameId &otherName) const
    {
        return name.kind() < otherName.kind();
    }
};

template <> struct Compare<ConversionNameId>
{
    bool operator()(const ConversionNameId &name, const ConversionNameId &otherName) const
    {
        return name.type() < otherName.type();
    }
};
template <> struct Compare<TemplateNameId>
{
    bool operator()(const TemplateNameId &name, const TemplateNameId &otherName) const
    {
        const Identifier* id = name.identifier();
        const Identifier* otherId = otherName.identifier();

        if (id == otherId) {
            // we have to differentiate TemplateNameId with respect to specialization or
            // instantiation
            if (name.isSpecialization() == otherName.isSpecialization()) {
                return std::lexicographical_compare(name.firstTemplateArgument(),
                                                    name.lastTemplateArgument(),
                                                    otherName.firstTemplateArgument(),
                                                    otherName.lastTemplateArgument());
            } else {
                return name.isSpecialization();
            }
        }

        return id < otherId;
    }
};
template <> struct Compare<QualifiedNameId>
{
    bool operator()(const QualifiedNameId &name, const QualifiedNameId &otherName) const
    {
        if (name.base() == otherName.base())
            return name.name() < otherName.name();

        return name.base() < otherName.base();
    }
};

template <> struct Compare<TaggedNameId>
{
    bool operator()(const TaggedNameId &name, const TaggedNameId &otherName) const
    {
        if (name.name() == otherName.name())
            return name.tag() < otherName.tag();
        return name.name() < otherName.name();
    }
};

template <> struct Compare<SelectorNameId>
{
    bool operator()(const SelectorNameId &name, const SelectorNameId &otherName) const
    {
        if (name.hasArguments() == otherName.hasArguments())
            return std::lexicographical_compare(name.firstName(), name.lastName(),
                                                otherName.firstName(), otherName.lastName());

        return name.hasArguments() < otherName.hasArguments();
    }
};


template <typename T>
class Table: public std::set<T, Compare<T> >
{
    typedef std::set<T, Compare<T> > _Base;
public:
    T *intern(const T &element)
    { return const_cast<T *>(&*_Base::insert(element).first); }
};

} // end of anonymous namespace

template <typename Iterator>
static void delete_array_entries(Iterator first, Iterator last)
{
    for (; first != last; ++first)
        delete *first;
}

template <typename Array>
static void delete_array_entries(const Array &a)
{ delete_array_entries(a.begin(), a.end()); }

class Control::Data
{
public:
    Data(Control *control)
        : control(control)
        , translationUnit(0)
        , diagnosticCollector(0)
        , deprecatedId(0)
        , unavailableId(0)
        , cpp11Override(0)
        , cpp11Final(0)
        , attrAvailability(0)
        , processor(0)
    {}

    ~Data()
    {
        // symbols
        delete_array_entries(symbols);
    }

    const EmptyName *findOrInsertEmptyName()
    {
        static EmptyName name;
        return &name;
    }

    template <typename Iterator>
    const TemplateNameId *findOrInsertTemplateNameId(const Identifier* id, bool isSpecialization,
                                                     Iterator first, Iterator last)
    {
        return templateNameIds.intern(TemplateNameId(id, isSpecialization, first, last));
    }

    const DestructorNameId *findOrInsertDestructorNameId(const Name* name)
    {
        return destructorNameIds.intern(DestructorNameId(name));
    }

    const TaggedNameId *findOrInsertTaggedNameId(TaggedNameId::Tag tag, const Name* name)
    {
        return taggedNameIds.intern(TaggedNameId(tag, name));
    }

    const OperatorNameId *findOrInsertOperatorNameId(OperatorNameId::Kind kind)
    {
        return operatorNameIds.intern(OperatorNameId(kind));
    }

    const ConversionNameId *findOrInsertConversionNameId(const FullySpecifiedType& type)
    {
        return conversionNameIds.intern(ConversionNameId(type));
    }

    const QualifiedNameId *findOrInsertQualifiedNameId(const Name* base, const Name* name)
    {
        return qualifiedNameIds.intern(QualifiedNameId(base, name));
    }

    template <typename Iterator>
    const SelectorNameId *findOrInsertSelectorNameId(Iterator first, Iterator last, bool hasArguments)
    {
        return selectorNameIds.intern(SelectorNameId(first, last, hasArguments));
    }

    IntegerType* findOrInsertIntegerType(IntegerType::Kind kind)
    {
        return integerTypes.intern(IntegerType(kind));
    }

    FloatType* findOrInsertFloatType(FloatType::Kind kind)
    {
        return floatTypes.intern(FloatType(kind));
    }

    PointerToMemberType* findOrInsertPointerToMemberType(const Name* memberName, const FullySpecifiedType& elementType)
    {
        return pointerToMemberTypes.intern(PointerToMemberType(memberName, elementType));
    }

    PointerType* findOrInsertPointerType(const FullySpecifiedType& elementType)
    {
        return pointerTypes.intern(PointerType(elementType));
    }

    ReferenceType* findOrInsertReferenceType(const FullySpecifiedType& elementType)
    {
        return referenceTypes.intern(ReferenceType(elementType));
    }

    ArrayType* findOrInsertArrayType(const FullySpecifiedType& elementType, unsigned size)
    {
        return arrayTypes.intern(ArrayType(elementType, size));
    }

    NamedType* findOrInsertNamedType(const Name* name)
    {
        return namedTypes.intern(NamedType(name));
    }

    QuantifiedType* findOrInsertQuantifiedType(const Name* name, QuantifiedType::Kind kind, int label)
    {
        return quantifiedTypes.intern(QuantifiedType(name, kind, label));
    }

    Declaration *newDeclaration(unsigned sourceLocation, const Name* name)
    {
        Declaration *declaration = new Declaration(translationUnit, sourceLocation, name);
        symbols.push_back(declaration);
        return declaration;
    }

    EnumeratorDeclaration *newEnumeratorDeclaration(unsigned sourceLocation, const Name* name)
    {
        EnumeratorDeclaration *decl = new EnumeratorDeclaration(translationUnit, sourceLocation, name);
        symbols.push_back(decl);
        return decl;
    }

    Argument *newArgument(unsigned sourceLocation, const Name* name)
    {
        Argument *argument = new Argument(translationUnit, sourceLocation, name);
        symbols.push_back(argument);
        return argument;
    }

    TypenameArgument *newTypenameArgument(unsigned sourceLocation, const Name* name)
    {
        TypenameArgument *argument = new TypenameArgument(translationUnit, sourceLocation, name);
        symbols.push_back(argument);
        return argument;
    }

    Function *newFunction(unsigned sourceLocation, const Name* name)
    {
        Function *function = new Function(translationUnit, sourceLocation, name);
        symbols.push_back(function);
        return function;
    }

    BaseClass *newBaseClass(unsigned sourceLocation, const Name* name)
    {
        BaseClass *baseClass = new BaseClass(translationUnit, sourceLocation, name);
        symbols.push_back(baseClass);
        return baseClass;
    }

    Block *newBlock(unsigned sourceLocation)
    {
        Block *block = new Block(translationUnit, sourceLocation);
        symbols.push_back(block);
        return block;
    }

    Class *newClass(unsigned sourceLocation, const Name* name)
    {
        Class *klass = new Class(translationUnit, sourceLocation, name);
        symbols.push_back(klass);
        return klass;
    }

    Namespace *newNamespace(unsigned sourceLocation, const Name* name)
    {
        Namespace *ns = new Namespace(translationUnit, sourceLocation, name);
        symbols.push_back(ns);
        return ns;
    }

    Template *newTemplate(unsigned sourceLocation, const Name* name)
    {
        Template *ns = new Template(translationUnit, sourceLocation, name);
        symbols.push_back(ns);
        return ns;
    }

    NamespaceAlias *newNamespaceAlias(unsigned sourceLocation, const Name* name)
    {
        NamespaceAlias *ns = new NamespaceAlias(translationUnit, sourceLocation, name);
        symbols.push_back(ns);
        return ns;
    }

    UsingNamespaceDirective *newUsingNamespaceDirective(unsigned sourceLocation, const Name* name)
    {
        UsingNamespaceDirective *u = new UsingNamespaceDirective(translationUnit, sourceLocation, name);
        symbols.push_back(u);
        return u;
    }

    ForwardClassDeclaration *newForwardClassDeclaration(unsigned sourceLocation, const Name* name)
    {
        ForwardClassDeclaration *c = new ForwardClassDeclaration(translationUnit, sourceLocation, name);
        symbols.push_back(c);
        return c;
    }

    Enum *newEnum(unsigned sourceLocation, const Name* name)
    {
        Enum *e = new Enum(translationUnit, sourceLocation, name);
        symbols.push_back(e);
        return e;
    }

    UsingDeclaration *newUsingDeclaration(unsigned sourceLocation, const Name* name)
    {
        UsingDeclaration *u = new UsingDeclaration(translationUnit, sourceLocation, name);
        symbols.push_back(u);
        return u;
    }

    Control *control;
    TranslationUnit *translationUnit;
    DiagnosticCollector *diagnosticCollector;

    LiteralTable<Identifier> identifiers;
    LiteralTable<StringLiteral> stringLiterals;
    LiteralTable<NumericLiteral> numericLiterals;

    // ### replace std::map with lookup tables. ASAP!

    // names
    Table<DestructorNameId> destructorNameIds;
    Table<OperatorNameId> operatorNameIds;
    Table<ConversionNameId> conversionNameIds;
    Table<TemplateNameId> templateNameIds;
    Table<QualifiedNameId> qualifiedNameIds;
    Table<SelectorNameId> selectorNameIds;
    Table<TaggedNameId> taggedNameIds;

    // types
    VoidType voidType;
    Table<IntegerType> integerTypes;
    Table<FloatType> floatTypes;
    Table<PointerToMemberType> pointerToMemberTypes;
    Table<PointerType> pointerTypes;
    Table<ReferenceType> referenceTypes;
    Table<ArrayType> arrayTypes;
    Table<NamedType> namedTypes;
    Table<QuantifiedType> quantifiedTypes;

    // symbols
    std::vector<Symbol* > symbols;
    std::unordered_set<const Symbol*> annuledSymbols;

    const Identifier* deprecatedId;
    const Identifier* unavailableId;

    // C++ contextual
    const Identifier* cpp11Override;
    const Identifier* cpp11Final;

    // Distinguished attributes
    const Identifier* attrAvailability;

    TopLevelDeclarationProcessor *processor;
};

Control::Control()
    : d(new Data(this))
{
    populateBuiltins();
}

Control::~Control()
{ delete d; }

void Control::reset()
{
    delete d;
    d = new Data(this);
    populateBuiltins();
}

void Control::populateBuiltins()
{
    d->deprecatedId = identifier("deprecated");
    d->unavailableId = identifier("unavailable");

    d->cpp11Override = identifier("override");
    d->cpp11Final = identifier("final");

    d->attrAvailability = identifier("availability");
}

TranslationUnit *Control::translationUnit() const
{ return d->translationUnit; }

TranslationUnit *Control::switchTranslationUnit(TranslationUnit *unit)
{
    TranslationUnit *previousTranslationUnit = d->translationUnit;
    d->translationUnit = unit;
    return previousTranslationUnit;
}

DiagnosticCollector *Control::diagnosticCollector() const
{ return d->diagnosticCollector; }

void Control::setDiagnosticCollector(DiagnosticCollector* collector)
{ d->diagnosticCollector = collector; }

const EmptyName *Control::emptyName()
{ return d->findOrInsertEmptyName(); }

const OperatorNameId *Control::findOperatorNameId(OperatorNameId::Kind operatorId) const
{
    Table<OperatorNameId>::const_iterator i = d->operatorNameIds.find(operatorId);
    if (i == d->operatorNameIds.end())
        return 0;
    else
        return &*i;
}

const Identifier* Control::findIdentifier(const char *chars, unsigned size) const
{ return d->identifiers.findLiteral(chars, size); }

const Identifier* Control::identifier(const char *chars, unsigned size)
{ return d->identifiers.findOrInsertLiteral(chars, size); }

const Identifier* Control::identifier(const char *chars)
{
    const unsigned length = unsigned(std::strlen(chars));
    return identifier(chars, length);
}

Control::IdentifierIterator Control::firstIdentifier() const
{ return d->identifiers.begin(); }

Control::IdentifierIterator Control::lastIdentifier() const
{ return d->identifiers.end(); }

Control::StringLiteralIterator Control::firstStringLiteral() const
{ return d->stringLiterals.begin(); }

Control::StringLiteralIterator Control::lastStringLiteral() const
{ return d->stringLiterals.end(); }

Control::NumericLiteralIterator Control::firstNumericLiteral() const
{ return d->numericLiterals.begin(); }

Control::NumericLiteralIterator Control::lastNumericLiteral() const
{ return d->numericLiterals.end(); }

const StringLiteral* Control::stringLiteral(const char *chars, unsigned size)
{ return d->stringLiterals.findOrInsertLiteral(chars, size); }

const StringLiteral* Control::stringLiteral(const char *chars)
{
    const unsigned length = unsigned(std::strlen(chars));
    return stringLiteral(chars, length);
}

const NumericLiteral* Control::numericLiteral(const char *chars, unsigned size)
{ return d->numericLiterals.findOrInsertLiteral(chars, size); }

const NumericLiteral* Control::numericLiteral(const char *chars)
{
    const unsigned length = unsigned(std::strlen(chars));
    return numericLiteral(chars, length);
}

const TemplateNameId *Control::templateNameId(const Identifier* id,
                                              bool isSpecialization,
                                              const FullySpecifiedType* const args,
                                              unsigned argv)
{
    return d->findOrInsertTemplateNameId(id, isSpecialization, args, args + argv);
}

const DestructorNameId *Control::destructorNameId(const Name* name)
{ return d->findOrInsertDestructorNameId(name); }

const TaggedNameId *Control::taggedNameId(TaggedNameId::Tag tag, const Name* name)
{
  return d->findOrInsertTaggedNameId(tag, name);
}

const OperatorNameId *Control::operatorNameId(OperatorNameId::Kind kind)
{ return d->findOrInsertOperatorNameId(kind); }

const ConversionNameId *Control::conversionNameId(const FullySpecifiedType& type)
{ return d->findOrInsertConversionNameId(type); }

const QualifiedNameId *Control::qualifiedNameId(const Name* base, const Name* name)
{
    return d->findOrInsertQualifiedNameId(base, name);
}

const SelectorNameId *Control::selectorNameId(const Name* const *names,
                                              unsigned nameCount,
                                              bool hasArguments)
{
    return d->findOrInsertSelectorNameId(names, names + nameCount, hasArguments);
}


VoidType* Control::voidType()
{ return &d->voidType; }

IntegerType* Control::integerType(IntegerType::Kind kind)
{ return d->findOrInsertIntegerType(kind); }

FloatType* Control::floatType(FloatType::Kind kind)
{ return d->findOrInsertFloatType(kind); }

PointerToMemberType* Control::pointerToMemberType(const Name* memberName, const FullySpecifiedType& elementType)
{ return d->findOrInsertPointerToMemberType(memberName, elementType); }

PointerType* Control::pointerType(const FullySpecifiedType& elementType)
{ return d->findOrInsertPointerType(elementType); }

ReferenceType* Control::referenceType(const FullySpecifiedType& elementType)
{ return d->findOrInsertReferenceType(elementType); }

ArrayType* Control::arrayType(const FullySpecifiedType& elementType, unsigned size)
{ return d->findOrInsertArrayType(elementType, size); }

NamedType* Control::namedType(const Name* name)
{ return d->findOrInsertNamedType(name); }

QuantifiedType* Control::quantifiedType(const Name* name, QuantifiedType::Kind kind, int label)
{ return d->findOrInsertQuantifiedType(name, kind, label); }

Argument *Control::newArgument(unsigned sourceLocation, const Name* name)
{ return d->newArgument(sourceLocation, name); }

TypenameArgument *Control::newTypenameArgument(unsigned sourceLocation, const Name* name)
{ return d->newTypenameArgument(sourceLocation, name); }

Function *Control::newFunction(unsigned sourceLocation, const Name* name)
{ return d->newFunction(sourceLocation, name); }

Namespace *Control::newNamespace(unsigned sourceLocation, const Name* name)
{ return d->newNamespace(sourceLocation, name); }

Template *Control::newTemplate(unsigned sourceLocation, const Name* name)
{ return d->newTemplate(sourceLocation, name); }

NamespaceAlias *Control::newNamespaceAlias(unsigned sourceLocation, const Name* name)
{ return d->newNamespaceAlias(sourceLocation, name); }

BaseClass *Control::newBaseClass(unsigned sourceLocation, const Name* name)
{ return d->newBaseClass(sourceLocation, name); }

Class *Control::newClass(unsigned sourceLocation, const Name* name)
{ return d->newClass(sourceLocation, name); }

Enum *Control::newEnum(unsigned sourceLocation, const Name* name)
{ return d->newEnum(sourceLocation, name); }

Block *Control::newBlock(unsigned sourceLocation)
{ return d->newBlock(sourceLocation); }

Declaration *Control::newDeclaration(unsigned sourceLocation, const Name* name)
{ return d->newDeclaration(sourceLocation, name); }

EnumeratorDeclaration *Control::newEnumeratorDeclaration(unsigned sourceLocation, const Name* name)
{ return d->newEnumeratorDeclaration(sourceLocation, name); }

UsingNamespaceDirective *Control::newUsingNamespaceDirective(unsigned sourceLocation,
                                                                const Name* name)
{ return d->newUsingNamespaceDirective(sourceLocation, name); }

UsingDeclaration *Control::newUsingDeclaration(unsigned sourceLocation, const Name* name)
{ return d->newUsingDeclaration(sourceLocation, name); }

ForwardClassDeclaration *Control::newForwardClassDeclaration(unsigned sourceLocation,
                                                             const Name* name)
{ return d->newForwardClassDeclaration(sourceLocation, name); }

void Control::annulSymbol(const Symbol* sym)
{ d->annuledSymbols.insert(sym); }

bool Control::isSymbolAnulled(const Symbol* sym)
{ return d->annuledSymbols.count(sym); }

const Identifier* Control::deprecatedId() const
{ return d->deprecatedId; }

const Identifier* Control::unavailableId() const
{ return d->unavailableId; }

const Identifier* Control::cpp11Override() const
{ return d->cpp11Override; }

const Identifier* Control::cpp11Final() const
{ return d->cpp11Final; }

const Identifier* Control::attrAvailability() const
{ return d->attrAvailability; }

Symbol* *Control::firstSymbol() const
{
    if (d->symbols.empty())
        return 0;

    return &*d->symbols.begin();
}

Symbol* *Control::lastSymbol() const
{
    if (d->symbols.empty())
        return 0;

    return &*d->symbols.begin() + d->symbols.size();
}

unsigned Control::symbolCount() const
{
    return unsigned(d->symbols.size());
}

bool Control::hasSymbol(Symbol* symbol) const
{
    return std::find(d->symbols.begin(), d->symbols.end(), symbol) != d->symbols.end();
}

void Control::squeeze()
{
    d->numericLiterals.reset();
}

TopLevelDeclarationProcessor *Control::topLevelDeclarationProcessor() const
{
    return d->processor;
}

void Control::setTopLevelDeclarationProcessor(TopLevelDeclarationProcessor *processor)
{
    d->processor = processor;
}

void Control::addSymbol(Symbol* symbol)
{
    d->symbols.push_back(symbol);
}
