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

#include "Symbols.h"

#include "Names.h"
#include "TypeVisitor.h"
#include "SymbolVisitor.h"
#include "Scope.h"
#include "Templates.h"

using namespace psyche;

UsingNamespaceDirective::UsingNamespaceDirective(TranslationUnit *translationUnit,
                                                 unsigned sourceLocation, const Name* name)
    : Symbol(translationUnit, sourceLocation, name)
{ }

UsingNamespaceDirective::UsingNamespaceDirective(Clone *clone, Subst *subst, UsingNamespaceDirective *original)
    : Symbol(clone, subst, original)
{ }

UsingNamespaceDirective::~UsingNamespaceDirective()
{ }

FullySpecifiedType UsingNamespaceDirective::type() const
{ return FullySpecifiedType(); }

void UsingNamespaceDirective::visitSymbol0(SymbolVisitor* visitor)
{ visitor->visit(this); }

NamespaceAlias::NamespaceAlias(TranslationUnit *translationUnit,
                               unsigned sourceLocation, const Name* name)
    : Symbol(translationUnit, sourceLocation, name), _namespaceName(0)
{ }

NamespaceAlias::NamespaceAlias(Clone *clone, Subst *subst, NamespaceAlias *original)
    : Symbol(clone, subst, original)
    , _namespaceName(clone->name(original->_namespaceName, subst))
{ }

NamespaceAlias::~NamespaceAlias()
{ }

const Name* NamespaceAlias::namespaceName() const
{ return _namespaceName; }

void NamespaceAlias::setNamespaceName(const Name* namespaceName)
{ _namespaceName = namespaceName; }

FullySpecifiedType NamespaceAlias::type() const
{ return FullySpecifiedType(); }

void NamespaceAlias::visitSymbol0(SymbolVisitor* visitor)
{ visitor->visit(this); }


UsingDeclaration::UsingDeclaration(TranslationUnit *translationUnit,
                                   unsigned sourceLocation, const Name* name)
    : Symbol(translationUnit, sourceLocation, name)
{ }

UsingDeclaration::UsingDeclaration(Clone *clone, Subst *subst, UsingDeclaration *original)
    : Symbol(clone, subst, original)
{ }

UsingDeclaration::~UsingDeclaration()
{ }

FullySpecifiedType UsingDeclaration::type() const
{ return FullySpecifiedType(); }

void UsingDeclaration::visitSymbol0(SymbolVisitor* visitor)
{ visitor->visit(this); }

Declaration::Declaration(TranslationUnit *translationUnit, unsigned sourceLocation, const Name* name)
    : Symbol(translationUnit, sourceLocation, name)
    , _initializer(0)
{ }

Declaration::Declaration(Clone *clone, Subst *subst, Declaration *original)
    : Symbol(clone, subst, original)
    , _type(clone->type(original->_type, subst))
    , _initializer(clone->stringLiteral(original->_initializer))
{ }

Declaration::~Declaration()
{ }

void Declaration::setType(const FullySpecifiedType &type)
{ _type = type; }

void Declaration::setInitializer(const StringLiteral* initializer)
{
    _initializer = initializer;
}

FullySpecifiedType Declaration::type() const
{ return _type; }

const StringLiteral* Declaration::getInitializer() const
{
    return _initializer;
}

void Declaration::visitSymbol0(SymbolVisitor* visitor)
{ visitor->visit(this); }

EnumeratorDeclaration::EnumeratorDeclaration(TranslationUnit *translationUnit, unsigned sourceLocation, const Name* name)
    : Declaration(translationUnit, sourceLocation, name)
    , _constantValue(0)
{}

EnumeratorDeclaration::~EnumeratorDeclaration()
{}

const StringLiteral* EnumeratorDeclaration::constantValue() const
{ return _constantValue; }

void EnumeratorDeclaration::setConstantValue(const StringLiteral* constantValue)
{ _constantValue = constantValue; }

Argument::Argument(TranslationUnit *translationUnit, unsigned sourceLocation, const Name* name)
    : Symbol(translationUnit, sourceLocation, name),
      _initializer(0)
{ }

Argument::Argument(Clone *clone, Subst *subst, Argument *original)
    : Symbol(clone, subst, original)
    , _initializer(clone->stringLiteral(original->_initializer))
    , _type(clone->type(original->_type, subst))
{ }

Argument::~Argument()
{ }

bool Argument::hasInitializer() const
{ return _initializer != 0; }

const StringLiteral* Argument::initializer() const
{ return _initializer; }

void Argument::setInitializer(const StringLiteral* initializer)
{ _initializer = initializer; }

void Argument::setType(const FullySpecifiedType &type)
{ _type = type; }

FullySpecifiedType Argument::type() const
{ return _type; }

void Argument::visitSymbol0(SymbolVisitor* visitor)
{ visitor->visit(this); }

TypenameArgument::TypenameArgument(TranslationUnit *translationUnit, unsigned sourceLocation, const Name* name)
    : Symbol(translationUnit, sourceLocation, name)
{ }

TypenameArgument::TypenameArgument(Clone *clone, Subst *subst, TypenameArgument *original)
    : Symbol(clone, subst, original)
    , _type(clone->type(original->_type, subst))
{ }

TypenameArgument::~TypenameArgument()
{ }

void TypenameArgument::setType(const FullySpecifiedType &type)
{ _type = type; }

FullySpecifiedType TypenameArgument::type() const
{ return _type; }

void TypenameArgument::visitSymbol0(SymbolVisitor* visitor)
{ visitor->visit(this); }

Function::Function(TranslationUnit *translationUnit, unsigned sourceLocation, const Name* name)
    : Scope(translationUnit, sourceLocation, name),
      _flags(0)
{ }

Function::Function(Clone *clone, Subst *subst, Function *original)
    : Scope(clone, subst, original)
    , _returnType(clone->type(original->_returnType, subst))
    , _flags(original->_flags)
{ }

Function::~Function()
{ }

bool Function::isNormal() const
{ return f._methodKey == NormalMethod; }

bool Function::isSignal() const
{ return f._methodKey == SignalMethod; }

bool Function::isSlot() const
{ return f._methodKey == SlotMethod; }

bool Function::isInvokable() const
{ return f._methodKey == InvokableMethod; }

int Function::methodKey() const
{ return f._methodKey; }

void Function::setMethodKey(int key)
{ f._methodKey = key; }

void Function::accept0(TypeVisitor* visitor)
{ visitor->visit(this); }

FullySpecifiedType Function::type() const
{
    FullySpecifiedType ty(const_cast<Function *>(this));
    ty.setConst(isConst());
    ty.setVolatile(isVolatile());
    return ty;
}

FullySpecifiedType Function::returnType() const
{ return _returnType; }

void Function::setReturnType(const FullySpecifiedType &returnType)
{ _returnType = returnType; }

bool Function::hasReturnType() const
{
    const FullySpecifiedType ty = returnType();
    return ty.isValid() || ty.isSigned() || ty.isUnsigned();
}

unsigned Function::argumentCount() const
{
    const unsigned memCnt = memberCount();
    if (memCnt > 0 && memberAt(0)->type()->isVoidType())
        return 0;

    // Definitions with function-try-blocks will have more than a block, and
    // arguments with a lambda as default argument will also have more blocks.
    unsigned argc = 0;
    for (unsigned it = 0; it < memCnt; ++it)
        if (memberAt(it)->isArgument())
            ++argc;
    return argc;
}

Symbol* Function::argumentAt(unsigned index) const
{
    for (unsigned it = 0, eit = memberCount(); it < eit; ++it) {
        if (Argument *arg = memberAt(it)->asArgument()) {
            if (index == 0)
                return arg;
            else
                --index;
        }
    }

    return 0;
}

bool Function::hasArguments() const
{
    unsigned argc = argumentCount();
    return ! (argc == 0 || (argc == 1 && argumentAt(0)->type()->isVoidType()));
}

unsigned Function::minimumArgumentCount() const
{
    unsigned index = 0;

    for (unsigned ei = argumentCount(); index < ei; ++index) {
        if (Argument *arg = argumentAt(index)->asArgument()) {
            if (arg->hasInitializer())
                break;
        }
    }

    return index;
}

bool Function::isVirtual() const
{ return f._isVirtual; }

void Function::setVirtual(bool isVirtual)
{ f._isVirtual = isVirtual; }

bool Function::isOverride() const
{ return f._isOverride; }

void Function::setOverride(bool isOverride)
{ f._isOverride = isOverride; }

bool Function::isFinal() const
{ return f._isFinal; }

void Function::setFinal(bool isFinal)
{ f._isFinal = isFinal; }

bool Function::isVariadic() const
{ return f._isVariadic; }

void Function::setVariadic(bool isVariadic)
{ f._isVariadic = isVariadic; }

bool Function::isConst() const
{ return f._isConst; }

void Function::setConst(bool isConst)
{ f._isConst = isConst; }

bool Function::isVolatile() const
{ return f._isVolatile; }

void Function::setVolatile(bool isVolatile)
{ f._isVolatile = isVolatile; }

bool Function::isPureVirtual() const
{ return f._isPureVirtual; }

void Function::setPureVirtual(bool isPureVirtual)
{ f._isPureVirtual = isPureVirtual; }

bool Function::isAmbiguous() const
{ return f._isAmbiguous; }

void Function::setAmbiguous(bool isAmbiguous)
{ f._isAmbiguous = isAmbiguous; }

bool Function::isGeneric() const
{ return f._isGeneric; }

void Function::setIsGeneric(bool isGeneric)
{ f._isGeneric = isGeneric; }

void Function::visitSymbol0(SymbolVisitor* visitor)
{
    if (visitor->visit(this)) {
        for (unsigned i = 0; i < memberCount(); ++i) {
            visitSymbol(memberAt(i), visitor);
        }
    }
}

bool Function::maybeValidPrototype(unsigned actualArgumentCount) const
{
    const unsigned argc = argumentCount();
    unsigned minNumberArguments = 0;

    for (; minNumberArguments < argc; ++minNumberArguments) {
        Argument *arg = argumentAt(minNumberArguments)->asArgument();

        if (! arg)
            return false;

        if (arg->hasInitializer())
            break;
    }

    if (actualArgumentCount < minNumberArguments) {
        // not enough arguments.
        return false;

    } else if (!isVariadic() && actualArgumentCount > argc) {
        // too many arguments.
        return false;
    }

    return true;
}


Block::Block(TranslationUnit *translationUnit, unsigned sourceLocation)
    : Scope(translationUnit, sourceLocation, /*name = */ 0)
{ }

Block::Block(Clone *clone, Subst *subst, Block *original)
    : Scope(clone, subst, original)
{ }

Block::~Block()
{ }

FullySpecifiedType Block::type() const
{ return FullySpecifiedType(); }

void Block::visitSymbol0(SymbolVisitor* visitor)
{
    if (visitor->visit(this)) {
        for (unsigned i = 0; i < memberCount(); ++i) {
            visitSymbol(memberAt(i), visitor);
        }
    }
}

Enum::Enum(TranslationUnit *translationUnit, unsigned sourceLocation, const Name* name)
    : Scope(translationUnit, sourceLocation, name)
    , _isScoped(false)
{ }

Enum::Enum(Clone *clone, Subst *subst, Enum *original)
    : Scope(clone, subst, original)
    , _isScoped(original->isScoped())
{ }

Enum::~Enum()
{ }

FullySpecifiedType Enum::type() const
{ return FullySpecifiedType(const_cast<Enum *>(this)); }

bool Enum::isScoped() const
{
    return _isScoped;
}

void Enum::setScoped(bool scoped)
{
    _isScoped = scoped;
}

void Enum::accept0(TypeVisitor* visitor)
{ visitor->visit(this); }

void Enum::visitSymbol0(SymbolVisitor* visitor)
{
    if (visitor->visit(this)) {
        for (unsigned i = 0; i < memberCount(); ++i) {
            visitSymbol(memberAt(i), visitor);
        }
    }
}

Template::Template(TranslationUnit *translationUnit, unsigned sourceLocation, const Name* name)
    : Scope(translationUnit, sourceLocation, name)
{ }

Template::Template(Clone *clone, Subst *subst, Template *original)
    : Scope(clone, subst, original)
{ }

Template::~Template()
{ }

unsigned Template::templateParameterCount() const
{
    if (declaration() != 0)
        return memberCount() - 1;

    return 0;
}

Symbol* Template::templateParameterAt(unsigned index) const
{ return memberAt(index); }

Symbol* Template::declaration() const
{
    if (isEmpty())
        return 0;

    if (Symbol* s = memberAt(memberCount() - 1)) {
        if (s->isClass() || s->isForwardClassDeclaration() ||
            s->isTemplate() || s->isFunction() || s->isDeclaration())
            return s;
    }

    return 0;
}

FullySpecifiedType Template::type() const
{ return FullySpecifiedType(const_cast<Template *>(this)); }

void Template::visitSymbol0(SymbolVisitor* visitor)
{
    if (visitor->visit(this)) {
        for (unsigned i = 0; i < memberCount(); ++i) {
            visitSymbol(memberAt(i), visitor);
        }
    }
}

void Template::accept0(TypeVisitor* visitor)
{ visitor->visit(this); }

Namespace::Namespace(TranslationUnit *translationUnit, unsigned sourceLocation, const Name* name)
    : Scope(translationUnit, sourceLocation, name)
    , _isInline(false)
{ }

Namespace::Namespace(Clone *clone, Subst *subst, Namespace *original)
    : Scope(clone, subst, original)
    , _isInline(original->_isInline)
{ }

Namespace::~Namespace()
{ }

void Namespace::accept0(TypeVisitor* visitor)
{ visitor->visit(this); }

void Namespace::visitSymbol0(SymbolVisitor* visitor)
{
    if (visitor->visit(this)) {
        for (unsigned i = 0; i < memberCount(); ++i) {
            visitSymbol(memberAt(i), visitor);
        }
    }
}

FullySpecifiedType Namespace::type() const
{ return FullySpecifiedType(const_cast<Namespace *>(this)); }

BaseClass::BaseClass(TranslationUnit *translationUnit, unsigned sourceLocation, const Name* name)
    : Symbol(translationUnit, sourceLocation, name),
      _isVirtual(false)
{ }

BaseClass::BaseClass(Clone *clone, Subst *subst, BaseClass *original)
    : Symbol(clone, subst, original)
    , _isVirtual(original->_isVirtual)
    , _type(clone->type(original->_type, subst))
{ }

BaseClass::~BaseClass()
{ }

FullySpecifiedType BaseClass::type() const
{ return _type; }

void BaseClass::setType(const FullySpecifiedType &type)
{ _type = type; }

bool BaseClass::isVirtual() const
{ return _isVirtual; }

void BaseClass::setVirtual(bool isVirtual)
{ _isVirtual = isVirtual; }

bool BaseClass::isVariadic() const
{ return _isVariadic; }

void BaseClass::setVariadic(bool isVariadic)
{ _isVariadic = isVariadic; }

void BaseClass::visitSymbol0(SymbolVisitor* visitor)
{ visitor->visit(this); }

ForwardClassDeclaration::ForwardClassDeclaration(TranslationUnit *translationUnit,
                                                 unsigned sourceLocation, const Name* name)
    : Symbol(translationUnit, sourceLocation, name)
{ }

ForwardClassDeclaration::ForwardClassDeclaration(Clone *clone, Subst *subst, ForwardClassDeclaration *original)
    : Symbol(clone, subst, original)
{ }

ForwardClassDeclaration::~ForwardClassDeclaration()
{ }

FullySpecifiedType ForwardClassDeclaration::type() const
{ return FullySpecifiedType(const_cast<ForwardClassDeclaration *>(this)); }

void ForwardClassDeclaration::visitSymbol0(SymbolVisitor* visitor)
{ visitor->visit(this); }

void ForwardClassDeclaration::accept0(TypeVisitor* visitor)
{ visitor->visit(this); }

Class::Class(TranslationUnit *translationUnit, unsigned sourceLocation, const Name* name)
    : Scope(translationUnit, sourceLocation, name),
      _key(ClassKey)
{ }

Class::Class(Clone *clone, Subst *subst, Class *original)
    : Scope(clone, subst, original)
    , _key(original->_key)
{
    for (size_t i = 0; i < original->_baseClasses.size(); ++i)
        addBaseClass(clone->symbol(original->_baseClasses.at(i), subst)->asBaseClass());
}

Class::~Class()
{ }

bool Class::isClass() const
{ return _key == ClassKey; }

bool Class::isStruct() const
{ return _key == StructKey; }

bool Class::isUnion() const
{ return _key == UnionKey; }

Class::Key Class::classKey() const
{ return _key; }

void Class::setClassKey(Key key)
{ _key = key; }

void Class::accept0(TypeVisitor* visitor)
{ visitor->visit(this); }

unsigned Class::baseClassCount() const
{ return unsigned(_baseClasses.size()); }

BaseClass *Class::baseClassAt(unsigned index) const
{ return _baseClasses.at(index); }

void Class::addBaseClass(BaseClass *baseClass)
{ _baseClasses.push_back(baseClass); }

FullySpecifiedType Class::type() const
{ return FullySpecifiedType(const_cast<Class *>(this)); }

void Class::visitSymbol0(SymbolVisitor* visitor)
{
    if (visitor->visit(this)) {
        for (unsigned i = 0; i < _baseClasses.size(); ++i) {
            visitSymbol(_baseClasses.at(i), visitor);
        }
        for (unsigned i = 0; i < memberCount(); ++i) {
            visitSymbol(memberAt(i), visitor);
        }
    }
}
