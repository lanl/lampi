/*
 * Copyright 2002-2003. The Regents of the University of
 * California. This material was produced under U.S. Government
 * contract W-7405-ENG-36 for Los Alamos National Laboratory, which is
 * operated by the University of California for the U.S. Department of
 * Energy. The Government is granted for itself and others acting on
 * its behalf a paid-up, nonexclusive, irrevocable worldwide license
 * in this material to reproduce, prepare derivative works, and
 * perform publicly and display publicly. Beginning five (5) years
 * after October 10,2002 subject to additional five-year worldwide
 * renewals, the Government is granted for itself and others acting on
 * its behalf a paid-up, nonexclusive, irrevocable worldwide license
 * in this material to reproduce, prepare derivative works, distribute
 * copies to the public, perform publicly and display publicly, and to
 * permit others to do so. NEITHER THE UNITED STATES NOR THE UNITED
 * STATES DEPARTMENT OF ENERGY, NOR THE UNIVERSITY OF CALIFORNIA, NOR
 * ANY OF THEIR EMPLOYEES, MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR
 * ASSUMES ANY LEGAL LIABILITY OR RESPONSIBILITY FOR THE ACCURACY,
 * COMPLETENESS, OR USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT,
 * OR PROCESS DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE
 * PRIVATELY OWNED RIGHTS.

 * Additionally, this program is free software; you can distribute it and/or 
 * modify it under the terms of the GNU Lesser General Public License as 
 * published by the Free Software Foundation; either version 2 of the License, 
 * or any later version.  Accordingly, this program is distributed in the hope 
 * that it will be useful, but WITHOUT ANY WARRANTY; without even the implied 
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * GNU Lesser General Public License for more details.
 */
/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdio.h>

#include "internal/new.h"
#include "util/ParseTree.h"

// Node method definitions
Node::Node(char *e, Node * p, Node ** c, Node * s)
{
    parent = p;
    children = c;
    sibling = s;
    expr = e;
}

Node::~Node()
{
    // If parent hasn't been deleted, remove correct child ptr.
    if (parent != NULL) {
        Node **c = parent->children;
        if (c[0] == this) {
            c[0] = NULL;
        } else {
            c[1] = NULL;
        }
    }
    parent = NULL;              // clear parent

    sibling = NULL;             // clear sibling

    if (expr != NULL) {
        ulm_delete(expr);
    }
    if (children != NULL) {
        // delete children, if they exist
        for (int i = 0; i < 2; i++) {
            delete children[i];
            children[i] = NULL;
        }
        ulm_delete(children);
    }
}

// ParseTree method definitions
ParseTree::ParseTree(char *o, char *d, char *e)
{
    opr = o;
    delims = d;
    expr = NULL;
    root = NULL;
    index = 0;
    correct = true;
    errStr = ulm_new(char, 250);

    // parse expression if specified to c'tor
    if (e != NULL)
        setExpression(e);
}

ParseTree::~ParseTree()
{
    ulm_delete(errStr);
    errStr = NULL;
    clearTree();
}

// This method deletes the tree and expression
void ParseTree::clearTree()
{
    errStr = NULL;
    correct = true;
    if (root != NULL) {
        delete root;
        root = NULL;
    }
}

// This method recursively generates the parse subtree
// for the expression contained in the node *n.  If the
// root is passed in, it generates the entire parse tree
void ParseTree::parse(Node * n)
{
    // extract expression from node
    char *node_expr = n->getExpression();

    // parse through expression
    int num_ops = strlen(opr);
    int num_delims = strlen(delims);
    int expr_sz = strlen(node_expr);
    int index;
    for (int i = 0; i < num_ops; i++) {
        for (index = 0; index < expr_sz; index++) {

            // character is an operator
            if (node_expr[index] == opr[i]) {
                char *op = ulm_new(char, 2);
                op[0] = '\0', op[1] = '\0';
                strncpy(op, opr + i, 1);
                Node **c = makeChildren(node_expr, index);
                n->setExpression(op);
                ulm_delete(node_expr);
                node_expr = NULL;
                c[0]->setParent(n);
                c[1]->setParent(n);
                n->setChildren(c);

                parse(c[0]);
                parse(c[1]);
                return;
            }
            // character is a delimiter
            for (int j = 0; j < num_delims; j += 2) {
                if (node_expr[index] == delims[j]) {
                    char l_delim = delims[j];
                    char r_delim = delims[j + 1];
                    int delim_cnt = 1;
                    while (index < expr_sz && delim_cnt > 0) {
                        index++;
                        if (node_expr[index] == r_delim)
                            delim_cnt--;
                        else if (node_expr[index] == l_delim)
                            delim_cnt++;
                    }

                    if (index == expr_sz) {
                        fprintf(stderr, "Mismatched delimiters\n");
                        sprintf(errStr, "Mismatched delimiters\n");
                        correct = false;
                        return;
                    }
                }
            }
        }
    }
}

// Returns an array of Node* pointing to the left
// and right subexpressions of the expression e.
// index is the index of the operator in e.
Node **ParseTree::makeChildren(char *e, int index)
{
    if (e == NULL ||
        (size_t) index > strlen(e) - 1)
        return NULL;

    char *l_val, *r_val;
    int expr_sz = strlen(e);

    l_val = ulm_new(char, index + 1);
    strncpy(l_val, e, index);
    l_val[index] = '\0';
    r_val = ulm_new(char, expr_sz - index);
    strncpy(r_val, e + index + 1,
            expr_sz - index - 1);
    r_val[expr_sz - index - 1] = '\0';

    // remove enclosing delimiters, if they exist and
    // trim any leading/trailing whitespace
    char *temp_l = trimWhitespace(l_val);
    char *temp_r = trimWhitespace(r_val);
    char *rd_lval = remDelims(temp_l);
    char *rd_rval = remDelims(temp_r);
    char *trimmed_lval = trimWhitespace(rd_lval);
    char *trimmed_rval = trimWhitespace(rd_rval);

    Node **c = ulm_new(Node *, 2);
    c[0] = new Node();
    c[0]->setExpression(trimmed_lval);
    c[1] = new Node();
    c[1]->setExpression(trimmed_rval);
    c[0]->setSibling(c[1]);
    ulm_delete(l_val);
    ulm_delete(r_val);
    ulm_delete(temp_l);
    ulm_delete(temp_r);
    ulm_delete(rd_lval);
    ulm_delete(rd_rval);
    return c;
}

// removes enclosing delimiters from the expression expr
// and returns the modified string
char *ParseTree::remDelims(char *expr)
{
    if (expr == NULL)
        return NULL;

    // find delimiter type
    char l_delim = 0, r_delim = 0;
    int found_delim = 0;
    for (int j = 0; (size_t) j < strlen(delims); j += 2) {
        if (expr[0] == delims[j]) {
            l_delim = delims[j];
            r_delim = delims[j + 1];
            found_delim = 1;
            break;
        }
    }

    // return copy of expression if there are no
    // surrounding delimiters
    if (found_delim == 0) {
        char *buf = ulm_new(char, strlen(expr) + 1);
        // for(int i = 0; i < strlen(expr)+1; i++) buf[i] = '\0';
        strcpy(buf, expr);
        buf[strlen(expr)] = '\0';
        return buf;
    }
    // find first left delimiter
    char *start = strchr(expr, l_delim);

    // find last right delimiter
    char *end = strrchr(expr, r_delim);

    // create the string between them
    int new_len = strlen(start) - strlen(end) - 1;
    char *buf = ulm_new(char, new_len + 1);
    strncpy(buf, start + 1, new_len);
    buf[new_len] = '\0';

    return buf;
}

// Sets the expression for the parse tree and
// parses the expression.  Performs error checking
// on delimiters within the expression.
void ParseTree::setExpression(char *e)
{
    // set expression
    expr = e;
    char *trimmed_expr = trimWhitespace(e);

    // check delimiters
    int passed = pairTest(trimmed_expr, delims);
    if (passed == 0) {
        fprintf(stderr, "Syntax error in expression '%s' will not parse!\n",
                trimmed_expr);
        sprintf(errStr, "Syntax error in expression '%s' will not parse!\n",
                trimmed_expr);
        correct = false;
        return;
    }
    // clear the current tree, if it exists
    if (root != NULL)
        clearTree();

    // create a new root
    root = new Node(trimmed_expr);

    // create parse tree
    parse(root);
}

// Removes leading/trailing whitespace from
// the expression e.
char *ParseTree::trimWhitespace(char *e)
{
    if (e == NULL)
        return NULL;

    char *start = e;
    while (*start == ' ')
        start++;
    char *end = e + strlen(e) - 1;
    while (*end == ' ')
        end--;

    int trimmed_len = strlen(start) - strlen(end) + 1;
    char *buf = ulm_new(char, trimmed_len + 1);
    strncpy(buf, start, trimmed_len);
    buf[trimmed_len] = '\0';
    return buf;
}

// Returns the number of leaves in the parse tree
int ParseTree::countLeaves(Node * n)
{
    if (n == NULL)
        return 0;
    Node **c = n->getChildren();
    if (c == NULL)
        return 1;
    else
        return countLeaves(c[0]) + countLeaves(c[1]);
}

// Finds the leaves of the parse tree and stores them
// in nlist.
void ParseTree::getLeaves(Node * n, Node ** nlist,
                          int start)
{
    if (n == NULL)
        return;
    if (start == 1)
        index = 0;

    Node **c = n->getChildren();
    if (c == NULL) {
        nlist[index] = n;
        index++;
    } else {
        getLeaves(c[0], nlist, 0);
        getLeaves(c[1], nlist, 0);
    }
}

// This method counts and compares the number of
// matching right and left delimiters in the expression.
// If the counts are not equal, it reports an error and
// returns -1.  If all (r, l) delimiter pairs have
// equal counts, it returns the number of pairs
//
// NOTE:  This method will not trap all errors, for
// example ({)}.
int ParseTree::delimCount(char *expr, char *delims)
{
    // report any errors with method's arguments
    if (expr == NULL) {
        fprintf(stderr, "Must include an expression to be checked!\n");
        correct = false;
        sprintf(errStr, "Must include an expression to be checked!\n");
        return -1;
    }
    if (delims == NULL)
        return 0;

    // set up local variables
    int expr_sz = strlen(expr);
    int num_delims = strlen(delims);

    // check the number of delimiters specified
    if (num_delims % 2 > 0) {
        fprintf(stderr, "Delimiters must be specified in pairs\n");
        sprintf(errStr, "Delimiters must be specified in pairs\n");
        correct = false;
        return -1;
    }
    // return zero if no delimiters are specified
    if (num_delims == 0)
        return 0;

    // run through expression, counting delimiters
    int pair_cnt = 0;
    for (int i = 0; i < num_delims - 1; i += 2) {
        char l_delim = delims[i];
        char r_delim = delims[i + 1];
        int l_cnt = 0;
        int r_cnt = 0;

        // count delimiters
        for (int j = 0; j < expr_sz; j++) {
            if (expr[j] == l_delim)
                l_cnt++;
            if (expr[j] == r_delim)
                r_cnt++;
        }

        // compare values
        if (l_cnt != r_cnt) {
            fprintf(stderr, "Mismatch in delimiters %c, %c\n",
                    l_delim, r_delim);
            sprintf(errStr, "Mismatch in delimiters %c, %c\n",
                    l_delim, r_delim);
            correct = false;
            return -1;
        } else if (l_delim == r_delim && l_cnt % 2 > 0) {
            fprintf(stderr, "Odd number of delimiters %c\n", l_delim);
            sprintf(errStr, "Odd number of delimiters %c\n", l_delim);
            correct = false;
            return -1;
        }
        if (l_delim == r_delim)
            pair_cnt += l_cnt / 2;
        else
            pair_cnt += l_cnt;
    }

    return pair_cnt;
}

// Creates a DelimPair object from the first pair l_delim,
// r_delim after offset in the expression expr.
DelimPair *ParseTree::findPair(char *expr, char l_delim,
                               char r_delim, int offset)
{
    // check for errors in arguments
    if (expr == NULL) {
        fprintf(stderr, "Must input an expression!\n");
        sprintf(errStr, "Must input an expression!\n");
        correct = false;
        return NULL;
    }
    if ((size_t) offset >= strlen(expr)) {
        return NULL;
    }
    // find first l_delim
    char *start = strchr(expr + offset, l_delim);
    if (start == NULL)
        return NULL;

    // calculate index
    int l_index = strlen(expr) - strlen(start);

    // find matching r_delim
    int delim_cnt = 1;
    int expr_sz = strlen(expr);
    int i;
    int r_index = -1;
    for (i = l_index + 1;
         i < expr_sz && delim_cnt > 0;
         i++) {
        if (expr[i] == r_delim)
            delim_cnt--;
        else if (expr[i] == l_delim)
            delim_cnt++;
        if (delim_cnt == 0)
            r_index = i;
    }

    // see if we've run off end of string
    if (r_index == -1) {
        fprintf(stderr, "No matching right delimiter for %c ", l_delim);
        fprintf(stderr, "at index %d in expression %s\n", l_index, expr);
        correct = false;
        sprintf(errStr, "No matching right delimiter for %c ", l_delim);
        sprintf(errStr, "at index %d in expression %s\n", l_index, expr);
        return NULL;
    }
    // create Pair object and return
    DelimPair *dp = new DelimPair();
    dp->l_index = l_index;
    dp->r_index = r_index;
    return dp;
}

// Checks to see if delimiters are nested properly,
// returns 1 for success, 0 for failure.
int ParseTree::pairTest(char *expr, char *delims)
{
    // count number of pairs and perform basic
    // error checks
    int num_pairs = delimCount(expr, delims);
    if (num_pairs == -1)
        return 0;
    else if (num_pairs == 0)
        return 1;

    // set up local variables
    int num_delims = strlen(delims);
    DelimPair **pairs = ulm_new(DelimPair *, num_pairs);

    // find all pairs
    int pair_cnt = 0;
    int i;
    for (i = 0; i < num_delims - 1; i += 2) {
        char l_delim = delims[i];
        char r_delim = delims[i + 1];
        DelimPair *dp;

        int offset = 0;
        while ((dp = findPair(expr, l_delim, r_delim, offset))
               != NULL) {
            pairs[pair_cnt] = dp;
            pair_cnt++;
            if (l_delim == r_delim) {
                offset += dp->r_index + 1;
            } else {
                offset += dp->l_index + 1;
            }
        }
    }

    // compare all pairs
    int passed = 1;
    for (i = 0; i < num_pairs && passed == 1; i++) {
        for (int j = i + 1; j < num_pairs; j++) {
            if (pairs[j]->l_index < pairs[i]->r_index &&
                pairs[j]->r_index > pairs[i]->r_index) {
                fprintf(stderr, "Delimiters are not nested!\n");
                fprintf(stderr, "Check pairs %d, %d %d, %d\n",
                        pairs[i]->l_index, pairs[i]->r_index,
                        pairs[j]->l_index, pairs[j]->r_index);
                sprintf(errStr, "Delimiters are not nested!\n");
                sprintf(errStr, "Check pairs %d, %d %d, %d\n",
                        pairs[i]->l_index, pairs[i]->r_index,
                        pairs[j]->l_index, pairs[j]->r_index);
                correct = false;
                passed = 0;
            }
        }
    }

    // delete all pairs
    for (i = 0; i < num_pairs; i++) {
        delete pairs[i];
    }
    ulm_delete(pairs);

    return passed;
}
