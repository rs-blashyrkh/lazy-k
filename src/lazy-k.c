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
    // Application nodes. Dynamically allocated in heap
    OP_APPLY    = 0x00,
    // Special opcodes - IN_C (input continuation) and IN_V (input value). All special nodes
    // are chained using 'left' pointer into single list. All special nodes of particular
    // type are resolved and replaced en masse.
    OP_IN_C     = 0x01,
    OP_IN_V     = 0x03,
    // Stopper opcodes - reduction stops if one of them is the left child of application
    OP_X        = 0x02,
    OP_Y        = 0x06,
    // Atom Z used for Lazy K mode output implementation. It effectively exchanges left and
    // right subtree
    OP_Z        = 0x05,
    // Output continuation (all modes) and output stop (BLC-only)
    OP_OUT_C    = 0x07,
    OP_OUT_S    = 0x09,

    // Basic combinators. Shift is arity-1
    OP_I        = 0x0B,
    OP_K        = 0x0D<<1,
    OP_S        = 0x0F<<2,
    OP_B        = 0x11<<2, // B = lambda xyz . x(yz)
    OP_C        = 0x13<<2, // C = lambda xyz . xzy

    // Extended combinators. Named after their 1-based serial number in lexicographically-sorted
    // list of normal forms. E.g. I=T1, K=T3, S=T2113. 'T' stands for Tromp who suggested binary
    // encoding of lambda terms. Common name (if any) is given as a comment.
    OP_T12      = 0x15<<1, // T = lambda xy . yx
    OP_T61      = 0x17<<1, // O = SI = lambda xy . y(xy)
    OP_T2       = 0x19<<1, // KI = lambda xy . y
    OP_T298     = 0x1B<<2, // V = lambda xyz . zxy
    OP_T23988   = 0x1D<<3, // BC = lambda xyzw . xywz
    OP_T4       = 0x1F<<2, // K(KI) = lambda xyz . z
    OP_T21      = 0x21,    // O(K(KI)) = T(KI) = lambda x . x(KI)
    OP_T6       = 0x23<<2, // KK = lambda xyz . y
    OP_T55      = 0x25<<2, // S(KK) = BK = lambda xyz . xy
    OP_T24652   = 0x27<<3, // BB = lambda xyzw . xy(zw)
    OP_T24299   = 0x29<<3, // BBV = lambda xyzw . wx(yz)
    OP_T24290   = 0x2B<<3, // C(BBV) = lambda xyzw . wy(xz)
    OP_T5281    = 0x2D<<3, // KS = lambda xyzw . yw(zw)
    OP_T200528  = 0x2F<<3, // S(KS) = BS = lambda xyzw . xyw(zw)
    OP_T129     = 0x31<<2, // KO = lambda xyz . z(yz)
    OP_T313     = 0x33<<2, // BT = lambda xyz . z(xy)
    OP_T2155    = 0x35<<2, // S(KO) = lambda xyz . z(xyz)
    OP_T3627    = 0x37<<2, // ++ = SB = lambda xyz . y(xyz)
    OP_T5       = 0x39,    // SII = lambda x . xx
    OP_T27176   = 0x3B<<2, // SS = lambda xyz . yz(xyz)
    OP_T561     = 0x3D<<1, // SSI = B(SII) = lambda xy . xy(xy)
    OP_T98      = 0x3F<<1, // 2 = SBI = lambda xy . x(xy)
    OP_T2064    = 0x41<<2, // S(KS)T = lambda xyz . zx(yz)
    OP_T19      = 0x43<<1, // S(KK)(SII) = lambda xy . xx
    OP_T5852    = 0x45<<2, // BB(SII) = lambda xyz . xx(yz)
    OP_T3445    = 0x47<<2, // SC = lambda xyz . yz(xy)
    OP_T185     = 0x49<<1, // SSK = SCI = lambda xy . xyx
    OP_T8576    = 0x4B<<3, // S(KK)(S(KS)T) = lambda xyzw . wx(zw)
    OP_T29      = 0x4D,    // SI(KK) = TK = lambda x . xK
    OP_T203117  = 0x4F<<3, // BS(S(KK)(S(KS)T)) = lambda xyzw . wx(yzw)
    OP_T339     = 0x51<<1, // S(SII) = lambda xy . yy(xy)
    OP_T329     = 0x53<<1, // C(T(KI)) = lambda xy . y(KI)x
    OP_T3157    = 0x55<<3, // B(BK) = lambda xyzw . xyz
};

struct Node
{
    struct Node   *next;
    struct Node   *left;
    struct Node   *right;
    unsigned int   opcode:31;
    unsigned int   mark:1;
};

static inline int is_special(unsigned int opcode)
{
    return (opcode&~2)==1;
}

static inline int is_stopper(unsigned int opcode)
{
    return (opcode&~4)==2;
}

typedef void (*apply_fn)(struct Node *);

static void apply_input_cont(struct Node *a);
static void apply_input_val(struct Node *a);
static void apply_atom_Z(struct Node *a);
static void apply_output_cont(struct Node *a);
static void apply_output_stop(struct Node *a);
static void apply_I(struct Node *a);
static void apply_K(struct Node *a);
static void apply_S(struct Node *a);
static void apply_B(struct Node *a);
static void apply_C(struct Node *a);
static void apply_T12(struct Node *a);
static void apply_T61(struct Node *a);
static void apply_T2(struct Node *a);
static void apply_T298(struct Node *a);
static void apply_T23988(struct Node *a);
static void apply_T4(struct Node *a);
static void apply_T21(struct Node *a);
static void apply_T6(struct Node *a);
static void apply_T55(struct Node *a);
static void apply_T24652(struct Node *a);
static void apply_T24299(struct Node *a);
static void apply_T24290(struct Node *a);
static void apply_T5281(struct Node *a);
static void apply_T200528(struct Node *a);
static void apply_T129(struct Node *a);
static void apply_T313(struct Node *a);
static void apply_T2155(struct Node *a);
static void apply_T3627(struct Node *a);
static void apply_T5(struct Node *a);
static void apply_T27176(struct Node *a);
static void apply_T561(struct Node *a);
static void apply_T98(struct Node *a);
static void apply_T2064(struct Node *a);
static void apply_T19(struct Node *a);
static void apply_T5852(struct Node *a);
static void apply_T3445(struct Node *a);
static void apply_T185(struct Node *a);
static void apply_T8576(struct Node *a);
static void apply_T29(struct Node *a);
static void apply_T203117(struct Node *a);
static void apply_T339(struct Node *a);
static void apply_T329(struct Node *a);
static void apply_T3157(struct Node *a);

static const apply_fn apply_functions[]=
{
    apply_input_cont,
    apply_input_val,
    apply_atom_Z,
    apply_output_cont,
    apply_output_stop,
    apply_I,
    apply_K,
    apply_S,
    apply_B,
    apply_C,
    apply_T12,
    apply_T61,
    apply_T2,
    apply_T298,
    apply_T23988,
    apply_T4,
    apply_T21,
    apply_T6,
    apply_T55,
    apply_T24652,
    apply_T24299,
    apply_T24290,
    apply_T5281,
    apply_T200528,
    apply_T129,
    apply_T313,
    apply_T2155,
    apply_T3627,
    apply_T5,
    apply_T27176,
    apply_T561,
    apply_T98,
    apply_T2064,
    apply_T19,
    apply_T5852,
    apply_T3445,
    apply_T185,
    apply_T8576,
    apply_T29,
    apply_T203117,
    apply_T339,
    apply_T329,
    apply_T3157
};

// Combinators. Statically allocated, not chained by ".next" and thus they can't become victims of GC.
static struct Node X={NULL, NULL, NULL, OP_X, 0};
static struct Node Y={NULL, NULL, NULL, OP_Y, 0};
static struct Node Z={NULL, NULL, NULL, OP_Z, 0};
static struct Node OUT_CONT={NULL, NULL, NULL, OP_OUT_C, 0};
static struct Node OUT_STOP={NULL, NULL, NULL, OP_OUT_S, 0};

static struct Node I={NULL, NULL, NULL, OP_I, 0};
static struct Node K={NULL, NULL, NULL, OP_K, 0};
static struct Node S={NULL, NULL, NULL, OP_S, 0};
static struct Node B={NULL, NULL, NULL, OP_B, 0};
static struct Node C={NULL, NULL, NULL, OP_C, 0};
static struct Node T12={NULL, NULL, NULL, OP_T12, 0};
static struct Node T61={NULL, NULL, NULL, OP_T61, 0};
static struct Node T2={NULL, NULL, NULL, OP_T2, 0};
static struct Node T298={NULL, NULL, NULL, OP_T298, 0};
static struct Node T23988={NULL, NULL, NULL, OP_T23988, 0};
static struct Node T4={NULL, NULL, NULL, OP_T4, 0};
static struct Node T21={NULL, NULL, NULL, OP_T21, 0};
static struct Node T6={NULL, NULL, NULL, OP_T6, 0};
static struct Node T55={NULL, NULL, NULL, OP_T55, 0};
static struct Node T24652={NULL, NULL, NULL, OP_T24652, 0};
static struct Node T24299={NULL, NULL, NULL, OP_T24299, 0};
static struct Node T24290={NULL, NULL, NULL, OP_T24290, 0};
static struct Node T5281={NULL, NULL, NULL, OP_T5281, 0};
static struct Node T200528={NULL, NULL, NULL, OP_T200528, 0};
static struct Node T129={NULL, NULL, NULL, OP_T129, 0};
static struct Node T313={NULL, NULL, NULL, OP_T313, 0};
static struct Node T2155={NULL, NULL, NULL, OP_T2155, 0};
static struct Node T3627={NULL, NULL, NULL, OP_T3627, 0};
static struct Node T5={NULL, NULL, NULL, OP_T5, 0};
static struct Node T27176={NULL, NULL, NULL, OP_T27176, 0};
static struct Node T561={NULL, NULL, NULL, OP_T561, 0};
static struct Node T98={NULL, NULL, NULL, OP_T98, 0};
static struct Node T2064={NULL, NULL, NULL, OP_T2064, 0};
static struct Node T19={NULL, NULL, NULL, OP_T19, 0};
static struct Node T5852={NULL, NULL, NULL, OP_T5852, 0};
static struct Node T3445={NULL, NULL, NULL, OP_T3445, 0};
static struct Node T185={NULL, NULL, NULL, OP_T185, 0};
static struct Node T8576={NULL, NULL, NULL, OP_T8576, 0};
static struct Node T29={NULL, NULL, NULL, OP_T29, 0};
static struct Node T203117={NULL, NULL, NULL, OP_T203117, 0};
static struct Node T339={NULL, NULL, NULL, OP_T339, 0};
static struct Node T329={NULL, NULL, NULL, OP_T329, 0};
static struct Node T3157={NULL, NULL, NULL, OP_T3157, 0};

#ifndef GC_THRESHOLD
# define GC_THRESHOLD (1024*1024*1024)
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
static unsigned int num_allocations=0;
// }

static void reduce(struct Node *expr);


static void mark_node(struct Node *node)
{
    if(node && !node->mark)
    {
        node->mark=1;
        if(!is_special(node->opcode)) // 'left' has different meaning for special nodes
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
    num_allocations-=freed;
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

    if(is_special(opcode))
    {
        p->left=special_node_list;
        special_node_list=p;
    }

    ++num_allocations;

    return p;
}

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
    if(is_special(a->opcode))
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

static void apply_input_cont(struct Node *a)
{
    struct Node *node=find_special_nodes(OP_IN_C);
    if(!node)
        return;

    switch(io_mode)
    {
    case IO_LAZYK:
        {
            struct Node *left=new_application(&T298, new_input_value());
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
                left=new_application(&T298, code=='0' ? &K : &T2);
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
                struct Node *p=&T2;
                for(int i=0; i<8; ++i, code>>=1)
                {
                    p=new_application(new_application(&T298, (code&1) ? &T2 : &K), p);
                }

                left=new_application(&T298, p);
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

    struct Node *donor=&T2;
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

static void apply_atom_Z(struct Node *a)
{
    replace_application(a, a->right, a->left);
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
            replace_application(a, &T12, &OUT_CONT);
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
                new_application(&T298, &OUT_CONT),
                &OUT_STOP);
        }
        break;

    case IO_BLC8:
        {
            unsigned int code=0;
            struct Node *l=a->right;
            for(int i=0; i<8; ++i)
            {
                struct Node *n=new_application(l, new_application(new_application(&T298, &X), &Y));
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
                new_application(&T298, &OUT_CONT),
                &OUT_STOP);
        }
        break;

    }
}

static void apply_output_stop(struct Node *a)
{
    exit(0);
}

static void apply_I(struct Node *a)
{
    replace_node(a, a->right);
}

static void apply_K(struct Node *a)
{
    replace_node(a, a->left->right);
}

static void apply_S(struct Node *a)
{
    replace_application(a, new_application(a->left->left->right, a->right), new_application(a->left->right, a->right));
}

static void apply_B(struct Node *a)
{
    replace_application(a, a->left->left->right, new_application(a->left->right, a->right));
}

static void apply_C(struct Node *a)
{
    replace_application(a, new_application(a->left->left->right, a->right), a->left->right);
}

static void apply_T12(struct Node *a)
{
    replace_application(a, a->right, a->left->right);
}

static void apply_T61(struct Node *a)
{
    replace_application(a, a->right, new_application(a->left->right, a->right));
}

static void apply_T2(struct Node *a)
{
    replace_node(a, a->right);
}

static void apply_T298(struct Node *a)
{
    replace_application(a, new_application(a->right, a->left->left->right), a->left->right);
}

static void apply_T23988(struct Node *a)
{
    replace_application(a, new_application(new_application(a->left->left->left->right, a->left->left->right), a->right), a->left->right);
}

static void apply_T4(struct Node *a)
{
    replace_node(a, a->right);
}

static void apply_T21(struct Node *a)
{
    replace_application(a, a->right, &T2);
}

static void apply_T6(struct Node *a)
{
    replace_node(a, a->left->right);
}

static void apply_T55(struct Node *a)
{
    replace_application(a, a->left->left->right, a->left->right);
}

static void apply_T24652(struct Node *a)
{
    replace_application(
        a,
        new_application(a->left->left->left->right, a->left->left->right),
        new_application(a->left->right, a->right));
}

static void apply_T24299(struct Node *a)
{
    replace_application(
        a,
        new_application(a->right, a->left->left->left->right),
        new_application(a->left->left->right, a->left->right));
}

static void apply_T24290(struct Node *a)
{
    replace_application(
        a,
        new_application(a->right, a->left->left->right),
        new_application(a->left->left->left->right, a->left->right));
}

static void apply_T5281(struct Node *a)
{
    replace_application(
        a,
        new_application(a->left->left->right, a->right),
        new_application(a->left->right, a->right));
}

static void apply_T200528(struct Node *a)
{
    replace_application(
        a,
        new_application(new_application(a->left->left->left->right, a->left->left->right), a->right),
        new_application(a->left->right, a->right));
}

static void apply_T129(struct Node *a)
{
    replace_application(
        a,
        a->right,
        new_application(a->left->right, a->right));
}

static void apply_T313(struct Node *a)
{
    replace_application(
        a,
        a->right,
        new_application(a->left->left->right, a->left->right));
}

static void apply_T2155(struct Node *a)
{
    replace_application(
        a,
        a->right,
        new_application(new_application(a->left->left->right, a->left->right), a->right));
}

static void apply_T3627(struct Node *a)
{
    replace_application(
        a,
        a->left->right,
        new_application(new_application(a->left->left->right, a->left->right), a->right));
}

static void apply_T5(struct Node *a)
{
    a->left=a->right;
}

static void apply_T27176(struct Node *a)
{
    replace_application(
        a,
        new_application(a->left->right, a->right),
        new_application(new_application(a->left->left->right, a->left->right), a->right));
}

static void apply_T561(struct Node *a)
{
    struct Node *t=new_application(a->left->right, a->right);
    replace_application(a, t, t);
}

static void apply_T98(struct Node *a)
{
    replace_application(a, a->left->right, new_application(a->left->right, a->right));
}

static void apply_T2064(struct Node *a)
{
    replace_application(
        a,
        new_application(a->right, a->left->left->right),
        new_application(a->left->right, a->right));
}

static void apply_T19(struct Node *a)
{
    replace_application(
        a,
        a->left->right,
        a->left->right);
}

static void apply_T5852(struct Node *a)
{
    replace_application(
        a,
        new_application(a->left->left->right, a->left->left->right),
        new_application(a->left->right, a->right));
}

static void apply_T3445(struct Node *a)
{
    replace_application(
        a,
        new_application(a->left->right, a->right),
        new_application(a->left->left->right, a->left->right));
}

static void apply_T185(struct Node *a)
{
    replace_application(
        a,
        new_application(a->left->right, a->right),
        a->left->right);
}

static void apply_T8576(struct Node *a)
{
    replace_application(
        a,
        new_application(a->right, a->left->left->left->right),
        new_application(a->left->right, a->right));
}

static void apply_T29(struct Node *a)
{
    replace_application(a, a->right, &K);
}

static void apply_T203117(struct Node *a)
{
    replace_application(
        a,
        new_application(a->right, a->left->left->left->right),
        new_application(
            new_application(a->left->left->right, a->left->right),
            a->right));
}

static void apply_T339(struct Node *a)
{
    replace_application(
        a,
        new_application(a->right, a->right),
        new_application(a->left->right, a->right));
}

static void apply_T329(struct Node *a)
{
    replace_application(
        a,
        new_application(a->right, &T2),
        a->left->right);
}

static void apply_T3157(struct Node *a)
{
    replace_application(
        a,
        new_application(a->left->left->left->right, a->left->left->right),
        a->left->right);
}

static struct Node *new_application_load(struct Node *left, struct Node *right)
{
    struct OptimizeRule
    {
        unsigned int  op_left;
        unsigned int  op_right;
        struct Node  *node;
    };
    static const struct OptimizeRule rules[]=
    {
        {OP_K,       OP_I,      &T2},
        {OP_S,       OP_K,      &T2},
        {OP_S,       OP_I,      &T61},
        {OP_C,       OP_I,      &T12},
        {OP_B,       OP_C,      &T23988},
        {OP_T23988,  OP_T12,    &T298},
        {OP_B,       OP_B,      &T24652},
        {OP_T24652,  OP_T298,   &T24299},
        {OP_C,       OP_T24299, &T24290},
        {OP_K,       OP_S,      &T5281},
        {OP_S,       OP_T5281,  &T200528},
        {OP_B,       OP_S,      &T200528},
        {OP_K,       OP_T61,    &T129},
        {OP_B,       OP_T12,    &T313},
        {OP_S,       OP_T129,   &T2155},
        {OP_T2155,   OP_K,      &T12},
        {OP_S,       OP_B,      &T3627},
        {OP_T61,     OP_I,      &T5},
        {OP_S,       OP_S,      &T27176},
        {OP_T27176,  OP_I,      &T561},
        {OP_B,       OP_T5,     &T561},
        {OP_T200528, OP_K,      &B},
        {OP_T3627,   OP_I,      &T98},
        {OP_B,       OP_K,      &T55},
        {OP_T61,     OP_T4,     &T21},
        {OP_T12,     OP_T2,     &T21},
        {OP_T200528, OP_T12,    &T2064},
        {OP_T55,     OP_T5,     &T19},
        {OP_T200528, OP_T19,    &T5852},
        {OP_T24652,  OP_T5,     &T5852},
        {OP_S,       OP_C,      &T3445},
        {OP_T3445,   OP_I,      &T185},
        {OP_T27176,  OP_K,      &T185},
        {OP_T55,     OP_T2064,  &T8576},
        {OP_T12,     OP_K,      &T29},
        {OP_T61,     OP_T6,     &T29},
        {OP_T200528, OP_T8576,  &T203117},
        {OP_S,       OP_T5,     &T339},
        {OP_C,       OP_T21,    &T329},
        {OP_B,       OP_T55,    &T3157},
        {OP_S,       OP_T6,     &T55},
        {OP_K,       OP_K,      &T6},
        {OP_K,       OP_T2,     &T4},
//        {OP_T6,      OP_T2,     &K},
    };

    for(unsigned int i=0; i<sizeof(rules)/sizeof(rules[0]); ++i)
    {
        if(left->opcode==rules[i].op_left && right->opcode==rules[i].op_right)
            return rules[i].node;
    }

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
        return &T2;
    }
    // Common rule 2: lambda x . M (where M doesn't contain x) -> K M
    else if(!contains_variable_zero(p))
    {
        return new_application_load(&K, p);
    }
    // Common rule 3: lambda x . x -> I
    else if(IS_VAR(p) && contains_variable_zero(p))
    {
        return &I;
    }
    // Common rule 4: lambda x . M x (where M doesn't contain x) -> M
    else if(IS_A(p) && !contains_variable_zero(p->left) && IS_VAR(p->right) && contains_variable_zero(p->right))
    {
        return p->left;
    }
    // My rule: lambda x . x M (where M doesn't contain x) -> T M
    else if(IS_A(p) && IS_VAR(p->left) && contains_variable_zero(p->left) && !contains_variable_zero(p->right))
    {
        return new_application_load(&T12, p->right);
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
        else if(is_stopper(op))
        {
            break;
        }
        else
        {
#if 0
            if(!p->left->left && !p->right->left)
            {
                fprintf(stderr, "%02x %02x\n", p->left->opcode, p->right->opcode);
            }
#endif
            // Trivial opcode
            if((op&0x01)==0)
            {
                p->opcode=op>>1;
            }
            // Non-trivial opcode with handler function
            else
            {
                apply_functions[(op>>1)](p);
            }

            if(p->opcode!=OP_APPLY && stack_size>0)
            {
                --stack_size;
                p=stack[stack_size];
            }
        }

        if(num_allocations>=GC_THRESHOLD/48)
        {
            gc();
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

static struct Node *dump(struct Node *p)
{
    if(!p)
        return p;

    unsigned int opcode=p->opcode;
    if(opcode==OP_APPLY)
    {
        fputc('`', stderr);
        dump(p->left);
        dump(p->right);
    }
    else
    {
        while((opcode&1)==0)
            opcode>>=1;
        opcode>>=1;

        static const char *s[]=
        {
                 "",     "",      "",       "",       "",      "I",      "K",       "S",
                "B",    "C",   "T12",    "T61",     "T2",   "T298", "T23988",      "T4",
              "T21",   "T6",   "T55", "T24652", "T24299", "T24290",  "T5281", "T200528",
             "T129", "T313", "T2155",  "T3627",     "T5", "T27176",   "T561",     "T98",
            "T2064",  "T19", "T5852",  "T3445",   "T185",  "T8576",    "T29", "T203117",
             "T339", "T329", "T3157",
        };
        fprintf(stderr, "%s", s[opcode]);
    }

    return p;
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
            node=dump(parse_lazyk_program(f));
        }
        else if(is_blc_source(argv[i]))
        {
            node=dump(parse_blc_program(f, 0));
        }
        else if(is_blc8_source(argv[i]))
        {
            node=dump(parse_blc8_program(f));
        }

        fclose(f);


        if(!node)
        {
            fprintf(stderr, "Failed to parse source file\n");
            return 1;
        }

        program=new_application(node, program);
    }

    program=new_application(program, &OUT_CONT);
    if(io_mode==IO_BLC || io_mode==IO_BLC8)
    {
        program=new_application(program, &OUT_STOP);
    }

    reduce(program);

    return program->opcode==OP_OUT_S ? 0 : 126;
}
