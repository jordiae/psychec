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

#ifndef PSYCHE_SYMBOL_VISITOR_H__
#define PSYCHE_SYMBOL_VISITOR_H__

#include "Symbol.h"

namespace psyche {

class CFE_API SymbolVisitor
{
    SymbolVisitor(const SymbolVisitor &other);
    void operator =(const SymbolVisitor &other);

public:
    SymbolVisitor() { }
    virtual ~SymbolVisitor() { }

    void accept(Symbol* symbol) { Symbol::visitSymbol(symbol, this); }

    virtual bool preVisit(Symbol* ) { return true; }
    virtual void postVisit(Symbol* ) {}

    virtual bool visit(UsingNamespaceDirective *) { return true; }
    virtual bool visit(UsingDeclaration *) { return true; }
    virtual bool visit(NamespaceAlias *) { return true; }
    virtual bool visit(Declaration *) { return true; }
    virtual bool visit(Argument *) { return true; }
    virtual bool visit(TypenameArgument *) { return true; }
    virtual bool visit(BaseClass *) { return true; }
    virtual bool visit(Enum *) { return true; }
    virtual bool visit(Function *) { return true; }
    virtual bool visit(Namespace *) { return true; }
    virtual bool visit(Template *) { return true; }
    virtual bool visit(Class *) { return true; }
    virtual bool visit(Block *) { return true; }
    virtual bool visit(ForwardClassDeclaration *) { return true; }
};

} // namespace psyche

#endif
