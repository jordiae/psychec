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

#include "ASTIdentityMatcher.h"

using namespace psyche;

bool ASTIdentityMatcher::match(ArrayAccessAST* node, ArrayAccessAST* pattern)
{
    if (!node || !pattern)
        return false;
    if (!AST::match(node->base_expression, pattern->base_expression, this))
        return false;
    return true;
}

bool ASTIdentityMatcher::match(ArrayInitializerAST* node, ArrayInitializerAST* pattern)
{
    if (!node || !pattern)
        return false;
    if (!AST::match(node->expression_list, pattern->expression_list, this))
        return false;
    return true;
}

bool ASTIdentityMatcher::match(BinaryExpressionAST* node, BinaryExpressionAST* pattern)
{
    if (!node || !pattern)
        return false;
    if (!AST::match(node->left_expression, pattern->left_expression, this))
        return false;
    if (unit_->tokenKind(node->binary_op_token) != unit_->tokenKind(pattern->binary_op_token))
        return false;
    if (!AST::match(node->right_expression, pattern->right_expression, this))
        return false;
    return true;
}

bool ASTIdentityMatcher::match(BoolLiteralAST* node, BoolLiteralAST* pattern)
{
    if (!node || !pattern)
        return false;
    if (unit_->tokenKind(node->literal_token) != unit_->tokenKind(pattern->literal_token))
        return false;
    return true;
}

bool ASTIdentityMatcher::match(BracedInitializerAST* node, BracedInitializerAST* pattern)
{
    if (!node || !pattern)
        return false;
    if (!AST::match(node->expression_list, pattern->expression_list, this))
        return false;
    return true;
}

bool ASTIdentityMatcher::match(BracketDesignatorAST* node, BracketDesignatorAST* pattern)
{
    if (!node || !pattern)
        return false;
    if (!AST::match(node->expression, pattern->expression, this))
        return false;
    return true;
}

bool ASTIdentityMatcher::match(DotDesignatorAST* node, DotDesignatorAST* pattern)
{
    if (!node || !pattern)
        return false;
    if (unit_->spell(node->identifier_token) != unit_->spell(pattern->identifier_token))
        return false;
    return true;
}

bool ASTIdentityMatcher::match(CallAST* node, CallAST* pattern)
{
    if (!node || !pattern)
        return false;
    if (!AST::match(node->base_expression, pattern->base_expression, this))
        return false;
    return true;
}

bool ASTIdentityMatcher::match(CastExpressionAST* node, CastExpressionAST* pattern)
{
    if (!node || !pattern)
        return false;
    if (!AST::match(node->type_id, pattern->type_id, this))
        return false;
    if (!AST::match(node->expression, pattern->expression, this))
        return false;
    return true;
}

bool ASTIdentityMatcher::match(TypeIdAST* node, TypeIdAST* pattern)
{
    if (!node || !pattern)
        return false;
    if (!AST::match(node->type_specifier_list, pattern->type_specifier_list, this))
        return false;
    if (!AST::match(node->declarator, pattern->declarator, this))
        return false;
    return true;
}

bool ASTIdentityMatcher::match(ConditionalExpressionAST* node, ConditionalExpressionAST* pattern)
{
    if (!node || !pattern)
        return false;
    if (!AST::match(node->condition, pattern->condition, this))
        return false;
    if (!AST::match(node->left_expression, pattern->left_expression, this))
        return false;
    if (!AST::match(node->right_expression, pattern->right_expression, this))
        return false;
    return true;
}

bool ASTIdentityMatcher::match(IdExpressionAST* node, IdExpressionAST* pattern)
{
    if (!node || !pattern)
        return false;
    if (!AST::match(node->name, pattern->name, this))
        return false;
    return true;
}

bool ASTIdentityMatcher::match(MemberAccessAST* node, MemberAccessAST* pattern)
{
    if (!node || !pattern)
        return false;
    if (!AST::match(node->base_expression, pattern->base_expression, this))
        return false;
    if (unit_->tokenKind(node->access_token) != unit_->tokenKind(pattern->access_token))
        return false;
    if (!AST::match(node->member_name, pattern->member_name, this))
        return false;
    return true;
}

bool ASTIdentityMatcher::match(NestedExpressionAST* node, NestedExpressionAST* pattern)
{
    if (!node || !pattern)
        return false;
    if (!AST::match(node->expression, pattern->expression, this))
        return false;
    return true;
}

bool ASTIdentityMatcher::match(NumericLiteralAST* node, NumericLiteralAST* pattern)
{
    if (!node || !pattern)
        return false;
    if (unit_->tokenKind(node->literal_token) != unit_->tokenKind(pattern->literal_token))
        return false;
    if (unit_->spell(node->literal_token) != unit_->spell(pattern->literal_token))
        return false;
    return true;
}

bool ASTIdentityMatcher::match(StringLiteralAST* node, StringLiteralAST* pattern)
{
    if (!node || !pattern)
        return false;
    if (unit_->tokenKind(node->literal_token) != unit_->tokenKind(pattern->literal_token))
        return false;
    if (unit_->spell(node->literal_token) != unit_->spell(pattern->literal_token))
        return false;
    if (!AST::match(node->next, pattern->next, this))
        return false;
    return true;
}

bool ASTIdentityMatcher::match(PointerLiteralAST* node, PointerLiteralAST* pattern)
{
    if (!node || !pattern)
        return false;
    if (unit_->tokenKind(node->literal_token) != unit_->tokenKind(pattern->literal_token))
        return false;
    return true;
}

bool ASTIdentityMatcher::match(PostIncrDecrAST* node, PostIncrDecrAST* pattern)
{
    if (!node || !pattern)
        return false;
    if (!AST::match(node->base_expression, pattern->base_expression, this))
        return false;
    if (unit_->tokenKind(node->incr_decr_token) != unit_->tokenKind(pattern->incr_decr_token))
        return false;
    return true;
}

bool ASTIdentityMatcher::match(SizeofExpressionAST* node, SizeofExpressionAST* pattern)
{
    if (!node || !pattern)
        return false;
    if (!AST::match(node->expression, pattern->expression, this))
        return false;
    return true;
}

bool ASTIdentityMatcher::match(UnaryExpressionAST* node, UnaryExpressionAST* pattern)
{
    if (!node || !pattern)
        return false;
    if (!AST::match(node->expression, pattern->expression, this))
        return false;
    if (unit_->tokenKind(node->unary_op_token) != unit_->tokenKind(pattern->unary_op_token))
        return false;
    return true;
}

bool ASTIdentityMatcher::match(ClassSpecifierAST* node, ClassSpecifierAST* pattern)
{
    if (!node || !pattern)
        return false;
    if (unit_->tokenKind(node->classkey_token) != unit_->tokenKind(pattern->classkey_token))
        return false;
    if (!AST::match(node->attribute_list, pattern->attribute_list, this))
        return false;
    if (!AST::match(node->name, pattern->name, this))
        return false;
    return true;
}

bool ASTIdentityMatcher::match(ElaboratedTypeSpecifierAST* node, ElaboratedTypeSpecifierAST* pattern)
{
    if (!node || !pattern)
        return false;
    if (!AST::match(node->attribute_list, pattern->attribute_list, this))
        return false;
    if (!AST::match(node->name, pattern->name, this))
        return false;
    return true;
}

bool ASTIdentityMatcher::match(EnumSpecifierAST* node, EnumSpecifierAST* pattern)
{
    if (!node || !pattern)
        return false;
    if (!AST::match(node->name, pattern->name, this))
        return false;
    return true;
}

bool ASTIdentityMatcher::match(GnuAttributeSpecifierAST* node, GnuAttributeSpecifierAST* pattern)
{
    if (!node || !pattern)
        return false;
    if (unit_->tokenKind(node->attribute_token) != unit_->tokenKind(pattern->attribute_token))
        return false;
    if (!AST::match(node->attribute_list, pattern->attribute_list, this))
        return false;
    return true;
}

bool ASTIdentityMatcher::match(NamedTypeSpecifierAST* node, NamedTypeSpecifierAST* pattern)
{
    if (!node || !pattern)
        return false;
    if (!AST::match(node->name, pattern->name, this))
        return false;
    return true;
}

bool ASTIdentityMatcher::match(SimpleSpecifierAST* node, SimpleSpecifierAST* pattern)
{
    if (!node || !pattern)
        return false;
    if (unit_->tokenKind(node->specifier_token) != unit_->tokenKind(pattern->specifier_token))
        return false;
    return true;
}

bool ASTIdentityMatcher::match(TypeofSpecifierAST* node, TypeofSpecifierAST* pattern)
{
    if (!node || !pattern)
        return false;
    if (!AST::match(node->expression, pattern->expression, this))
        return false;
    return true;
}

bool ASTIdentityMatcher::match(DeclaratorAST* node, DeclaratorAST* pattern)
{
    if (!node || !pattern)
        return false;
    if (!AST::match(node->attribute_list, pattern->attribute_list, this))
        return false;
    if (!AST::match(node->ptr_operator_list, pattern->ptr_operator_list, this))
        return false;
    if (!AST::match(node->core_declarator, pattern->core_declarator, this))
        return false;
    if (!AST::match(node->postfix_declarator_list, pattern->postfix_declarator_list, this))
        return false;
    if (!AST::match(node->post_attribute_list, pattern->post_attribute_list, this))
        return false;
    if (unit_->tokenKind(node->equal_token) != unit_->tokenKind(pattern->equal_token))
        return false;
    if (!AST::match(node->initializer, pattern->initializer, this))
        return false;
    return true;
}

bool ASTIdentityMatcher::match(DeclaratorIdAST* node, DeclaratorIdAST* pattern)
{
    if (!node || !pattern)
        return false;
    if (!AST::match(node->name, pattern->name, this))
        return false;
    return true;

}

bool ASTIdentityMatcher::match(PointerAST* node, PointerAST* pattern)
{
    if (!node || !pattern)
        return false;
    if (unit_->tokenKind(node->star_token) != unit_->tokenKind(pattern->star_token))
        return false;
    if (!AST::match(node->cv_qualifier_list, pattern->cv_qualifier_list, this))
        return false;
    return true;
}

bool ASTIdentityMatcher::match(SimpleNameAST* node, SimpleNameAST* pattern)
{
    if (!node || !pattern)
        return false;
    return !(strcmp(node->name->identifier()->begin(),
                    pattern->name->identifier()->begin()));
}

bool ASTIdentityMatcher::match(TaggedNameAST* node, TaggedNameAST* pattern)
{
    if (!node || !pattern)
        return false;
    return !(strcmp(node->name->identifier()->begin(),
                    pattern->name->identifier()->begin()));
}
