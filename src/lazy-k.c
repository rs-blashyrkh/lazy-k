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

enum OpCode
{
    OP_APPLY = 0x00,
    OP_BC    = 0x08,
    OP_BCx   = 0x04,
    OP_BCxy  = 0x02,
    OP_BCxyz = 0x01,
    OP_S     = 0x020C,
    OP_Sx    = 0x0106,
    OP_Sxy   = 0x0083,
    OP_B     = 0x14,
    OP_Bx    = 0x0A,
    OP_Bxy   = 0x05,
    OP_C     = 0x1C,
    OP_Cx    = 0x0E,
    OP_Cxy   = 0x07,
    OP_V     = 0x24,
    OP_Vx    = 0x12,
    OP_Vxy   = 0x09,
    OP_K     = 0x16,
    OP_Kx    = 0x0B,
    OP_O     = 0x1A,
    OP_Ox    = 0x0D,
    OP_W     = 0x1E,
    OP_Wx    = 0x0F,
    OP_T     = 0x22,
    OP_Tx    = 0x11,
    OP_I     = 0xF013,
    OP_M     = 0x15,
    OP_OUT_C = 0x17,
    OP_IN_C  = 0x19, // 11001  |
    OP_IN_V  = 0x1B, // 11011  |__ 110x1
    OP_OUT_S = 0x1D,
    OP_Z     = 0x1F,
    OP_X     = 0x10, // Stoppers must end with 0000
    OP_Y     = 0x20,
};

struct Node
{
    struct Node   *next;
    struct Node   *left;
    struct Node   *right;
    unsigned int   opcode:31;
    unsigned int   mark:1;
};

static inline int is_special(struct Node *p)
{
    return (p->opcode&0x3D)==0x19;
}

#ifndef GC_EVERY
# define GC_EVERY 25000
#endif

struct ProtectedNode
{
    struct ProtectedNode *next;
    struct Node *node;
};

// Global variables
// {
struct ProtectedNode *protected_nodes=NULL;
static struct Node *special_node_list=NULL; // INPUT_CONT and INPUT_VAL nodes, ".left" is used for chaining
static struct Node *all_node_list=NULL; // All nodes (including special ones), ".next" is used for chaining
static enum IOMode io_mode=IO_AUTO;
// }

static void reduce(struct Node *expr);

typedef void (*apply_fn)(struct Node *);

static void apply_BCxyz(struct Node *a);
static void apply_Sxy(struct Node *a);
static void apply_Bxy(struct Node *a);
static void apply_Cxy(struct Node *a);
static void apply_Vxy(struct Node *a);
static void apply_Kx(struct Node *a);
static void apply_Ox(struct Node *a);
static void apply_Wx(struct Node *a);
static void apply_Tx(struct Node *a);
static void apply_I(struct Node *a);
static void apply_M(struct Node *a);
static void apply_output_cont(struct Node *a);
static void apply_output_stop(struct Node *a);
static void apply_input_cont(struct Node *a);
static void apply_input_val(struct Node *a);
static void apply_atom_Z(struct Node *a);

static const apply_fn apply_functions[]=
{
    apply_BCxyz,
    apply_Sxy,
    apply_Bxy,
    apply_Cxy,
    apply_Vxy,
    apply_Kx,
    apply_Ox,
    apply_Wx,
    apply_Tx,
    apply_I,
    apply_M,
    apply_output_cont,
    apply_input_cont,
    apply_input_val,
    apply_output_stop,
    apply_atom_Z,
};

static void mark_node(struct Node *node)
{
    if(node && !node->mark)
    {
        node->mark=1;
        if(!is_special(node)) // 'left' has different meaning for special nodes
        {
            mark_node(node->left);
            mark_node(node->right);
        }
    }
}

static void gc(void)
{
    // Mark all protected nodes and their children
    for(struct ProtectedNode *pn=protected_nodes; pn!=NULL; pn=pn->next)
        mark_node(pn->node);

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
    //fprintf(stderr, "GC: %u freed, %u kept, sp=%u\n", freed, kept, stack_size-1);
}

static inline struct Node *new_node(struct Node *left, struct Node *right, unsigned int opcode)
{
    struct Node *p=(struct Node *)malloc(sizeof(struct Node));
    p->next=all_node_list;
    p->left=left;
    p->right=right;
    p->opcode=opcode;
    p->mark=0;
    all_node_list=p;

    if(is_special(p))
    {
        p->left=special_node_list;
        special_node_list=p;
    }

    return p;
}

// Combinators. Statically allocated, not chained by ".next" and thus they can't become victims of GC.
static struct Node I={NULL, NULL, NULL, OP_I, 0};
static struct Node K={NULL, NULL, NULL, OP_K, 0};
static struct Node S={NULL, NULL, NULL, OP_S, 0};
static struct Node B={NULL, NULL, NULL, OP_B, 0};
static struct Node C={NULL, NULL, NULL, OP_C, 0};
static struct Node O={NULL, NULL, NULL, OP_O, 0};
static struct Node KI={NULL, &K, &I, OP_APPLY, 0};
static struct Node M={NULL, NULL, NULL, OP_M, 0};
static struct Node T={NULL, NULL, NULL, OP_T, 0};
static struct Node BC={NULL, NULL, NULL, OP_BC, 0};
static struct Node V={NULL, NULL, NULL, OP_V, 0};

static struct Node X={NULL, NULL, NULL, OP_X, 0};
static struct Node Y={NULL, NULL, NULL, OP_Y, 0};
static struct Node Z={NULL, NULL, NULL, OP_Z, 0};


static inline struct Node *new_combinator(char ch)
{
    ch=tolower(ch);
    if(ch=='i')
        return &I;
    else if(ch=='k')
        return &K;
    else if(ch=='s')
        return &S;
    else
        return NULL;
}

static inline struct Node *new_application(struct Node *left, struct Node *right)
{
    return new_node(left, right, OP_APPLY);
}

static inline struct Node *new_variable(unsigned int index)
{
    // Variables are temporary nodes existing before bracket abstraction is finished
    // Variable node OPCODE is (index+1)<<8
    return new_node(NULL, NULL, (index+1)<<8);
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
    a->opcode=source->opcode;
    if(is_special(a))
    {
        a->left=special_node_list;
        special_node_list=a;
    }
}

static inline struct Node *new_input_cont(void)
{
    return new_node(NULL, NULL, OP_IN_C);
}

static inline struct Node *new_input_value(void)
{
    return new_node(NULL, NULL, OP_IN_V);
}

static inline struct Node *new_output_sink(void)
{
    return new_node(NULL, NULL, OP_OUT_C);
}

static inline struct Node *new_output_stop(void)
{
    return new_node(NULL, NULL, OP_OUT_S);
}

// Find all nodes, remove them from the hash table, chain together into single-linked list
// and return the head of the list (and in the darkness bind them, of course)
static struct Node *find_special_nodes(unsigned int opcode)
{
    struct Node *res=NULL;

    struct Node **pp=&special_node_list;
    while(*pp!=NULL)
    {
        struct Node *p=*pp;
        if(p->opcode==opcode)
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

static void apply_BCxyz(struct Node *a)
{
    replace_application(a, new_application(new_application(a->left->left->left->right, a->left->left->right), a->right), a->left->right);
}

static void apply_Sxy(struct Node *a)
{
    replace_application(a, new_application(a->left->left->right, a->right), new_application(a->left->right, a->right));
}

static void apply_Bxy(struct Node *a)
{
    replace_application(a, a->left->left->right, new_application(a->left->right, a->right));
}

static void apply_Cxy(struct Node *a)
{
    replace_application(a, new_application(a->left->left->right, a->right), a->left->right);
}

static void apply_Vxy(struct Node *a)
{
    replace_application(a, new_application(a->right, a->left->left->right), a->left->right);
}

static void apply_Kx(struct Node *a)
{
    replace_node(a, a->left->right);
}

static void apply_Ox(struct Node *a)
{
    replace_application(a, a->right, new_application(a->left->right, a->right));
}

static void apply_Wx(struct Node *a)
{
    replace_application(a, new_application(a->left->left->right, a->right), a->right);
}

static void apply_Tx(struct Node *a)
{
    replace_application(a, a->right, a->left->right);
}

static void apply_I(struct Node *a)
{
    replace_node(a, a->right);
}

static void apply_M(struct Node *a)
{
    a->left=a->right;
}

static void apply_output_cont(struct Node *a)
{
    switch(io_mode)
    {
    case IO_LAZYK:
        {
            struct Node *n=new_application(new_application(a->right, &Z), &X);
            reduce(n);

            unsigned int code=0;
            while(n->opcode==OP_APPLY && n->right->opcode==OP_Z && code<256+126)
            {
                ++code;
                n=n->left;
            }
            if(n->opcode!=OP_X)
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
            struct Node *n=new_application(new_application(a->right, &X), &Y);
            reduce(n);

            if(n->opcode==OP_X)
                fputc('0', stdout);
            else if(n->opcode==OP_Y)
                fputc('1', stdout);
            else
                exit(126);
            fflush(stdout);

            // lambda x . x <OUT> <STOP> = C(CI<OUT>)<STOP>
            replace_application(
                a,
                new_application(&V, new_output_sink()),
                new_output_stop());
        }
        break;

    case IO_BLC8:
        {
            unsigned int code=0;
            struct Node *l=a->right;
            for(int i=0; i<8; ++i)
            {
                struct Node *n=new_application(l, new_application(new_application(&V, &X), &Y));
                reduce(n);
                if(n->left->opcode==OP_X)
                {
                    code=2*code;
                }
                else if(n->left->opcode==OP_Y)
                {
                    code=2*code+1;
                }
                else
                {
                    exit(126);
                }
                l=n->right;
            }
            fputc(code, stdout);
            fflush(stdout);

            replace_application(
                a,
                new_application(&V, new_output_sink()),
                new_output_stop());
        }
        break;

    }
}

static void apply_output_stop(struct Node *a)
{
    exit(0);
}

static void apply_atom_Z(struct Node *a)
{
    replace_application(a, a->right, a->left);
}

static void apply_input_cont(struct Node *a)
{
    struct Node *node=find_special_nodes(OP_IN_C);
    if(!node)
        return;

    switch(io_mode)
    {
    case IO_LAZYK:
        {
            struct Node *left=new_application(&V, new_input_value());
            struct Node *right=new_input_cont();

            while(node)
            {
                struct Node *next=node->left;

                node->left=left;
                node->right=right;
                node->opcode=OP_APPLY;

                node=next;
            }
        }
        break;

    case IO_BLC:
        {
            int code;
            for(;;)
            {
                code=fgetc(stdin);
                if(code=='0' || code=='1' || code==EOF)
                    break;
            }

            struct Node *left=NULL;
            struct Node *right=NULL;

            if(code==EOF)
            {
                left=&K;
                right=&I;
            }
            else
            {
                left=new_application(&V, code=='0' ? &K : &KI);
                right=new_input_cont();
            }

            while(node)
            {
                struct Node *next=node->left;

                node->left=left;
                node->right=right;
                node->opcode=OP_APPLY;

                node=next;
            }
        }
        break;

    case IO_BLC8:
        {
            int code=fgetc(stdin);

            struct Node *left=NULL;
            struct Node *right=NULL;

            if(code==EOF)
            {
                left=&K;
                right=&I;
            }
            else
            {
                struct Node *p=&KI;
                for(int i=0; i<8; ++i, code>>=1)
                {
                    p=new_application(new_application(&V, (code&1) ? &KI : &K), p);
                }

                left=new_application(&V, p);
                right=new_input_cont();
            }

            while(node)
            {
                struct Node *next=node->left;

                node->left=left;
                node->right=right;
                node->opcode=OP_APPLY;

                node=next;
            }
        }
        break;
    }
}

static void apply_input_val(struct Node *a)
{
    struct Node *node=find_special_nodes(OP_IN_V);
    if(!node)
        return;

    int code=fgetc(stdin);
    if(code<0 || code>255)
        code=256;

    struct Node *donor=&KI;
    for(int i=0; i<code; ++i)
    {
        donor=new_application(new_application(&S, &B), donor);
    }

    while(node)
    {
        struct Node *next=node->left;

        node->left=donor->left;
        node->right=donor->right;
        node->opcode=donor->opcode;

        node=next;
    }
}

static struct Node *new_application_load(struct Node *left, struct Node *right)
{
#ifdef OPTIMIZE_SI_LOAD
    if(left->opcode==OP_S && right->opcode==OP_I)
        return &O;
#endif

#ifdef OPTIMIZE_SxI_LOAD
    if(left->opcode==OP_APPLY && left->left->opcode==OP_S && right->opcode==OP_I)
        return new_node(left, right, OP_Wx);
#endif

#ifdef OPTIMIZE_M_LOAD
    if(left->opcode==OP_O && right->opcode==OP_I)
        return &M;
#endif

#ifdef OPTIMIZE_CI_LOAD
    if(left->opcode==OP_C && right->opcode==OP_I)
        return &T;
#endif

#ifdef OPTIMIZE_BC_LOAD
    if(left->opcode==OP_B && right->opcode==OP_C)
        return &BC;
#endif

#ifdef OPTIMIZE_V_LOAD
    if(left->opcode==OP_BC && right->opcode==OP_T)
        return &V;
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
        else if(ch=='i' || ch=='k' || ch=='s')
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
    return p->opcode==OP_APPLY;
}

static inline int IS_VAR(const struct Node *p)
{
    return p->opcode>0 && (p->opcode&0xFF)==0;
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
        unsigned int index=(p->opcode>>8)-1;
        assert(index>0);
        p->opcode-=256;
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
        return p->opcode==256;
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
        return p->opcode==q->opcode;
    }
    else
    {
        return p->opcode==q->opcode;
    }
}

static struct Node *eliminate_lambda(struct Node *p)
{
    // It uses some Tromp rules as well as some others

    // Tromp rule 1: lambda x . S K y -> S K
    if(IS_A(p) && IS_A(p->left) && p->left->left->opcode==OP_S && p->left->right->opcode==OP_K)
    {
        return &KI;
    }
    // Common rule 2: lambda x . M (where M doesn't contain variable 0) -> K M
    else if(!contains_variable_zero(p))
    {
        return new_application_load(&K, p);
    }
    // Common rule 3: lambda x . x -> I
    else if(IS_VAR(p) && contains_variable_zero(p))
    {
        return &I;
    }
    // Common rule 4: lambda x . M x (where M doesn't contain variable 0) -> M
    else if(IS_A(p) && !contains_variable_zero(p->left) && IS_VAR(p->right) && contains_variable_zero(p->right))
    {
        return p->left;
    }

    // Tromp rule 5: lambda x . x M x -> S S K x M
    // Not sure about its usefullness
    else if(IS_A(p) && IS_A(p->left) && IS_VAR(p->left->left) && contains_variable_zero(p->left->left) && IS_VAR(p->right) && contains_variable_zero(p->right))
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

struct BLC8BitBuffer
{
    FILE         *f;
    unsigned int  bits;
    unsigned int  count;
};

static inline void init_blc8_buffer(struct BLC8BitBuffer *p, FILE *f)
{
    p->f=f;
    p->bits=0;
    p->count=0;
}

static inline int read_blc8_buffer(struct BLC8BitBuffer *p)
{
    if(p->count==0)
    {
        int ch=fgetc(p->f);
        if(ch==EOF)
            return -1;
        p->bits=ch;
        p->count=8;
    }

    --p->count;
    p->bits<<=1;
    return (p->bits&0x100)>>8;
}

static struct Node *parse_blc8_subprogram(struct BLC8BitBuffer *buf, unsigned int lambda_depth)
{
    int ch=read_blc8_buffer(buf);
    if(ch==0)
    {
        ch=read_blc8_buffer(buf);
        if(ch==0)
        {
            struct Node *p=parse_blc8_subprogram(buf, lambda_depth+1);
            if(!p)
                return NULL;

            p=eliminate_lambda(p);
            adjust_variables(p);

            return p;
        }
        else if(ch==1)
        {
            struct Node *left=parse_blc8_subprogram(buf, lambda_depth);
            if(!left)
                return NULL;
            struct Node *right=parse_blc8_subprogram(buf, lambda_depth);
            if(!right)
                return NULL;
            return new_application_load(left, right);
        }
        else
        {
            return NULL;
        }
    }
    else if(ch==1)
    {
        unsigned int index=0;
        while((ch=read_blc8_buffer(buf))==1)
        {
            ++index;
        }
        if(ch!=0 || index>=lambda_depth)
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

static struct Node *parse_blc8_program(FILE *f)
{
    struct BLC8BitBuffer buf;
    init_blc8_buffer(&buf, f);

    return parse_blc8_subprogram(&buf, 0);
}

static void reduce(struct Node *p)
{
    // If the expression's root node is not Application then there's nothing we can do
    // to reduce it
    if(p->opcode!=OP_APPLY)
        return;

    struct ProtectedNode prot;
    prot.node=p;
    prot.next=protected_nodes;
    protected_nodes=&prot;

    unsigned int stack_cap=1000;
    unsigned int stack_size=0;
    struct Node **stack=(struct Node **)malloc(stack_cap*sizeof(struct Node *));

    unsigned int applies=0;
    while(p->opcode==OP_APPLY)
    {
        unsigned int op=p->left->opcode;
        // Left subtree is an application as well - we need to go deeper
        if(op==OP_APPLY)
        {
            if(stack_size==stack_cap)
            {
                stack_cap*=2;
                stack=(struct Node **)realloc(stack, stack_cap*sizeof(struct Node *));
            }
            stack[stack_size]=p;
            ++stack_size;

            p=p->left;
        }
        // Stopper atoms
        else if((op&0x0F)==0)
        {
            break;
        }
        else
        {
            // Trivial opcode
            if((op&0x01)==0)
            {
#if 0
                static const unsigned int mod[3]={0, OP_Sxy^OP_Wx, OP_Sx^OP_O};

                unsigned int opr=p->right->opcode;
                p->opcode=(op>>1)^mod[((op&0x0F00)>>8)&((opr&0xF000)>>12)];
#else
                p->opcode=op>>1;
#endif
            }
            // Non-trivial opcode with handler function
            else
            {
                apply_functions[(op>>1)&0x0F](p);
            }

            if(p->opcode!=OP_APPLY && stack_size>0)
            {
                --stack_size;
                p=stack[stack_size];
            }
        }

        ++applies;
        if(applies==GC_EVERY)
        {
            gc();
            applies=0;
        }
    }

    free(stack);

    protected_nodes=prot.next;
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
        else if(is_blc8_source(argv[i]))
        {
            node=parse_blc8_program(f);
        }

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

    return program->opcode==OP_OUT_S ? 0 : 126;
}
