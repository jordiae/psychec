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


#include "AST.h"
#include "ASTMatcher.h"

using namespace psyche;

bool SimpleSpecifierAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (SimpleSpecifierAST* _other = pattern->asSimpleSpecifier())
        return matcher->match(this, _other);

    return false;
}

bool AlignmentSpecifierAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (AlignmentSpecifierAST* _other = pattern->asAlignmentSpecifier())
        return matcher->match(this, _other);

    return false;
}

bool GnuAttributeSpecifierAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (GnuAttributeSpecifierAST* _other = pattern->asGnuAttributeSpecifier())
        return matcher->match(this, _other);

    return false;
}

bool GnuAttributeAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (GnuAttributeAST* _other = pattern->asGnuAttribute())
        return matcher->match(this, _other);

    return false;
}

bool TypeofSpecifierAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (TypeofSpecifierAST* _other = pattern->asTypeofSpecifier())
        return matcher->match(this, _other);

    return false;
}

bool DecltypeSpecifierAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (DecltypeSpecifierAST* _other = pattern->asDecltypeSpecifier())
        return matcher->match(this, _other);

    return false;
}

bool DeclaratorAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (DeclaratorAST* _other = pattern->asDeclarator())
        return matcher->match(this, _other);

    return false;
}

bool SimpleDeclarationAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (SimpleDeclarationAST* _other = pattern->asSimpleDeclaration())
        return matcher->match(this, _other);

    return false;
}

bool EmptyDeclarationAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (EmptyDeclarationAST* _other = pattern->asEmptyDeclaration())
        return matcher->match(this, _other);

    return false;
}

bool AccessDeclarationAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (AccessDeclarationAST* _other = pattern->asAccessDeclaration())
        return matcher->match(this, _other);

    return false;
}

bool AsmDefinitionAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (AsmDefinitionAST* _other = pattern->asAsmDefinition())
        return matcher->match(this, _other);

    return false;
}

bool BaseSpecifierAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (BaseSpecifierAST* _other = pattern->asBaseSpecifier())
        return matcher->match(this, _other);

    return false;
}

bool IdExpressionAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (IdExpressionAST* _other = pattern->asIdExpression())
        return matcher->match(this, _other);

    return false;
}

bool CompoundExpressionAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (CompoundExpressionAST* _other = pattern->asCompoundExpression())
        return matcher->match(this, _other);

    return false;
}

bool CompoundLiteralAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (CompoundLiteralAST* _other = pattern->asCompoundLiteral())
        return matcher->match(this, _other);

    return false;
}

bool BinaryExpressionAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (BinaryExpressionAST* _other = pattern->asBinaryExpression())
        return matcher->match(this, _other);

    return false;
}

bool CastExpressionAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (CastExpressionAST* _other = pattern->asCastExpression())
        return matcher->match(this, _other);

    return false;
}

bool ClassSpecifierAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (ClassSpecifierAST* _other = pattern->asClassSpecifier())
        return matcher->match(this, _other);

    return false;
}

bool CaseStatementAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (CaseStatementAST* _other = pattern->asCaseStatement())
        return matcher->match(this, _other);

    return false;
}

bool CompoundStatementAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (CompoundStatementAST* _other = pattern->asCompoundStatement())
        return matcher->match(this, _other);

    return false;
}

bool ConditionAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (ConditionAST* _other = pattern->asCondition())
        return matcher->match(this, _other);

    return false;
}

bool ConditionalExpressionAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (ConditionalExpressionAST* _other = pattern->asConditionalExpression())
        return matcher->match(this, _other);

    return false;
}

bool CppCastExpressionAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (CppCastExpressionAST* _other = pattern->asCppCastExpression())
        return matcher->match(this, _other);

    return false;
}

bool CtorInitializerAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (CtorInitializerAST* _other = pattern->asCtorInitializer())
        return matcher->match(this, _other);

    return false;
}

bool DeclarationStatementAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (DeclarationStatementAST* _other = pattern->asDeclarationStatement())
        return matcher->match(this, _other);

    return false;
}

bool DeclaratorIdAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (DeclaratorIdAST* _other = pattern->asDeclaratorId())
        return matcher->match(this, _other);

    return false;
}

bool NestedDeclaratorAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (NestedDeclaratorAST* _other = pattern->asNestedDeclarator())
        return matcher->match(this, _other);

    return false;
}

bool BitfieldDeclaratorAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (BitfieldDeclaratorAST* _other = pattern->asBitfieldDeclarator())
        return matcher->match(this, _other);

    return false;
}

bool FunctionDeclaratorAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (FunctionDeclaratorAST* _other = pattern->asFunctionDeclarator())
        return matcher->match(this, _other);

    return false;
}

bool ArrayDeclaratorAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (ArrayDeclaratorAST* _other = pattern->asArrayDeclarator())
        return matcher->match(this, _other);

    return false;
}

bool DeleteExpressionAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (DeleteExpressionAST* _other = pattern->asDeleteExpression())
        return matcher->match(this, _other);

    return false;
}

bool DoStatementAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (DoStatementAST* _other = pattern->asDoStatement())
        return matcher->match(this, _other);

    return false;
}

bool NamedTypeSpecifierAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (NamedTypeSpecifierAST* _other = pattern->asNamedTypeSpecifier())
        return matcher->match(this, _other);

    return false;
}

bool ElaboratedTypeSpecifierAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (ElaboratedTypeSpecifierAST* _other = pattern->asElaboratedTypeSpecifier())
        return matcher->match(this, _other);

    return false;
}

bool EnumSpecifierAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (EnumSpecifierAST* _other = pattern->asEnumSpecifier())
        return matcher->match(this, _other);

    return false;
}

bool EnumeratorAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (EnumeratorAST* _other = pattern->asEnumerator())
        return matcher->match(this, _other);

    return false;
}

bool ExceptionDeclarationAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (ExceptionDeclarationAST* _other = pattern->asExceptionDeclaration())
        return matcher->match(this, _other);

    return false;
}

bool DynamicExceptionSpecificationAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (DynamicExceptionSpecificationAST* _other = pattern->asDynamicExceptionSpecification())
        return matcher->match(this, _other);

    return false;
}

bool NoExceptSpecificationAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (NoExceptSpecificationAST* _other = pattern->asNoExceptSpecification())
        return matcher->match(this, _other);

    return false;
}

bool ExpressionOrDeclarationStatementAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (ExpressionOrDeclarationStatementAST* _other = pattern->asExpressionOrDeclarationStatement())
        return matcher->match(this, _other);

    return false;
}

bool ExpressionStatementAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (ExpressionStatementAST* _other = pattern->asExpressionStatement())
        return matcher->match(this, _other);

    return false;
}

bool AmbiguousStatementAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (AmbiguousStatementAST* _other = pattern->asAmbiguousStatement())
        return matcher->match(this, _other);

    return false;
}

bool FunctionDefinitionAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (FunctionDefinitionAST* _other = pattern->asFunctionDefinition())
        return matcher->match(this, _other);

    return false;
}

bool ForeachStatementAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (ForeachStatementAST* _other = pattern->asForeachStatement())
        return matcher->match(this, _other);

    return false;
}

bool RangeBasedForStatementAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (RangeBasedForStatementAST* _other = pattern->asRangeBasedForStatement())
        return matcher->match(this, _other);

    return false;
}

bool ForStatementAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (ForStatementAST* _other = pattern->asForStatement())
        return matcher->match(this, _other);

    return false;
}

bool IfStatementAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (IfStatementAST* _other = pattern->asIfStatement())
        return matcher->match(this, _other);

    return false;
}

bool ArrayInitializerAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (ArrayInitializerAST* _other = pattern->asArrayInitializer())
        return matcher->match(this, _other);

    return false;
}

bool LabeledStatementAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (LabeledStatementAST* _other = pattern->asLabeledStatement())
        return matcher->match(this, _other);

    return false;
}

bool LinkageBodyAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (LinkageBodyAST* _other = pattern->asLinkageBody())
        return matcher->match(this, _other);

    return false;
}

bool LinkageSpecificationAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (LinkageSpecificationAST* _other = pattern->asLinkageSpecification())
        return matcher->match(this, _other);

    return false;
}

bool MemInitializerAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (MemInitializerAST* _other = pattern->asMemInitializer())
        return matcher->match(this, _other);

    return false;
}

bool NestedNameSpecifierAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (NestedNameSpecifierAST* _other = pattern->asNestedNameSpecifier())
        return matcher->match(this, _other);

    return false;
}

bool QualifiedNameAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (QualifiedNameAST* _other = pattern->asQualifiedName())
        return matcher->match(this, _other);

    return false;
}

bool QuantifiedTypeSpecifierAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (QuantifiedTypeSpecifierAST* _other = pattern->asQuantifiedTypeSpecifier())
        return matcher->match(this, _other);

    return false;
}

bool OperatorFunctionIdAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (OperatorFunctionIdAST* _other = pattern->asOperatorFunctionId())
        return matcher->match(this, _other);

    return false;
}

bool ConversionFunctionIdAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (ConversionFunctionIdAST* _other = pattern->asConversionFunctionId())
        return matcher->match(this, _other);

    return false;
}

bool EmptyNameAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (EmptyNameAST* _other = pattern->asEmptyName())
        return matcher->match(this, _other);

    return false;
}

bool SimpleNameAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (SimpleNameAST* _other = pattern->asSimpleName())
        return matcher->match(this, _other);

    return false;
}

bool TaggedNameAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (TaggedNameAST* _other = pattern->asTaggedName())
        return matcher->match(this, _other);

    return false;
}

bool DestructorNameAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (DestructorNameAST* _other = pattern->asDestructorName())
        return matcher->match(this, _other);

    return false;
}

bool TemplateIdAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (TemplateIdAST* _other = pattern->asTemplateId())
        return matcher->match(this, _other);

    return false;
}

bool NamespaceAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (NamespaceAST* _other = pattern->asNamespace())
        return matcher->match(this, _other);

    return false;
}

bool NamespaceAliasDefinitionAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (NamespaceAliasDefinitionAST* _other = pattern->asNamespaceAliasDefinition())
        return matcher->match(this, _other);

    return false;
}

bool AliasDeclarationAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (AliasDeclarationAST* _other = pattern->asAliasDeclaration())
        return matcher->match(this, _other);

    return false;
}

bool ExpressionListParenAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (ExpressionListParenAST* _other = pattern->asExpressionListParen())
        return matcher->match(this, _other);

    return false;
}

bool NewArrayDeclaratorAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (NewArrayDeclaratorAST* _other = pattern->asNewArrayDeclarator())
        return matcher->match(this, _other);

    return false;
}

bool NewExpressionAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (NewExpressionAST* _other = pattern->asNewExpression())
        return matcher->match(this, _other);

    return false;
}

bool NewTypeIdAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (NewTypeIdAST* _other = pattern->asNewTypeId())
        return matcher->match(this, _other);

    return false;
}

bool OperatorAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (OperatorAST* _other = pattern->asOperator())
        return matcher->match(this, _other);

    return false;
}

bool ParameterDeclarationAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (ParameterDeclarationAST* _other = pattern->asParameterDeclaration())
        return matcher->match(this, _other);

    return false;
}

bool ParameterDeclarationClauseAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (ParameterDeclarationClauseAST* _other = pattern->asParameterDeclarationClause())
        return matcher->match(this, _other);

    return false;
}

bool CallAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (CallAST* _other = pattern->asCall())
        return matcher->match(this, _other);

    return false;
}

bool ArrayAccessAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (ArrayAccessAST* _other = pattern->asArrayAccess())
        return matcher->match(this, _other);

    return false;
}

bool PostIncrDecrAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (PostIncrDecrAST* _other = pattern->asPostIncrDecr())
        return matcher->match(this, _other);

    return false;
}

bool MemberAccessAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (MemberAccessAST* _other = pattern->asMemberAccess())
        return matcher->match(this, _other);

    return false;
}

bool TypeidExpressionAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (TypeidExpressionAST* _other = pattern->asTypeidExpression())
        return matcher->match(this, _other);

    return false;
}

bool TypenameCallExpressionAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (TypenameCallExpressionAST* _other = pattern->asTypenameCallExpression())
        return matcher->match(this, _other);

    return false;
}

bool TypeConstructorCallAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (TypeConstructorCallAST* _other = pattern->asTypeConstructorCall())
        return matcher->match(this, _other);

    return false;
}

bool PointerToMemberAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (PointerToMemberAST* _other = pattern->asPointerToMember())
        return matcher->match(this, _other);

    return false;
}

bool PointerAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (PointerAST* _other = pattern->asPointer())
        return matcher->match(this, _other);

    return false;
}

bool ReferenceAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (ReferenceAST* _other = pattern->asReference())
        return matcher->match(this, _other);

    return false;
}

bool BreakStatementAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (BreakStatementAST* _other = pattern->asBreakStatement())
        return matcher->match(this, _other);

    return false;
}

bool ContinueStatementAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (ContinueStatementAST* _other = pattern->asContinueStatement())
        return matcher->match(this, _other);

    return false;
}

bool GotoStatementAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (GotoStatementAST* _other = pattern->asGotoStatement())
        return matcher->match(this, _other);

    return false;
}

bool ReturnStatementAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (ReturnStatementAST* _other = pattern->asReturnStatement())
        return matcher->match(this, _other);

    return false;
}

bool SizeofExpressionAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (SizeofExpressionAST* _other = pattern->asSizeofExpression())
        return matcher->match(this, _other);

    return false;
}

bool AlignofExpressionAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (AlignofExpressionAST* _other = pattern->asAlignofExpression())
        return matcher->match(this, _other);

    return false;
}

bool PointerLiteralAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (PointerLiteralAST* _other = pattern->asPointerLiteral())
        return matcher->match(this, _other);

    return false;
}

bool NumericLiteralAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (NumericLiteralAST* _other = pattern->asNumericLiteral())
        return matcher->match(this, _other);

    return false;
}

bool BoolLiteralAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (BoolLiteralAST* _other = pattern->asBoolLiteral())
        return matcher->match(this, _other);

    return false;
}

bool ThisExpressionAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (ThisExpressionAST* _other = pattern->asThisExpression())
        return matcher->match(this, _other);

    return false;
}

bool NestedExpressionAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (NestedExpressionAST* _other = pattern->asNestedExpression())
        return matcher->match(this, _other);

    return false;
}

bool StaticAssertDeclarationAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (StaticAssertDeclarationAST* _other = pattern->asStaticAssertDeclaration())
        return matcher->match(this, _other);

    return false;
}

bool StringLiteralAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (StringLiteralAST* _other = pattern->asStringLiteral())
        return matcher->match(this, _other);

    return false;
}

bool SwitchStatementAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (SwitchStatementAST* _other = pattern->asSwitchStatement())
        return matcher->match(this, _other);

    return false;
}

bool GenericsDeclarationAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (GenericsDeclarationAST* _other = pattern->asGenericsDeclaration())
        return matcher->match(this, _other);

    return false;
}

bool TemplateDeclarationAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (TemplateDeclarationAST* _other = pattern->asTemplateDeclaration())
        return matcher->match(this, _other);

    return false;
}

bool ThrowExpressionAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (ThrowExpressionAST* _other = pattern->asThrowExpression())
        return matcher->match(this, _other);

    return false;
}

bool NoExceptOperatorExpressionAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (NoExceptOperatorExpressionAST* _other = pattern->asNoExceptOperatorExpression())
        return matcher->match(this, _other);

    return false;
}

bool TranslationUnitAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (TranslationUnitAST* _other = pattern->asTranslationUnit())
        return matcher->match(this, _other);

    return false;
}

bool TryBlockStatementAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (TryBlockStatementAST* _other = pattern->asTryBlockStatement())
        return matcher->match(this, _other);

    return false;
}

bool CatchClauseAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (CatchClauseAST* _other = pattern->asCatchClause())
        return matcher->match(this, _other);

    return false;
}

bool TypeIdAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (TypeIdAST* _other = pattern->asTypeId())
        return matcher->match(this, _other);

    return false;
}

bool TypenameTypeParameterAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (TypenameTypeParameterAST* _other = pattern->asTypenameTypeParameter())
        return matcher->match(this, _other);

    return false;
}

bool TemplateTypeParameterAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (TemplateTypeParameterAST* _other = pattern->asTemplateTypeParameter())
        return matcher->match(this, _other);

    return false;
}

bool UnaryExpressionAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (UnaryExpressionAST* _other = pattern->asUnaryExpression())
        return matcher->match(this, _other);

    return false;
}

bool UsingAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (UsingAST* _other = pattern->asUsing())
        return matcher->match(this, _other);

    return false;
}

bool UsingDirectiveAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (UsingDirectiveAST* _other = pattern->asUsingDirective())
        return matcher->match(this, _other);

    return false;
}

bool WhileStatementAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (WhileStatementAST* _other = pattern->asWhileStatement())
        return matcher->match(this, _other);

    return false;
}

bool LambdaExpressionAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (LambdaExpressionAST* _other = pattern->asLambdaExpression())
        return matcher->match(this, _other);

    return false;
}

bool LambdaIntroducerAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (LambdaIntroducerAST* _other = pattern->asLambdaIntroducer())
        return matcher->match(this, _other);

    return false;
}

bool LambdaCaptureAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (LambdaCaptureAST* _other = pattern->asLambdaCapture())
        return matcher->match(this, _other);

    return false;
}

bool CaptureAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (CaptureAST* _other = pattern->asCapture())
        return matcher->match(this, _other);

    return false;
}

bool LambdaDeclaratorAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (LambdaDeclaratorAST* _other = pattern->asLambdaDeclarator())
        return matcher->match(this, _other);

    return false;
}

bool TrailingReturnTypeAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (TrailingReturnTypeAST* _other = pattern->asTrailingReturnType())
        return matcher->match(this, _other);

    return false;
}

bool BracedInitializerAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (BracedInitializerAST* _other = pattern->asBracedInitializer())
        return matcher->match(this, _other);

    return false;
}

bool DotDesignatorAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (DotDesignatorAST* _other = pattern->asDotDesignator())
        return matcher->match(this, _other);

    return false;
}

bool BracketDesignatorAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (BracketDesignatorAST* _other = pattern->asBracketDesignator())
        return matcher->match(this, _other);

    return false;
}

bool DesignatedInitializerAST::match0(AST* pattern, ASTMatcher* matcher)
{
    if (DesignatedInitializerAST* _other = pattern->asDesignatedInitializer())
        return matcher->match(this, _other);

    return false;
}

