/*
 * Copyright 2002-2003. The Regents of the University of California. This material 
 * was produced under U.S. Government contract W-7405-ENG-36 for Los Alamos 
 * National Laboratory, which is operated by the University of California for 
 * the U.S. Department of Energy. The Government is granted for itself and 
 * others acting on its behalf a paid-up, nonexclusive, irrevocable worldwide 
 * license in this material to reproduce, prepare derivative works, and 
 * perform publicly and display publicly. Beginning five (5) years after 
 * October 10,2002 subject to additional five-year worldwide renewals, the 
 * Government is granted for itself and others acting on its behalf a paid-up, 
 * nonexclusive, irrevocable worldwide license in this material to reproduce, 
 * prepare derivative works, distribute copies to the public, perform publicly 
 * and display publicly, and to permit others to do so. NEITHER THE UNITED 
 * STATES NOR THE UNITED STATES DEPARTMENT OF ENERGY, NOR THE UNIVERSITY OF 
 * CALIFORNIA, NOR ANY OF THEIR EMPLOYEES, MAKES ANY WARRANTY, EXPRESS OR 
 * IMPLIED, OR ASSUMES ANY LEGAL LIABILITY OR RESPONSIBILITY FOR THE ACCURACY, 
 * COMPLETENESS, OR USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT, OR 
 * PROCESS DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE PRIVATELY 
 * OWNED RIGHTS.

 * Additionally, this program is free software; you can distribute it and/or 
 * modify it under the terms of the GNU Lesser General Public License as 
 * published by the Free Software Foundation; either version 2 of the License, 
 * or any later version.  Accordingly, this program is distributed in the hope 
 * that it will be useful, but WITHOUT ANY WARRANTY; without even the implied 
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * GNU Lesser General Public License for more details.
 */
/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/



#ifndef _PARSETREE
#define _PARSETREE

#include <string.h>

//! A pair of delimiters in an expression
/*!
  This class represents a pair of delimiters in an
  expression.  It is used for error-checking
*/

class DelimPair {
public:
    int l_index, r_index;
};

//! A node in the Parse Tree
/*!
  This class represents a node in the ParseTree. After
  parsing, each node in the parse tree will contain
  either an operator or a value.
  \sa ParseTree
*/
class Node {
    Node* parent;    //!< Pointer to parent
    Node** children; //!< Array of pointers to children
    Node* sibling;   //!< Pointer to right sibling
    char* expr;      //!< Expression represented by node

public:
    //! Constructor
    /*!
      \param e Expression represented by node
      \param p Pointer to parent of node
      \param c Array of pointers to children of node
      \param s Pointer to sibling of node
    */
    Node(char* e = NULL, Node* p = NULL,
         Node** c = NULL, Node* s = NULL);

    //! Destructor
    ~Node();

    // accessors
    //! Returns pointer to parent of node
    Node* getParent() const { return parent; }
    //! Returns array of pointers to children of node
    Node** getChildren() const { return children; }
    //! Returns pointer to sibling of node
    Node* getSibling() const { return sibling; }
    //! Returns expression represented by node
    char* getExpression() const { return expr; }

    // mutators
    //! Sets parent of node to p
    void setParent(Node* p) { parent = p; }
    //! Sets children of node to c
    void setChildren(Node** c ) { children = c; }
    //! Sets sibling of node to s
    void setSibling(Node* s) { sibling = s; }
    //! Sets expression of node to e
    void setExpression(char* e) { expr = e; }
};

//! A simple recursive descent parser
/*!
  This class implements a simple recursive descent
  parser.  Each ParseTree object, when created, is
  initialized with a set of operators and a set of
  delimiters.  Then, the setExpression(expr) method
  can be called to create the parse tree of the
  expression utilizing the operators and delimiters.
*/
class ParseTree {
    // state
    Node* root;    //!< pointer to root node of tree
    char *opr;     //!< list of one char operators
    char* delims;  //!< list of one char delimiter pairs
    char* expr;    //!< expression parsed
    int index;     //!< used by getLeaves() to count leaves
    bool correct;  //!< indicates if an error occured when tree is built
    char* errStr;  //!< String containing error message

    // 'helper' methods
    //! Deletes all nodes from the tree
    void clearTree();
    //! Parses the expression in the node pointed to by n
    void parse(Node* n);
    //! Removes the delimiters surrounding the expression expr
    char* remDelims(char* expr);
    //! Trims whitespace from around the expression expr
    char* trimWhitespace(char* e);
    //! Counts the delimiters *delims in the expression *expr
    int delimCount(char* expr, char* delims);
    //! Finds the next pair of delimiters in expr
    DelimPair* findPair(char* expr, char l_delim,
                        char r_delim, int offset = 0);
    //! Detects errors in delimiter placement in expr
    int pairTest(char *expr, char* delims);
    //! Makes the childe nodes of the expression e, split at index
    Node** makeChildren(char* e, int index);

public:
    //! Constructor
    /*!
      \param o Operators (i.e. separators) to be parsed from expression
      \param d Delimiters
      \param e Expression to be parsed
    */
    ParseTree(char* o, char* d, char* e = NULL);

    //! Destructor
    ~ParseTree();

    // accessors
    //! Returns operators used in construction of parse tree
    char* getOperator() const { return opr; }
    //! Returns delimiters used in construction of parse tree
    char* getDelimiters() const { return delims; }
    //! Returns expression parsed by parse tree
    char* getExpression() const { return expr; }
    //! Returns a pointer to the root of the parse tree
    Node* getRoot() const { return root; }
    //! Returns whether the expression parsed was correct
    bool isCorrect() { return correct; }
    //! Returns an error string if expression was incorrect
    char* getError() { return errStr; }

    // mutators
    //! Sets the operators to o and triggers a new parse
    void setOperator(char* o) { opr = o; }
    //! Sets the delimiters to d and triggers a new parse
    void setDelimeters(char* d) { delims = d; }
    //! Sets the expression to e and triggers a new parse
    void setExpression(char* e);

    // functionality
    //! Returns the number of leaves in the tree rooted at n
    int countLeaves(Node* n);
    /*!
      Returns a list of pointers to the leaves of the tree
      rooted at n.
    */
    void getLeaves(Node* n, Node** nlist,
                   int start = 1);
};


#endif
