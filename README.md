# Parser
A small Project to explore the Boost::qi::spirit and boost::phoenix parser lib and get into how a simple AST Parser works.<br> 
This example implements a small calculator and its syntax as an EBNF.<br>
<br>
<br>
It's grammer consists of the following EBNF: <br>
``` 
EBNF of this Grammar:
    varname = "A" .. "z" , { <alphanumeric> }
    start = (varname, "=", term) | term
    term = product, ("+" | "-"), term | product
    product = (factor, ("*" | "/" | "^"), product) | (factor)
    factor = group | varname | integer-number
    group = "(", term, ")"
```