////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2026 OVITO GmbH, Germany
//
//  This file is part of OVITO (Open Visualization Tool).
//
//  OVITO is free software; you can redistribute it and/or modify it either under the
//  terms of the GNU General Public License version 3 as published by the Free Software
//  Foundation (the "GPL") or, at your option, under the terms of the MIT License.
//  If you do not alter this notice, a recipient may use your version of this
//  file under either the GPL or the MIT License.
//
//  You should have received a copy of the GPL along with this program in a
//  file LICENSE.GPL.txt.  You should have received a copy of the MIT License along
//  with this program in a file LICENSE.MIT.txt
//
//  This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND,
//  either express or implied. See the GPL or the MIT License for the specific language
//  governing rights and limitations.
//
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ovito/stdobj/StdObj.h>

namespace Ovito::PropertyExpressionRewriter {

// Tokenize an expression
[[nodiscard]] QStringList tokenizeExpression(const QString& expression);

// Known operators for parser
enum class Op : uint8_t
{
    AND,   // &&
    OR,    // ||
    EQ,    // ==
    NEQ,   // !=
    GE,    // >
    GEQ,   // >=
    LE,    // <
    LEQ,   // <=
    QM,    // ?
    COL,   // :
    ADD,   // +
    SUB,   // -
    MULT,  // *
    DIV,   // /
    POW,   // ^
    NONE   // Blank
};

// Convert an operation (Op) enum to its string representation.
inline QString OpToString(Op op);

// Convert a string to operation (Op) enum.
inline Op StringToOp(const QString& str);

// Known AST node types (for runtime type information)
enum class ASTNodeType : uint8_t
{
    IDENTIFIER,
    MULTIIDENTIFIER,
    UNARYOP,
    BINARYOP,
    TERNARYOP,
    FUNC,
    NONE
};

// ASTNode base class
struct ASTNode {
    // Holds the specific type of the ASTNode for later lookup
    explicit ASTNode(bool group, ASTNodeType type = ASTNodeType::NONE) : type{type}, group{group} {}
    ASTNodeType type;
    bool group;

#ifdef OVITO_DEBUG
    void debugPrint() const;
#endif
};

// Types used for mapping of string type names to integer values
using ValueType = QStringList;
using InnerMapType = std::unordered_map<QString, ValueType>;
using MapType = std::unordered_map<QString, InnerMapType>;

// Describes one unique string equality or regex comparison found during expression rewriting.
// For equality: both == and != share the same spec; the appended ==0 / !=0 encodes the operator.
// For regex: regexPattern is non-empty; regexFlags holds optional modifiers (e.g. "i").
struct StringComparisonSpec {
    QString leftPropName;   ///< Mangled name of the left-hand string property.
    QString rightLiteral;   ///< The literal string value (empty for variable–variable or regex comparison).
    QString rightPropName;  ///< Mangled name of right-hand string property (empty for literal or regex comparison).
    QString regexPattern;   ///< The regex pattern (empty for equality comparisons).
    QString regexFlags;     ///< Optional regex modifier flags, e.g. "i" for case-insensitive.
    bool operator==(const StringComparisonSpec&) const = default;
};

// Returns true if the expression references at least one token that is a registered
// string property, meaning step-1 string-comparison rewriting is required.
[[nodiscard]] bool expressionNeedsStringRewrite(const QString& expr,
    const std::unordered_set<QString>& stringPropertyNames);

// Returns true if the expression contains a double-quote character, meaning step-2
// AST-based type-name rewriting may be required.
[[nodiscard]] bool expressionNeedsTypeRewrite(const QString& expr);

// Rewrite string property comparison patterns (== / !=) in an expression by replacing
// each unique comparison with a generated muParser variable name.
// Generated variable names and their specifications are recorded in comparisonMap.
// Example: 'MyProp == "Value A"' becomes '__MyProp_eq_lit_Value_A__!=0'.
[[nodiscard]] QString rewriteStringPropertyComparisons(const QString& expr,
    const std::unordered_set<QString>& stringPropertyNames,
    std::unordered_map<QString, StringComparisonSpec>& comparisonMap);

// Returns true if the expression contains =~ and at least one token that is a registered
// string property, meaning regex-rewriting is required.
[[nodiscard]] bool expressionNeedsRegexRewrite(const QString& expr,
    const std::unordered_set<QString>& stringPropertyNames);

// Rewrite regex match patterns (=~ /pattern/ or =~ /pattern/i) in an expression by replacing
// each unique match with a generated muParser variable name.
// Generated variable names and their specifications are recorded in comparisonMap.
// Example: 'MyProp =~ /Fe.*/' becomes '__MyProp_rx_Fe____!=0'.
[[nodiscard]] QString rewriteRegexComparisons(const QString& expr,
    const std::unordered_set<QString>& propertyNames,
    std::unordered_map<QString, StringComparisonSpec>& comparisonMap);

/**
 * \brief Helper class that parses a (tokenized) math expression into an AST for post processing.
 * mapping: a dict containing the mapping from str to int | vector<int>
 *    e.g., Ni -> (4, 5, 6)
 *          Li -> 1
 */
class Parser
{
public:
    // Create a new parser
    // The lifetime of mapping MUST exceed the usage of the AST as strings are not copied
    Parser(const MapType& mapping) : _mapping(mapping) {}

    // Parse a list of tokens into an AST
    // The lifetime of tokens MUST exceed the usage of the AST as strings are not copied
    [[nodiscard]] std::unique_ptr<ASTNode> parse(const QString* expression, const QStringList* tokens);

private:
    // Converts the token index variable to an approximate position in the original expression.
    [[nodiscard]] qsizetype indexToPosition(int index) const;

    // Return the current token without consuming it.
    [[nodiscard]] const QString* peek() const;

    // Consume and return the current token.
    [[nodiscard]] const QString* consume();

    // If the current token is in 'expected', consume and return it else return null.
    [[nodiscard]] std::optional<Op> match(std::initializer_list<QStringView> expected);

    // Parse the current expression.
    [[nodiscard]] std::unique_ptr<ASTNode> parseExpression();

    // Parse ternary expression: OrExpr ( '?' TernaryExpr ':' TernaryExpr )
    [[nodiscard]] std::unique_ptr<ASTNode> parseTernary();

    // Parse OrExpr -> AndExpr ( '||' AndExpr ).
    [[nodiscard]] std::unique_ptr<ASTNode> parseOrExpression();

    // Parse AndExpr -> Comparison ( '&&' Comparison ).
    [[nodiscard]] std::unique_ptr<ASTNode> parseAndExpression();

    // Parse left and right side of exactly one comparison operator
    [[nodiscard]] std::unique_ptr<ASTNode> parseComparison();

    // Parse left and right side of one or more chained math operations
    [[nodiscard]] std::unique_ptr<ASTNode> parseMathOperation(ASTNode* left_inp = nullptr);

    // Parse a Primary: Identifier or '(' Expression ')'
    [[nodiscard]] std::unique_ptr<ASTNode> parsePrimary(ASTNode* left_inp = nullptr);

private:
    // Example mapping for structure and particle types:
    // MapType mapping = {{"StructureType", {{"'other'", {"0"}}, {"'fcc'", {"1"}}, {"'hcp'", {"2"}}, {"'bcc'", {"3"}}}},
    //                            {"ParticleType", {{"'Li'", {"1"}}, {"'Co'", {"2"}}, {"'O'", {"3"}}, {"'Ni'", {"4", "5", "6"}}}}};
    const MapType& _mapping;

    // List of tokens to be processed
    const QStringList* _tokens;

    // Original expression
    const QString* _expression;

    // Current index
    int _index = 0;
};

/**
 * \brief Helper class that rewrites a math expression AST to account for type names.
 */
class ASTWriter
{
public:
    ASTWriter(const MapType& mapping) : _mapping{mapping} {}

    // Convert an AST back into a string, replacing type names with integer values
    // as specified in '_mappings'.
    // Tokens and mapping from the ast generation MUST still be valid.
    [[nodiscard]] QString write(const ASTNode* astNode);

private:
    // Convert an identifier to its string list representation
    [[nodiscard]] const QStringList* expressionToValuesList(const ASTNode* astNode);

    // Distribute potentially multi valued ternary
    [[nodiscard]] QString handleBinaryWithTernary(const QString& leftString, const ASTNode* rightNode);

private:
    // Example mapping for structure and particle types:
    // MapType mapping = {{"StructureType", {{"'other'", {"0"}}, {"'fcc'", {"1"}}, {"'hcp'", {"2"}}, {"'bcc'", {"3"}}}},
    //                            {"ParticleType", {{"'Li'", {"1"}}, {"'Co'", {"2"}}, {"'O'", {"3"}}, {"'Ni'", {"4", "5", "6"}}}}};
    const MapType& _mapping;

    // Scratch space used during processing
    QStringList _scratch;
};

}  // namespace Ovito::PropertyExpressionRewriter
