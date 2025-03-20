// Blashyrkh.maniac.coding
// BTC:1Maniaccv5vSQVuwrmRtfazhf2WsUJ1KyD DOGE:DManiac9Gk31A4vLw9fLN9jVDFAQZc2zPj

// Copyright (c) 2025-2026 Blashyrkh
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>


enum IOMode
{
    IO_AUTO,
    IO_LAZYK,
    IO_BLC,
    IO_BLC8
};

struct Node
{
    struct Node   *next;
    struct Node   *left;
    struct Node   *right;
    int          (*apply)(struct Node *a);
    unsigned int   special:1;
    unsigned int   mark:1;
    unsigned int   index:30;
};

struct ProtectedNode
{
    struct ProtectedNode *next;
    struct Node          *node;
};

#ifndef GC_EVERY
# define GC_EVERY 25000
#endif

// Global variables
// {
static struct ProtectedNode *protected_nodes=NULL;
static struct Node *special_node_list=NULL; // INPUT_CONT and INPUT_CHAR nodes, ".left" is used for chaining
static struct Node *all_node_list=NULL; // All nodes (including special ones), ".next" is used for chaining
static enum IOMode io_mode=IO_AUTO;
// }

static void reduce(struct Node *expr);

static int apply_I(struct Node *a);
static int apply_K(struct Node *a);
static int apply_Kx(struct Node *a);
static int apply_S(struct Node *a);
static int apply_Sx(struct Node *a);
static int apply_Sxy(struct Node *a);
static int apply_B(struct Node *a);
static int apply_Bx(struct Node *a);
static int apply_Bxy(struct Node *a);
static int apply_C(struct Node *a);
static int apply_Cx(struct Node *a);
static int apply_Cxy(struct Node *a);
static int apply_O(struct Node *a);
static int apply_Ox(struct Node *a);
static int apply_Wx(struct Node *a);
static int apply_KI(struct Node *a);
static int apply_KIx(struct Node *a);
static int apply_M(struct Node *a);
static int apply_T(struct Node *a);
static int apply_Tx(struct Node *a);
static int apply_BC(struct Node *a);
static int apply_BCx(struct Node *a);
static int apply_BCxy(struct Node *a);
static int apply_BCxyz(struct Node *a);
static int apply_V(struct Node *a);
static int apply_Vx(struct Node *a);
static int apply_Vxy(struct Node *a);
static int apply_Q3(struct Node *a);
static int apply_Q3x(struct Node *a);
static int apply_Q3xy(struct Node *a);
static int apply_D(struct Node *a);
static int apply_Dx(struct Node *a);
static int apply_Dxy(struct Node *a);
static int apply_Dxyz(struct Node *a);
static int apply_input_cont(struct Node *a);
static int apply_input_char(struct Node *a);
static int apply_output_cont(struct Node *a);
static int apply_output_stop(struct Node *a);
static int apply_atom_X(struct Node *a);
static int apply_atom_Y(struct Node *a);

static void mark_node(struct Node *node)
{
    if(node && !node->mark)
    {
        node->mark=1;
        if(!node->special)
        {
            mark_node(node->left);
            mark_node(node->right);
        }
    }
}

static void gc(void)
{
    // Mark all protected nodes and all their subtrees
    for(struct ProtectedNode *p=protected_nodes; p!=NULL; p=p->next)
        mark_node(p->node);

    struct Node **pp;

    // Unchain all unmarked special nodes
    pp=&special_node_list;
    while(*pp)
    {
        struct Node *p=*pp;
        if(!p->mark)
        {
            *pp=p->left;
        }
        else
        {
            pp=&p->left;
        }
    }

    unsigned int freed=0;
    unsigned int kept=0;

    // Unchain and free all unmarked nodes, unmark all marked nodes
    pp=&all_node_list;
    while(*pp)
    {
        struct Node *p=*pp;
        if(!p->mark)
        {
            *pp=p->next;
            free(p);
            ++freed;
        }
        else
        {
            p->mark=0;
            pp=&p->next;
            ++kept;
        }
    }
    //fprintf(stderr, "GC: %u freed, %u kept\n", freed, kept);
}

static inline struct Node *new_node(struct Node *left, struct Node *right, int (*apply)(struct Node *a), int special)
{
    struct Node *p=(struct Node *)malloc(sizeof(struct Node));
    p->next=all_node_list;
    p->left=left;
    p->right=right;
    p->apply=apply;
    p->special=special;
    p->mark=0;
    p->index=0;
    all_node_list=p;

    if(special)
    {
        p->left=special_node_list;
        special_node_list=p;
    }

    return p;
}

// Combinators. Statically allocated, not chained by ".next" and thus they can't become victims of GC.
static struct Node I={NULL, NULL, NULL, apply_I, 0, 0};
static struct Node K={NULL, NULL, NULL, apply_K, 0, 0};
static struct Node S={NULL, NULL, NULL, apply_S, 0, 0};
static struct Node B={NULL, NULL, NULL, apply_B, 0, 0};
static struct Node C={NULL, NULL, NULL, apply_C, 0, 0};
static struct Node O={NULL, NULL, NULL, apply_O, 0, 0};
static struct Node KI={NULL, NULL, NULL, apply_KI, 0, 0};
static struct Node M={NULL, NULL, NULL, apply_M, 0, 0};
static struct Node T={NULL, NULL, NULL, apply_T, 0, 0};
static struct Node BC={NULL, NULL, NULL, apply_BC, 0, 0};
static struct Node V={NULL, NULL, NULL, apply_V, 0, 0};
static struct Node Q3={NULL, NULL, NULL, apply_Q3, 0, 0};
static struct Node D={NULL, NULL, NULL, apply_D, 0, 0};

static inline struct Node *new_combinator(char ch)
{
    ch=tolower(ch);
    if(ch=='i')
        return &I;
    else if(ch=='k')
        return &K;
    else if(ch=='s')
        return &S;
    else if(ch=='b')
        return &B;
    else if(ch=='c')
        return &C;
    else
        return NULL;
}

static inline struct Node *new_application(struct Node *left, struct Node *right)
{
    return new_node(left, right, NULL, 0);
}

static inline struct Node *new_variable(unsigned int index)
{
    // Variable node is like an application node but with both 'left' and 'right' set to NULL.
    // 'index' is de Bruijn zero-based
    struct Node *p=new_node(NULL, NULL, NULL, 0);
    p->index=index;
    return p;
}

static inline void replace_application(struct Node *a, struct Node *left, struct Node *right)
{
    a->left=left;
    a->right=right;
}

static inline void replace_node(struct Node *a, const struct Node *source)
{
    a->left=source->left;
    a->right=source->right;
    a->apply=source->apply;
    a->special=source->special;
    if(a->special)
    {
        a->left=special_node_list;
        special_node_list=a;
    }
}

static inline struct Node *new_input_cont(void)
{
    return new_node(NULL, NULL, apply_input_cont, 1);
}

static inline struct Node *new_input_char(void)
{
    return new_node(NULL, NULL, apply_input_char, 1);
}

static inline struct Node *new_output_sink(void)
{
    return new_node(NULL, NULL, apply_output_cont, 0);
}

static inline struct Node *new_output_stop(void)
{
    return new_node(NULL, NULL, apply_output_stop, 0);
}

static inline struct Node *new_atom_X(void)
{
    return new_node(NULL, NULL, apply_atom_X, 0);
}

static inline struct Node *new_atom_Y(void)
{
    return new_node(NULL, NULL, apply_atom_Y, 0);
}

// Find all nodes, remove them from the hash table, chain together into single-linked list
// and return the head of the list (and in the darkness bind them, of course)
static struct Node *find_special_nodes(int (*apply)(struct Node *a))
{
    struct Node *res=NULL;

    struct Node **pp=&special_node_list;
    while(*pp!=NULL)
    {
        struct Node *p=*pp;
        if(p->apply==apply)
        {
            *pp=p->left;
            p->left=res;
            res=p;
        }
        else
        {
            pp=&p->left;
        }
    }
    return res;
}

static int apply_I(struct Node *a)
{
    replace_node(a, a->right);
    return 0;
}

static int apply_K(struct Node *a)
{
#ifdef OPTIMIZE_KI_RUN
    if(a->right->apply==apply_I)
    {
        a->apply=apply_KI;
        return 0;
    }
#endif
    a->apply=apply_Kx;
    return 0;
}

static int apply_Kx(struct Node *a)
{
    replace_node(a, a->left->right);
    return 0;
}

static int apply_S(struct Node *a)
{
#ifdef OPTIMIZE_SI_RUN
    if(a->right->apply==apply_I)
    {
        a->apply=apply_O;
        return 0;
    }
#endif
#ifdef OPTIMIZE_KI_RUN
    if(a->right->apply==apply_K)
    {
        a->apply=apply_KI;
        return 0;
    }
#endif

    a->apply=apply_Sx;
    return 0;
}

static int apply_Sx(struct Node *a)
{
#ifdef OPTIMIZE_SxI_RUN
    if(a->right->apply==apply_I)
    {
        a->apply=apply_Wx;
        return 0;
    }
#endif

    a->apply=apply_Sxy;
    return 0;
}


static int apply_Sxy(struct Node *a)
{
    replace_application(a, new_application(a->left->left->right, a->right), new_application(a->left->right, a->right));
    return 0;
}

static int apply_B(struct Node *a)
{
#ifdef OPTIMIZE_BC_RUN
    if(a->right->apply==apply_C)
    {
        a->apply=apply_BC;
        return 0;
    }
#endif
#ifdef OPTIMIZE_Q3_RUN
    if(a->right->apply==apply_T)
    {
        a->apply=apply_Q3;
        return 0;
    }
#endif
#ifdef OPTIMIZE_D_RUN
    if(a->right->apply==apply_B)
    {
        a->apply=apply_D;
        return 0;
    }
#endif

    a->apply=apply_Bx;
    return 0;
}

static int apply_Bx(struct Node *a)
{
#ifdef OPTIMIZE_V_RUN
    if(a->left->right->apply==apply_C && a->right->apply==apply_T)
    {
        a->apply=apply_V;
        return 0;
    }
#endif

    a->apply=apply_Bxy;
    return 0;
}

static int apply_Bxy(struct Node *a)
{
    replace_application(a, a->left->left->right, new_application(a->left->right, a->right));
    return 0;
}

static int apply_C(struct Node *a)
{
#ifdef OPTIMIZE_CI_RUN
    if(a->right->apply==apply_I)
    {
        a->apply=apply_T;
        return 0;
    }
#endif

    a->apply=apply_Cx;
    return 0;
}

static int apply_Cx(struct Node *a)
{
    a->apply=apply_Cxy;
    return 0;
}

static int apply_Cxy(struct Node *a)
{
    replace_application(a, new_application(a->left->left->right, a->right), a->left->right);
    return 0;
}

static int apply_O(struct Node *a)
{
#ifdef OPTIMIZE_M_RUN
    if(a->right->apply==apply_I)
    {
        a->apply=apply_M;
        return 0;
    }
#endif
    a->apply=apply_Ox;
    return 0;
}

static int apply_Ox(struct Node *a)
{
    replace_application(a, a->right, new_application(a->left->right, a->right));
    return 0;
}

static int apply_Wx(struct Node *a)
{
    replace_application(a, new_application(a->left->left->right, a->right), a->right);
    return 0;
}

static int apply_KI(struct Node *a)
{
    a->apply=apply_KIx;
    return 0;
}

static int apply_KIx(struct Node *a)
{
    replace_node(a, a->right);
    return 0;
}

static int apply_M(struct Node *a)
{
    a->left=a->right;
    return 0;
}

static int apply_T(struct Node *a)
{
    a->apply=apply_Tx;
    return 0;
}

static int apply_Tx(struct Node *a)
{
    replace_application(a, a->right, a->left->right);
    return 0;
}

static int apply_BC(struct Node *a)
{
#ifdef OPTIMIZE_V_RUN
    if(a->right->apply==apply_T)
    {
        a->apply=apply_V;
        return 0;
    }
#endif
    a->apply=apply_BCx;
    return 0;
}

static int apply_BCx(struct Node *a)
{
    a->apply=apply_BCxy;
    return 0;
}

static int apply_BCxy(struct Node *a)
{
    a->apply=apply_BCxyz;
    return 0;
}

static int apply_BCxyz(struct Node *a)
{
    replace_application(a, new_application(new_application(a->left->left->left->right, a->left->left->right), a->right), a->left->right);
    return 0;
}

static int apply_V(struct Node *a)
{
    a->apply=apply_Vx;
    return 0;
}

static int apply_Vx(struct Node *a)
{
    a->apply=apply_Vxy;
    return 0;
}

static int apply_Vxy(struct Node *a)
{
    replace_application(a, new_application(a->right, a->left->left->right), a->left->right);
    return 0;
}

static int apply_Q3(struct Node *a)
{
    a->apply=apply_Q3x;
    return 0;
}

static int apply_Q3x(struct Node *a)
{
    a->apply=apply_Q3xy;
    return 0;
}

static int apply_Q3xy(struct Node *a)
{
    replace_application(a, a->right, new_application(a->left->left->right, a->left->right));
    return 0;
}

static int apply_D(struct Node *a)
{
    a->apply=apply_Dx;
    return 0;
}

static int apply_Dx(struct Node *a)
{
    a->apply=apply_Dxy;
    return 0;
}

static int apply_Dxy(struct Node *a)
{
    a->apply=apply_Dxyz;
    return 0;
}

static int apply_Dxyz(struct Node *a)
{
    replace_application(a, new_application(a->left->left->left->right, a->left->left->right), new_application(a->left->right, a->right));
    return 0;
}

static int apply_input_cont(struct Node *a)
{
    struct Node *node=find_special_nodes(apply_input_cont);
    if(!node)
        return 0;

    switch(io_mode)
    {
    case IO_LAZYK:
        {
            struct Node *left=new_application(&C, new_application(&T, new_input_char()));
            struct Node *right=new_input_cont();

            while(node)
            {
                struct Node *next=node->left;

                node->left=left;
                node->right=right;
                node->apply=NULL;
                node->special=0;

                node=next;
            }
        }
        break;

    case IO_BLC:
        // TODO
        break;

    case IO_BLC8:
        // TODO
        break;

    }

    return 0;
}

static int apply_input_char(struct Node *a)
{
    struct Node *node=find_special_nodes(apply_input_char);
    if(!node)
        return 0;

    int code=fgetc(stdin);
    if(code<0 || code>255)
        code=256;

    struct Node *donor=new_application(&K, &I);
    for(int i=0; i<code; ++i)
    {
        donor=new_application(new_application(&S, &B), donor);
    }

    while(node)
    {
        struct Node *next=node->left;

        node->left=donor->left;
        node->right=donor->right;
        node->apply=donor->apply;
        node->special=0;

        node=next;
    }

    return 0;
}

static int apply_output_cont(struct Node *a)
{
    switch(io_mode)
    {
    case IO_LAZYK:
        {
            struct Node *n=new_application(new_application(a->right, new_atom_X()), new_atom_Y());
            reduce(n);

            unsigned int code=0;
            while(!n->apply && n->right->apply==apply_atom_X && code<256+126)
            {
                ++code;
                n=n->left;
            }
            if(n->apply!=apply_atom_Y)
                code=256+126;

            if(code<256)
            {
                fputc(code, stdout);
                fflush(stdout);
            }
            else
            {
                exit(code-256);
            }

            // Should return CI<OUT> to continue
            replace_application(a, &T, new_output_sink());
        }
        break;

    case IO_BLC:
        {
            struct Node *n=new_application(new_application(a->right, new_atom_X()), new_atom_Y());
            reduce(n);

            if(n->apply==apply_atom_X)
                fputc('0', stdout);
            else if(n->apply==apply_atom_Y)
                fputc('1', stdout);
            else
                exit(126);
            fflush(stdout);

            // lambda x . x <OUT> <STOP> = C(CI<OUT>)<STOP>
            replace_application(
                a,
                new_application(&C, new_application(&T, new_output_sink())),
                new_output_stop());
        }
        break;

    case IO_BLC8:
        abort();
        // TODO!
        break;

    }
    return 0;
}

static int apply_output_stop(struct Node *a)
{
    exit(0);
}

static int apply_atom_X(struct Node *a)
{
    replace_application(a, a->right, a->left);
    return 0;
}

static int apply_atom_Y(struct Node *a)
{
    return 1;
}

static struct Node *new_application_load(struct Node *left, struct Node *right)
{
#ifdef OPTIMIZE_SI_LOAD
    if(left->apply==apply_S && right->apply==apply_I)
        return &O;
#endif

#ifdef OPTIMIZE_KI_LOAD
    if((left->apply==apply_K && right->apply==apply_I) || (left->apply==apply_S && right->apply==apply_K))
        return &KI;
#endif

#ifdef OPTIMIZE_M_LOAD
    if(left->apply==apply_O && right->apply==apply_I)
        return &M;
#endif

#ifdef OPTIMIZE_CI_LOAD
    if(left->apply==apply_C && right->apply==apply_I)
        return &T;
#endif

#ifdef OPTIMIZE_BC_LOAD
    if(left->apply==apply_B && right->apply==apply_C)
        return &BC;
#endif

#ifdef OPTIMIZE_V_LOAD
//    if(!left->apply && left->left && left->right && left->left->apply==apply_B && left->right->apply==apply_C && right->apply==apply_T)
//        return &V;
    if(left->apply==apply_BC && right->apply==apply_T)
        return &V;
#endif

#ifdef OPTIMIZE_Q3_LOAD
    if(left->apply==apply_B && right->apply==apply_T)
        return &Q3;
#endif

#ifdef OPTIMIZE_D_LOAD
    if(left->apply==apply_B && right->apply==apply_B)
        return &D;
#endif

    return new_application(left, right);
}

// TODO: return parsing error info (line, col, message)
static struct Node *parse_lazyk_program(FILE *f)
{
    unsigned int line=1;
    unsigned int col=0;
    int ignore_rest_of_line=0;

    unsigned int op_stack_size=0;
    unsigned int op_stack_cap=0;
    struct Node **op_stack=NULL;

    unsigned int n_stack_size=0;
    unsigned int n_stack_cap=10;
    unsigned int *n_stack=(unsigned int *)malloc(n_stack_cap*sizeof(unsigned int));
    n_stack[n_stack_size++]=0;

    int ch;
    while((ch=fgetc(f))!=EOF)
    {
        if(ch=='\n')
        {
            ++line;
            col=0;
            ignore_rest_of_line=0;
            continue;
        }

        ++col;
        if(ignore_rest_of_line)
            continue;

        if(ch=='#')
            ignore_rest_of_line=1;
        else if(ch=='`')
        {
            if(n_stack_size>=n_stack_cap)
            {
                n_stack_cap+=10;
                n_stack=(unsigned int *)realloc(n_stack, n_stack_cap*sizeof(unsigned int));
                if(!n_stack)
                    abort();
            }
            n_stack[n_stack_size++]=0;
        }
        else if(ch=='i' || ch=='k' || ch=='s' || ch=='b' || ch=='c') // extended Lazy K - B and C combinators are added
        {
            if(op_stack_size>=op_stack_cap)
            {
                op_stack_cap+=20;
                op_stack=(struct Node **)realloc(op_stack, op_stack_cap*sizeof(struct Node *));
                if(!op_stack)
                    abort();
            }
            op_stack[op_stack_size++]=new_combinator(ch);
            ++n_stack[n_stack_size-1];

            while(n_stack_size>0 && n_stack[n_stack_size-1]==2)
            {
                struct Node *left=op_stack[op_stack_size-2];
                struct Node *right=op_stack[op_stack_size-1];

                --n_stack_size;
                ++n_stack[n_stack_size-1];
                --op_stack_size;

                op_stack[op_stack_size-1]=new_application_load(left, right);
            }
        }
    }
    // TODO: checks

    struct Node *res=NULL;
    if(op_stack_size==1)
        res=op_stack[0];

    free(op_stack);
    free(n_stack);

    return res;
}

static inline int IS_A(const struct Node *p)
{
    return !p->apply && p->left && p->right;
}

static inline int IS_VAR(const struct Node *p)
{
    return !p->apply && !p->left && !p->right;
}

static int IS_COMB(const struct Node *p)
{
    if(IS_A(p))
    {
        return IS_COMB(p->left) && IS_COMB(p->right);
    }
    else
    {
        return !IS_VAR(p);
    }
}

static inline int get_non_ws_char(FILE *f)
{
    for(;;)
    {
        int ch=fgetc(f);
        if(ch==EOF || !isspace(ch))
            return ch;
    }
}

static void adjust_variables(struct Node *p)
{
    if(IS_A(p))
    {
        adjust_variables(p->left);
        adjust_variables(p->right);
    }
    else if(IS_VAR(p))
    {
        assert(p->index>0);
        --p->index;
    }
}

static int contains_variable_zero(const struct Node *p)
{
    if(IS_A(p))
    {
        return contains_variable_zero(p->left) || contains_variable_zero(p->right);
    }
    else if(IS_VAR(p))
    {
        return p->index==0;
    }
    else
    {
        return 0;
    }
}

static int is_same(const struct Node *p, const struct Node *q)
{
    if(IS_A(p) && IS_A(q))
    {
        return is_same(p->left, q->left) && is_same(p->right, q->right);
    }
    else if(IS_VAR(p) && IS_VAR(q))
    {
        return p->index==q->index;
    }
    else
    {
        return p->apply!=NULL && q->apply!=NULL && p->apply==q->apply;
    }
}

static struct Node *eliminate_lambda(struct Node *p)
{
    // It uses some Tromp rules as well as some others

    // Tromp rule 1: lambda x . S K y -> S K
    if(IS_A(p) && IS_A(p->left) && p->left->left->apply==&apply_S && p->left->right->apply==&apply_K)
    {
        return new_application_load(&S, &K);
    }
    // Common rule 2: lambda x . M (where M doesn't contain variable 0) -> K M
    else if(!contains_variable_zero(p))
    {
        return new_application_load(&K, p);
    }
    // Common rule 3: lambda x . x -> I
    else if(IS_VAR(p) && p->index==0)
    {
        return &I;
    }
    // Common rule 4: lambda x . M x (where M doesn't contain variable 0) -> M
    else if(IS_A(p) && !contains_variable_zero(p->left) && IS_VAR(p->right) && p->right->index==0)
    {
        return p->left;
    }

    // Tromp rule 5: lambda x . x M x -> S S K x M
    // Not sure about its usefullness
    else if(IS_A(p) && IS_A(p->left) && IS_VAR(p->left->left) && p->left->left->index==0 && IS_VAR(p->right) && p->right->index==0)
    {
        return eliminate_lambda(
            new_application_load(
                new_application_load(
                    new_application_load(
                        new_application_load(&S, &S),
                        &K),
                    p->left->left),
                p->left->right));
    }

/*
    // BAD RULE!
    // Tromp rule 6: lambda x . M (N L) -> lambda x . S (K M) N L  (M & N do not contain variable 0 or all variables at all???)
    else if(IS_A(p) && IS_A(p->right) && IS_COMB(p->left) && IS_COMB(p->right->left))
    {
        return eliminate_lambda(
            new_application_load(
                new_application_load(
                    new_application_load(
                        &S,
                        eliminate_lambda(p->left)),
                    p->right->left),
                p->right->right));
    }
*/

    // Tromp rule 7: lambda x . M N L -> lambda x . S M (K L) N  (M & L are combinators)
    else if(IS_A(p) && IS_A(p->left) && IS_COMB(p->left->left) && IS_COMB(p->right))
    {
        return eliminate_lambda(
            new_application_load(
                new_application_load(
                    new_application_load(&S, p->left->left),
                    eliminate_lambda(p->right)),
                p->left->right));
    }

    // Tromp rule 8: lambda x . (M L) (N L) -> lambda x . (S M N) L -> B (S M N) (lambda x . L)
    else if(IS_A(p) && IS_A(p->left) && IS_A(p->right) && IS_COMB(p->left->left) && IS_COMB(p->right->left) && is_same(p->left->right, p->right->right))
    {
        return eliminate_lambda(
            new_application_load(
                new_application_load(
                    new_application_load(&S, p->left->left),
                    p->right->left),
                p->left->right));
    }

    // Rule for B combinator
    else if(IS_A(p) && !contains_variable_zero(p->left) && contains_variable_zero(p->right))
    {
        return new_application_load(new_application_load(&B, p->left), eliminate_lambda(p->right));
    }

    // Rule for C combinator
    else if(IS_A(p) && contains_variable_zero(p->left) && !contains_variable_zero(p->right))
    {
        return new_application_load(new_application_load(&C, eliminate_lambda(p->left)), p->right);
    }

    // Fallback rule: lambda x . M N -> S (lambda x . M) (lambda x . N)
    else
    {
        return new_application_load(new_application_load(&S, eliminate_lambda(p->left)), eliminate_lambda(p->right));
    }
}

static struct Node *parse_blc_program(FILE *f, unsigned int lambda_depth)
{
    int ch=get_non_ws_char(f);
    if(ch=='0')
    {
        ch=get_non_ws_char(f);
        if(ch=='0')
        {
            struct Node *p=parse_blc_program(f, lambda_depth+1);
            if(!p)
                return NULL;

            p=eliminate_lambda(p);
            adjust_variables(p);

            return p;
        }
        else if(ch=='1')
        {
            struct Node *left=parse_blc_program(f, lambda_depth);
            if(!left)
                return NULL;
            struct Node *right=parse_blc_program(f, lambda_depth);
            if(!right)
                return NULL;
            return new_application_load(left, right);
        }
        else
        {
            return NULL;
        }
    }
    else if(ch=='1')
    {
        unsigned int index=0;
        while((ch=get_non_ws_char(f))=='1')
        {
            ++index;
        }
        if(ch!='0' || index>=lambda_depth)
        {
            return NULL;
        }

        return new_variable(index);
    }
    else
    {
        return NULL;
    }
}

static inline const char *comb(struct Node *p)
{
    if(p->apply==apply_I)
        return "I";
    else if(p->apply==apply_K)
        return "K";
    else if(p->apply==apply_Kx)
        return "Kx";
    else if(p->apply==apply_S)
        return "S";
    else if(p->apply==apply_Sx)
        return "Sx";
    else if(p->apply==apply_Sxy)
        return "Sxy";
    else if(p->apply==apply_B)
        return "B";
    else if(p->apply==apply_Bx)
        return "Bx";
    else if(p->apply==apply_Bxy)
        return "Bxy";
    else if(p->apply==apply_C)
        return "C";
    else if(p->apply==apply_Cx)
        return "Cx";
    else if(p->apply==apply_Cxy)
        return "Cxy";
    else if(p->apply==apply_O)
        return "O";
    else if(p->apply==apply_Ox)
        return "Ox";
    else if(p->apply==apply_Wx)
        return "Wx";
    else if(p->apply==apply_KI)
        return "Ki";
    else if(p->apply==apply_KIx)
        return "Kix";
    else if(p->apply==apply_M)
        return "M";
    else if(p->apply==apply_T)
        return "T";
    else if(p->apply==apply_Tx)
        return "Tx";
    else if(p->apply==apply_BC)
        return "BC";
    else if(p->apply==apply_BCx)
        return "BCx";
    else if(p->apply==apply_BCxy)
        return "BCxy";
    else if(p->apply==apply_BCxyz)
        return "BCxyz";
    else if(p->apply==apply_V)
        return "V";
    else if(p->apply==apply_Vx)
        return "Vx";
    else if(p->apply==apply_Vxy)
        return "Vxy";
    else if(p->apply==apply_Q3)
        return "Q3";
    else if(p->apply==apply_Q3x)
        return "Q3x";
    else if(p->apply==apply_Q3xy)
        return "Q3xy";
    else if(p->apply==apply_D)
        return "D";
    else if(p->apply==apply_Dx)
        return "Dx";
    else if(p->apply==apply_Dxy)
        return "Dxy";
    else if(p->apply==apply_Dxyz)
        return "Dxyz";
    else
        return NULL;
}

static void reduce(struct Node *p)
{
    // If the expression's root node is not Application then there's nothing we can do
    // to reduce it
    if(p->apply)
        return;

    // Protect root node (and, hence, all the tree) from GC
    struct ProtectedNode prot;
    prot.next=protected_nodes;
    prot.node=p;
    protected_nodes=&prot;

    unsigned int stack_size=0;
    unsigned int stack_cap=0;
    struct Node **stack=NULL;

    int applies=0;
    while(!p->apply || stack_size>0)
    {
        if(!p->apply)
        {
            if(!p->left->apply)
            {
                if(stack_size==stack_cap)
                {
                    stack_cap+=100;
                    stack=(struct Node **)realloc(stack, stack_cap*sizeof(struct Node *));
                    if(!stack)
                        abort();
                }
                stack[stack_size++]=p;
                p=p->left;
            }
            else
            {
#ifdef PROFILE_APPLICATION
                const char *s1=comb(p->left);
                const char *s2=comb(p->right);
                if(s1 && s2)
                    fprintf(stderr, "%s%s\n", s1, s2);
#endif
                if(p->left->apply(p))
                    break;
            }

            ++applies;
            if(applies==GC_EVERY)
            {
                gc();
                applies=0;
            }
        }
        else
        {
            p=stack[--stack_size];
        }
    }

    protected_nodes=prot.next;

    free(stack);
}

static inline int is_lazyk_source(const char *filename)
{
    char *p=strrchr(filename, '.');
    return p && strcmp(p, ".lazy")==0;
}

static inline int is_blc_source(const char *filename)
{
    char *p=strrchr(filename, '.');
    return p && strcmp(p, ".blc")==0;
}

static inline int is_blc8_source(const char *filename)
{
    char *p=strrchr(filename, '.');
    return p && strcmp(p, ".blc8")==0;
}


int main(int argc, char *argv[])
{
    for(int i=1; i<argc; ++i)
    {
        enum IOMode file_io_mode=IO_AUTO;

        if(is_lazyk_source(argv[i]))
            file_io_mode=IO_LAZYK;
        else if(is_blc_source(argv[i]))
            file_io_mode=IO_BLC;
        else if(is_blc8_source(argv[i]))
            file_io_mode=IO_BLC8;
        else
        {
            fprintf(stderr, "Can't determine program source language (Lazy K, BLC, BLC8): `%s'\n", argv[i]);
            return 1;
        }
        if(io_mode!=IO_AUTO && file_io_mode!=io_mode)
        {
            fprintf(stderr, "Can't run a batch of programs with different IO modes\n");
            return 1;
        }

        io_mode=file_io_mode;
    }

    if(io_mode==IO_AUTO)
        io_mode=IO_LAZYK;

    struct Node *program=new_input_cont();
    for(int i=1; i<argc; ++i)
    {
        FILE *f=fopen(argv[i], "rt");
        if(!f)
        {
            perror("Failed to open source file");
            return 1;
        }

        struct Node *node;
        if(is_lazyk_source(argv[i]))
        {
            node=parse_lazyk_program(f);
        }
        else if(is_blc_source(argv[i]))
        {
            node=parse_blc_program(f, 0);
        }
        // TODO: else


        fclose(f);


        if(!node)
        {
            fprintf(stderr, "Failed to parse source file\n");
            return 1;
        }

        program=new_application(node, program);
    }

    program=new_application(program, new_output_sink());
    if(io_mode==IO_BLC || io_mode==IO_BLC8)
    {
        program=new_application(program, new_output_stop());
    }

    reduce(program);

    return 126;
}
