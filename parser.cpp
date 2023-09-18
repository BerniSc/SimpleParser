// A simple Example of a Parser written in C++ using Boost Spirit and an abstract syntax tree (AST)
//
// The grammar accepts expressions like "y = 1 + 2 * x", constructs an AST and
// evaluates it. Non-assignment expression are also evaluated.

#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <math.h>

#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix.hpp>

namespace qi = boost::spirit::qi;
namespace phx = boost::phoenix;

/******************************************************************************/

// Utility to run a parser, check for errors, and capture the results.
template<typename Parser, typename Skipper, typename ... Args>
void PhraseParseOrDie(const std::string& input, const Parser& p, const Skipper& s, Args&& ... args) {
    std::string::const_iterator begin = input.begin(), end = input.end();
    boost::spirit::qi::phrase_parse(begin, end, p, s, std::forward<Args>(args) ...);
    if (begin != end) {
        std::cout << "Unparseable: " << std::quoted(std::string(begin, end)) << std::endl;
        throw std::runtime_error("Parse error");
    }
}

/******************************************************************************/

// the variable value map (Map x from equation to actual double value)
std::map<std::string, double> variable_map;

//Abstract Interface for Node of Tree
class ASTNode {
public:
    virtual double evaluate() = 0;
    virtual ~ASTNode() { }
};

using ASTNodePtr = ASTNode*;

// Extend abstract Node Interface to specify handling of Operators
template<char Operator>
class OperatorNode : public ASTNode {
public:
    //Each Operator has LHS and RHS -> Therefore children
    OperatorNode(const ASTNodePtr& left, const ASTNodePtr& right) : left(left), right(right) { }

    //recursive descent until last Node is actual Constant Number Value
    double evaluate() {
        if (Operator == '+')
            return left->evaluate() + right->evaluate();
        else if (Operator == '*')
            return left->evaluate() * right->evaluate();
        else if (Operator == '-')
            return left->evaluate() - right->evaluate();
        else if (Operator == '/')
            return left->evaluate() / right->evaluate();
        else if (Operator == '^')
            return pow(left->evaluate(), right->evaluate());
        else if (Operator == 'A')
            return (bool(left->evaluate()) * bool(right->evaluate()));
        else if (Operator == 'O')
            return bool(bool(left->evaluate()) + bool(right->evaluate()));
    }

    ~OperatorNode() {
        delete left;
        delete right;
    }

private:
    ASTNodePtr left, right;
};

// Extend abstract Node Interface to specify handling of Numbers
class ConstantNode : public ASTNode {
public:
    //Does not need Children as it is just numbers
    ConstantNode(double value) : value(value) { }

    double evaluate() {
        return value;
    }

private:
    double value;
};

// Extend abstract Node Interface to specify handling of Variables
class VariableNode : public ASTNode {
public:
    //Stores "Name" of variable
    VariableNode(std::string identifier) : identifier(identifier) { }

    //Returns Maped value of Variable
    double evaluate() {
        return variable_map[identifier];
    }

private:
    std::string identifier;
};

// Extend abstract Node Interface to specify handling of Equationsymbol
template<char Operator>
class AssignmentNode : public ASTNode {
public:
    //Allowing "Ans" by also storing current Value in identifier, but effectivly only needs to evaluate whole tree as it is Top Node
    AssignmentNode(std::string identifier, const ASTNodePtr& value) : identifier(identifier), value(value) { }

    double evaluate() {
        if(Operator == '=') {
            double v = value->evaluate();
            variable_map[identifier] = v;
            return v;
        } else if(Operator == '+') {
            double v = value->evaluate();
            variable_map[identifier] += v;
            return variable_map[identifier];
        } else if(Operator == '-') {
            double v = value->evaluate();
            variable_map[identifier] -= v;
            return variable_map[identifier];
        } else if(Operator == '*') {
            double v = value->evaluate();
            variable_map[identifier] *= v;
            return variable_map[identifier];
        } else if(Operator == '/') {
            double v = value->evaluate();
            variable_map[identifier] /= v;
            return variable_map[identifier];
        }
    }

private:
    std::string identifier;
    ASTNodePtr value;
};

/******************************************************************************/
// EBNF of this Grammar:
//      varname = "A" .. "z" , { <alphanumeric> }
//      start = (varname, ("=" | "+=" | "-=" | "*=" | "/=") , term) | term
//      term = product, ("+" | "-"), term | product
//      product = (factor, ("*" | "/" | "^" | "&&" | "||") , product) | (factor)
//      factor = group | varname | double-number
//      group = "(", term, ")"


//Acutall Grammar 
class ArithmeticGrammar : public qi::grammar<std::string::const_iterator, ASTNodePtr(), qi::space_type> {
public:
    using Iterator = std::string::const_iterator;

    ArithmeticGrammar() : ArithmeticGrammar::base_type(start) {
        //Say that varname equals alpha(character a-Z) or arbitrary number of alum(alphanumeric) i.E. it is possible to say var=42 and then use var
        // %= is the most of the time equivalent of =, but needs to be %= this case as varname needs to be evaluated Later
        varname %= qi::alpha >> *qi::alnum;

        // _val is set through lambdas -> Placeholder for actual value later

        // Start either has to be either varname (f.e. 'y=') and then Term or just Term;
        // Also Assignment Operations are allowed like +=, -=, *= and /= 
        start = (varname >> '=' >> term)[qi::_val = phx::new_<AssignmentNode<'='>>(qi::_1, qi::_2)] | 
                (varname >> qi::lit("+=") >> term)[qi::_val = phx::new_<AssignmentNode<'+'>>(qi::_1, qi::_2)] |
                (varname >> qi::lit("-=") >> term)[qi::_val = phx::new_<AssignmentNode<'-'>>(qi::_1, qi::_2)] |
                (varname >> qi::lit("*=") >> term)[qi::_val = phx::new_<AssignmentNode<'*'>>(qi::_1, qi::_2)] |
                (varname >> qi::lit("/=") >> term)[qi::_val = phx::new_<AssignmentNode<'/'>>(qi::_1, qi::_2)] |
                term[qi::_val = qi::_1];

        // Term either has to be a Product +/- a Term or it can be just a Product
        term = (product >> '+' >> term)[qi::_val = phx::new_<OperatorNode<'+'> >(qi::_1, qi::_2)] |
            (product >> '-' >> term)[qi::_val = phx::new_<OperatorNode<'-'> >(qi::_1, qi::_2)] |
            product[qi::_val = qi::_1];

        // Product can be a factor times a product or just a factor
        product = (factor >> '*' >> product)[qi::_val = phx::new_<OperatorNode<'*'> >(qi::_1, qi::_2)] |
            (factor >> '/' >> product)[qi::_val = phx::new_<OperatorNode<'/'> >(qi::_1, qi::_2)] |
            (factor >> '^' >> product)[qi::_val = phx::new_<OperatorNode<'^'> >(qi::_1, qi::_2)] |
            (factor >> qi::lit("&&") >> product)[qi::_val = phx::new_<OperatorNode<'A'>>(qi::_1, qi::_2)] |
            (factor >> qi::lit("||") >> product)[qi::_val = phx::new_<OperatorNode<'O'>>(qi::_1, qi::_2)] |
            factor[qi::_val = qi::_1];

        // Factor can be a group, a varname or just a regular int
        factor = group[qi::_val = qi::_1] | 
            varname[qi::_val = phx::new_<VariableNode>(qi::_1)] | 
            qi::double_[qi::_val = phx::new_<ConstantNode>(qi::_1)];

        //Group is just Brackets with Term in Middle -> Needs to be evaluated later, Therefore has %=
        group %= '(' >> term >> ')';
    }

    //Uses Iterator to go over parsed string, store output in string and as a skipper the qi::space_type is expected
    qi::rule<Iterator, std::string(), qi::space_type> varname;
    //Same but stores in ASTNote Pointer
    qi::rule<Iterator, ASTNodePtr(), qi::space_type> start, term, group, product, factor;
};

void testGrammar(std::string input) {
    try {
        ASTNode* out_node;
        // Pass input, Parser, skipper (space) and output-Node
        PhraseParseOrDie(input, ArithmeticGrammar(), qi::space, out_node);

        std::cout << "evaluate() = " << out_node->evaluate() << std::endl;
        delete out_node;
    }
    catch (std::exception& e) {
        std::cout << "EXCEPTION was THROWN: " << e.what() << std::endl;
    }
}

/******************************************************************************/

int main() {
    // important variables
    variable_map["x"] = 42;
    variable_map["pi"] = 3.14159265359;

    std::cout << "Reading stdin" << std::endl;

    std::string line;
    while (std::getline(std::cin, line)) {
        testGrammar(line);
    }

    return 0;
}

/******************************************************************************/
