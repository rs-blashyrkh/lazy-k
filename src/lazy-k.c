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
    unsigned int   opcode:31;
    unsigned int   mark:1;
};

#define DECLARE_OPCODE(name, value, apply_fn) \
    enum enum_ ## name {OP_ ## name=value}; \
    static void apply_fn(struct Node *a); \
    static struct Node name={NULL, NULL, NULL, OP_ ## name, 0};

// Application nodes. Dynamically allocated on heap
static const unsigned int OP_APPLY = 0x00;
// Special opcodes - IN (input). No other special opcodes in current implementation, but they
// may be added in the future. All special nodes are chained using 'left' pointer into single
// list. All special nodes with particular opcode are resolved and replaced en masse.
static const unsigned int OP_IN  = 0x01;
static void apply_input(struct Node *a);

// Stopper opcodes - reduction stops if one of them is the left child of application
DECLARE_OPCODE(ATOM_X,     0x02,    NO_APPLY)
DECLARE_OPCODE(ATOM_Y,     0x06,    NO_APPLY)

// Atom Z used for Lazy K mode output implementation. It effectively exchanges left and
// right subtree
DECLARE_OPCODE(ATOM_Z,     0x03,    apply_ATOM_Z)

// Output continuation (all modes) and output stop (BLC-only)
DECLARE_OPCODE(OUT_C,      0x05,    apply_output_cont)
DECLARE_OPCODE(OUT_S,      0x07,    apply_output_stop)

// Basic combinators. Shift is arity-1
DECLARE_OPCODE(I,          0x09,    apply_I)
DECLARE_OPCODE(K,          0x0B<<1, apply_K)
DECLARE_OPCODE(S,          0x0D<<2, apply_S)
DECLARE_OPCODE(B,          0x0F<<2, apply_B) // B = lambda xyz . x(yz)
DECLARE_OPCODE(C,          0x11<<2, apply_C) // C = lambda xyz . xzy

// Extended combinators. Named after their 1-based serial number in lexicographically-sorted
// list of normal forms. E.g. I=T1, K=T3, S=T2113. 'T' stands for Tromp who suggested binary
// encoding of lambda terms. Common name (if any) is given as a comment.
DECLARE_OPCODE(T12,        0x13<<1, apply_T12)    // T = lambda xy . yx
DECLARE_OPCODE(T61,        0x15<<1, apply_T61)    // O = SI = lambda xy . y(xy)
DECLARE_OPCODE(T2,         0x17<<1, NO_APPLY)     // KI = lambda xy . y
DECLARE_OPCODE(T298,       0x19<<2, apply_T298)   // V = lambda xyz . zxy
DECLARE_OPCODE(T23988,     0x1B<<3, apply_T23988) // BC = lambda xyzw . xywz
DECLARE_OPCODE(T4,         0x1D<<2, NO_APPLY)     // K(KI) = lambda xyz . z
DECLARE_OPCODE(T21,        0x1F,    apply_T21)    // O(K(KI)) = T(KI) = lambda x . x(KI)
DECLARE_OPCODE(T6,         0x21<<2, NO_APPLY)     // KK = lambda xyz . y
DECLARE_OPCODE(T55,        0x23<<2, apply_T55)    // S(KK) = BK = lambda xyz . xy
DECLARE_OPCODE(T24652,     0x25<<3, apply_T24652) // BB = lambda xyzw . xy(zw)
DECLARE_OPCODE(T24299,     0x27<<3, apply_T24299) // BBV = lambda xyzw . wx(yz)
DECLARE_OPCODE(T24290,     0x29<<3, apply_T24290) // C(BBV) = lambda xyzw . wy(xz)
DECLARE_OPCODE(T5281,      0x2B<<3, NO_APPLY)     // KS = lambda xyzw . yw(zw)
DECLARE_OPCODE(T200528,    0x2D<<3, apply_T200528)// S(KS) = BS = lambda xyzw . xyw(zw)
DECLARE_OPCODE(T129,       0x2F<<2, NO_APPLY)     // KO = lambda xyz . z(yz)
DECLARE_OPCODE(T313,       0x31<<2, apply_T313)   // Q3 = BT = lambda xyz . z(xy)
DECLARE_OPCODE(T2155,      0x33<<2, apply_T2155)  // S(KO) = BO = lambda xyz . z(xyz)
DECLARE_OPCODE(T3627,      0x35<<2, apply_T3627)  // ++ = SB = lambda xyz . y(xyz)
DECLARE_OPCODE(T5,         0x37,    apply_T5)     // M = SII = lambda x . xx
DECLARE_OPCODE(T27176,     0x39<<2, apply_T27176) // SS = lambda xyz . yz(xyz)
DECLARE_OPCODE(T561,       0x3B<<1, apply_T561)   // M2 = SSI = B(SII) = lambda xy . xy(xy)
DECLARE_OPCODE(T98,        0x3D<<1, apply_T98)    // 2 = SBI = lambda xy . x(xy)
DECLARE_OPCODE(T2064,      0x3F<<2, apply_T2064)  // S(KS)T = lambda xyz . zx(yz)
DECLARE_OPCODE(T19,        0x41<<1, apply_T19)    // S(KK)(SII) = lambda xy . xx
DECLARE_OPCODE(T5852,      0x43<<2, apply_T5852)  // BB(SII) = lambda xyz . xx(yz)
DECLARE_OPCODE(T3445,      0x45<<2, apply_T3445)  // SC = lambda xyz . yz(xy)
DECLARE_OPCODE(T185,       0x47<<1, apply_T185)   // SSK = SCI = lambda xy . xyx
DECLARE_OPCODE(T8576,      0x49<<3, apply_T8576)  // S(KK)(S(KS)T) = lambda xyzw . wx(zw)
DECLARE_OPCODE(T29,        0x4B,    apply_T29)    // SI(KK) = TK = lambda x . xK
DECLARE_OPCODE(T203117,    0x4D<<3, apply_T203117)// BS(S(KK)(S(KS)T)) = lambda xyzw . wx(yzw)
DECLARE_OPCODE(T339,       0x4F<<1, apply_T339)   // S(SII) = lambda xy . yy(xy)
DECLARE_OPCODE(T329,       0x51<<1, apply_T329)   // C(T(KI)) = lambda xy . y(KI)x
DECLARE_OPCODE(T3157,      0x53<<3, apply_T3157)  // B(BK) = lambda xyzw . xyz
DECLARE_OPCODE(T15578,     0x55<<2, apply_T15578) // B(C(T(KI))) = lambda xyz . z(KI)(xy)
DECLARE_OPCODE(T20,        0x57,    apply_T20)    // S(SII)I = lambda x . xxx
DECLARE_OPCODE(T26787,     0x59<<2, apply_T26787) // S(S(KS)T) = lambda xyz . zy(xyz)
DECLARE_OPCODE(T46377,     0x5B<<2, apply_T46377) // S(BB(SII)) = lambda xyz . yy(xyz)
DECLARE_OPCODE(T103,       0x5D,    apply_T103)   // S(SII)(K(KI)) = lambda x . xx(KI)
DECLARE_OPCODE(T155,       0x5F,    apply_T155)   // S(SII)(KK) = lambda x . xxK
DECLARE_OPCODE(T320,       0x61<<2, apply_T320)   // Q = CB = lambda xyz . y(xz)
DECLARE_OPCODE(T25104,     0x63<<3, apply_T25104) // B(CB) = lambda xyzw . z(xyw)
DECLARE_OPCODE(T8695,      0x65<<3, apply_T8695)  // BKS = lambda xyzw . xw(zw)
DECLARE_OPCODE(T511,       0x67<<2, apply_T511)   // S(BK) = lambda xyz . y(xy)
DECLARE_OPCODE(T3397,      0x69<<2, apply_T3397)  // SV = lambda xyz . zy(xy)
DECLARE_OPCODE(T530,       0x6B<<1, apply_T530)   // C(TK) = lambda x y . yKx
DECLARE_OPCODE(T336,       0x6D<<1, apply_T336)   // S(SSK) = lambda xy . y(xy)y
DECLARE_OPCODE(T8,         0x6F<<2, apply_T8)     // BKK = lambda xyz . x
DECLARE_OPCODE(T12076,     0x71<<1, apply_T12076) // 4 = SII(SBI) = lambda xy . x(x(x(xy)))
DECLARE_OPCODE(T4056,      0x73<<1, apply_T4056)  // S(SI) = lambda xy . xy(y(xy))
DECLARE_OPCODE(T44556,     0x75<<2, apply_T44556) // S(KS)(SII) = BS(SII) = lambda xyz . xxz(yz)
DECLARE_OPCODE(T25996,     0x77<<2, apply_T25996) // C(BS(SII)) = lambda xyz . yyz(xz)
DECLARE_OPCODE(T64,        0x79<<1, apply_T64)    // L = CB(SII) = lambda xy . x(yy)
DECLARE_OPCODE(Y,          0x7B,    apply_Y)      // Y=SSI(CB(SII)): Yx = x(Yx), no normal form
DECLARE_OPCODE(T25323,     0x7D<<3, apply_T25323) // B1 = BBB = lambda xyzw . x(yzw)
DECLARE_OPCODE(T25242,     0x7F<<3, apply_T25242) // C(BBB) = lambda xyzw . y(xzw)
DECLARE_OPCODE(T323,       0x81<<2, apply_T323)   // Q1 = BCB = C(BBB)T = lambda xyz . x(zy)
DECLARE_OPCODE(T1982,      0x83<<3, apply_T1982)  // B(BK)(BCB) = lambda xyzw . x(wy)
DECLARE_OPCODE(T24,        0x85<<3, apply_T24)    // BK(BKK) = lambda xyzw . x
DECLARE_OPCODE(T584,       0x87<<1, apply_T584)   // U = B(SI)(SII) = lambda xy . y(xxy)
DECLARE_OPCODE(T41988,     0x89<<3, apply_T41988) // BK(BB(SII)) = lambda xyzw . xx(zw)
DECLARE_OPCODE(T297,       0x8B<<2, apply_T297)   // F = CV = lambda xyz . zyx
DECLARE_OPCODE(T10,        0x8D,    apply_T10)    // TI = lambda x . xI
DECLARE_OPCODE(T95,        0x8F<<1, apply_T95)    // BT(SII) = lambda xy . y(xx)
DECLARE_OPCODE(T9,         0x91<<1, NO_APPLY)     // K(SII) = lambda xy . yy
DECLARE_OPCODE(T4212,      0x93<<1, apply_T4212)  // S(CB(SII)) = lambda xy . y(xy(xy))
DECLARE_OPCODE(T78,        0x95<<3, NO_APPLY)     // K(S(KK)) = lambda xyzw . yz
DECLARE_OPCODE(T198,       0x97<<2, NO_APPLY)     // K(SBI) = lambda xyz . y(yz)
DECLARE_OPCODE(T7,         0x99<<3, NO_APPLY)     // K(K(KI)) = lambda xyzw . w
DECLARE_OPCODE(T11,        0x9B<<3, NO_APPLY)     // K(KK) = lambda xyzw . z
DECLARE_OPCODE(T58,        0x9D<<1, apply_T58)    // lambda xy . xyy
DECLARE_OPCODE(T24648,     0x9F<<3, apply_T24648) // lambda xyzw . xz(yw)
DECLARE_OPCODE(T365,       0xA1<<1, apply_T365)   // lambda xy . y(y(xy))

static inline int is_special(unsigned int opcode)
{
    return opcode==OP_IN;
}

static inline int is_stopper(unsigned int opcode)
{
    return (opcode&~4)==2;
}

typedef void (*apply_fn)(struct Node *);

static const apply_fn apply_functions[]=
{
    apply_input,
    apply_ATOM_Z,
    apply_output_cont,
    apply_output_stop,
    apply_I,
    apply_K,
    apply_S,
    apply_B,
    apply_C,
    apply_T12,
    apply_T61,
    apply_I,  // T2 = K I
    apply_T298,
    apply_T23988,
    apply_I,  // T4 = K T2 = K (K I)
    apply_T21,
    apply_K,  // T6 = K K
    apply_T55,
    apply_T24652,
    apply_T24299,
    apply_T24290,
    apply_S,  // T5281 = K S
    apply_T200528,
    apply_T61,// T129 = K T61
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
    apply_T3157,
    apply_T15578,
    apply_T20,
    apply_T26787,
    apply_T46377,
    apply_T103,
    apply_T155,
    apply_T320,
    apply_T25104,
    apply_T8695,
    apply_T511,
    apply_T3397,
    apply_T530,
    apply_T336,
    apply_T8,
    apply_T12076,
    apply_T4056,
    apply_T44556,
    apply_T25996,
    apply_T64,
    apply_Y,
    apply_T25323,
    apply_T25242,
    apply_T323,
    apply_T1982,
    apply_T24,
    apply_T584,
    apply_T41988,
    apply_T297,
    apply_T10,
    apply_T95,
    apply_T5,  // T9 = K T5
    apply_T4212,
    apply_T55, // T78 = K T55
    apply_T98, // T198 = K T98
    apply_I,   // T7 = K T4 = K (K T2) = K (K (K I))
    apply_K,   // T11 = K T6 = K(KK)
    apply_T58,
    apply_T24648,
    apply_T365,
};

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
static struct Node *free_node_list=NULL;
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

            p->next=free_node_list;
            free_node_list=p;

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
    struct Node *p;
    if(!free_node_list)
    {
        p=(struct Node *)malloc(sizeof(struct Node));
    }
    else
    {
        p=free_node_list;
        free_node_list=p->next;
    }

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

static inline struct Node *new_application(struct Node *left, struct Node *right)
{
    return new_node(left, right, OP_APPLY);
}

static inline struct Node *new_numeral(unsigned int n)
{
    if(n==0)
        return &T2;
    else if(n==1)
        return &I;
    else
    {
        struct Node *numeral=&T98; // T98 = Church "2"
        for(int i=2; i<n; ++i)
        {
            numeral=new_application(&T3627, numeral); // T3627 = SB = ++
        }
        return numeral;
    }
}

static inline struct Node *new_lazyk_combinator(char ch)
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

static inline struct Node *new_thechurch_combinator(char ch)
{
    if(ch=='+')
        // C(BS(BB))
        return new_application(&C, new_application(&T200528, &T24652));
    else if(ch=='*')
        return &T320;
    else if(ch=='^')
        return &T12;
    else if(ch>='0' && ch<='9')
        return new_numeral(ch-'0');
    else
        return NULL;
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

static inline struct Node *new_input(void)
{
    return new_node(NULL, NULL, OP_IN);
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

static void apply_input(struct Node *a)
{
    struct Node *node=find_special_nodes(OP_IN);
    if(!node)
        return;

    switch(io_mode)
    {
    case IO_LAZYK:
        {
            int code=fgetc(stdin);
            if(code<0 || code>255)
                code=256;


            struct Node *left=new_application(&T298, new_numeral(code)); // T298 = T
            struct Node *right=new_input();

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
                right=new_input();
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
                right=new_input();
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

static void apply_ATOM_Z(struct Node *a)
{
    replace_application(a, a->right, a->left);
}

static void lazyk_fetch_output_char(struct Node *n)
{
    n=new_application(new_application(n, &ATOM_Z), &ATOM_X);
    reduce(n);

    unsigned int code=0;
    while(n->opcode==OP_APPLY && n->right->opcode==OP_ATOM_Z && code<256+126)
    {
        ++code;
        n=n->left;
    }
    if(n->opcode!=OP_ATOM_X)
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
}

static void apply_output_cont(struct Node *a)
{
    switch(io_mode)
    {
    case IO_LAZYK:
        {
            lazyk_fetch_output_char(a->right);

            // Should return CI<OUT> to continue
            replace_application(a, &T12, &OUT_C);
        }
        break;

    case IO_BLC:
        {
            struct Node *n=new_application(new_application(a->right, &ATOM_X), &ATOM_Y);
            reduce(n);

            if(n->opcode==OP_ATOM_X)
                fputc('0', stdout);
            else if(n->opcode==OP_ATOM_Y)
                fputc('1', stdout);
            else
                exit(126);
            fflush(stdout);

            // lambda x . x <OUT> <STOP> = C(CI<OUT>)<STOP>
            replace_application(
                a,
                new_application(&T298, &OUT_C),
                &OUT_S);
        }
        break;

    case IO_BLC8:
        {
            unsigned int code=0;
            struct Node *l=a->right;
            for(int i=0; i<8; ++i)
            {
                struct Node *n=new_application(l, new_application(new_application(&T298, &ATOM_X), &ATOM_Y));
                reduce(n);
                if(n->left->opcode==OP_ATOM_X)
                {
                    code=2*code;
                }
                else if(n->left->opcode==OP_ATOM_Y)
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
                new_application(&T298, &OUT_C),
                &OUT_S);
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

static void apply_T298(struct Node *a)
{
    replace_application(a, new_application(a->right, a->left->left->right), a->left->right);
}

static void apply_T23988(struct Node *a)
{
    replace_application(a, new_application(new_application(a->left->left->left->right, a->left->left->right), a->right), a->left->right);
}

static void apply_T21(struct Node *a)
{
    replace_application(a, a->right, &T2);
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

static void apply_T200528(struct Node *a)
{
    replace_application(
        a,
        new_application(new_application(a->left->left->left->right, a->left->left->right), a->right),
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

static void apply_T15578(struct Node *a)
{
    replace_application(
        a,
        new_application(a->right, &T2),
        new_application(a->left->left->right, a->left->right));
}

static void apply_T20(struct Node *a)
{
    a->left=new_application(a->right, a->right);
}

static void apply_T26787(struct Node *a)
{
    replace_application(
        a,
        new_application(a->right, a->left->right),
        new_application(new_application(a->left->left->right, a->left->right), a->right));
}

static void apply_T46377(struct Node *a)
{
    replace_application(
        a,
        new_application(a->left->right, a->left->right),
        new_application(new_application(a->left->left->right, a->left->right), a->right));
}

static void apply_T103(struct Node *a)
{
    replace_application(
        a,
        new_application(a->right, a->right),
        &T2);
}

static void apply_T155(struct Node *a)
{
    replace_application(
        a,
        new_application(a->right, a->right),
        &K);
}

static void apply_T320(struct Node *a)
{
    replace_application(
        a,
        a->left->right,
        new_application(a->left->left->right, a->right));
}

static void apply_T25104(struct Node *a)
{
    replace_application(
        a,
        a->left->right,
        new_application(
            new_application(a->left->left->left->right, a->left->left->right),
            a->right));
}

static void apply_T8695(struct Node *a)
{
    replace_application(
        a,
        new_application(a->left->left->left->right, a->right),
        new_application(a->left->right, a->right));
}

static void apply_T511(struct Node *a)
{
    replace_application(
        a,
        a->left->right,
        new_application(a->left->left->right, a->left->right));
}

static void apply_T3397(struct Node *a)
{
    replace_application(
        a,
        new_application(a->right, a->left->right),
        new_application(a->left->left->right, a->left->right));
}

static void apply_T530(struct Node *a)
{
    replace_application(
        a,
        new_application(a->right, &K),
        a->left->right);
}

static void apply_T336(struct Node *a)
{
    a->left=new_application(a->right, new_application(a->left->right, a->right));
}

static void apply_T8(struct Node *a)
{
    replace_node(a, a->left->left->right);
}

static void apply_T12076(struct Node *a)
{
    replace_application(
        a,
        a->left->right,
        new_application(
            a->left->right,
            new_application(
                a->left->right,
                new_application(a->left->right, a->right))));
}

static void apply_T4056(struct Node *a)
{
    struct Node *t=new_application(a->left->right, a->right);
    replace_application(
        a,
        t,
        new_application(a->right, t));
}

static void apply_T44556(struct Node *a)
{
    replace_application(
        a,
        new_application(
            new_application(a->left->left->right, a->left->left->right),
            a->right),
        new_application(a->left->right, a->right));
}

static void apply_T25996(struct Node *a)
{
    replace_application(
        a,
        new_application(
            new_application(a->left->right, a->left->right),
            a->right),
        new_application(a->left->left->right, a->right));
}

static void apply_T64(struct Node *a)
{
    replace_application(
        a,
        a->left->right,
        new_application(a->right, a->right));
}

static void apply_Y(struct Node *a)
{
    replace_application(
        a,
        a->right,
        a);
}

static void apply_T25323(struct Node *a)
{
    replace_application(
        a,
        a->left->left->left->right,
        new_application(
            new_application(a->left->left->right, a->left->right),
            a->right));
}

static void apply_T25242(struct Node *a)
{
    replace_application(
        a,
        a->left->left->right,
        new_application(
            new_application(a->left->left->left->right, a->left->right),
            a->right));
}

static void apply_T323(struct Node *a)
{
    replace_application(
        a,
        a->left->left->right,
        new_application(a->right, a->left->right));
}

static void apply_T1982(struct Node *a)
{
    replace_application(
        a,
        a->left->left->left->right,
        new_application(a->right, a->left->left->right));
}

static void apply_T24(struct Node *a)
{
    replace_node(a, a->left->left->left->right);
}

static void apply_T584(struct Node *a)
{
    replace_application(
        a,
        a->right,
        new_application(
            new_application(a->left->right, a->left->right),
            a->right));
}

static void apply_T41988(struct Node *a)
{
    replace_application(
        a,
        new_application(a->left->left->left->right, a->left->left->left->right),
        new_application(a->left->right, a->right));
}

static void apply_T297(struct Node *a)
{
    replace_application(
        a,
        new_application(a->right, a->left->right),
        a->left->left->right);
}

static void apply_T10(struct Node *a)
{
    replace_application(
        a,
        a->right,
        &I);
}

static void apply_T95(struct Node *a)
{
    replace_application(
        a,
        a->right,
        new_application(a->left->right, a->left->right));
}

static void apply_T4212(struct Node *a)
{
    struct Node *xy=new_application(a->left->right, a->right);
    replace_application(
        a,
        a->right,
        new_application(xy, xy));
}

static void apply_T58(struct Node *a)
{
    replace_application(
        a,
        new_application(a->left->right, a->right),
        a->right);
}

static void apply_T24648(struct Node *a)
{
    replace_application(
        a,
        new_application(a->left->left->left->right, a->left->right),
        new_application(a->left->left->right, a->right));
}

static void apply_T365(struct Node *a)
{
    replace_application(
        a,
        a->right,
        new_application(a->right, new_application(a->left->right, a->right)));
}

static struct Node *new_application_load(struct Node *left, struct Node *right)
{
    // S(Kx) -> Bx
    if(left->opcode==OP_S && right->opcode==OP_APPLY && right->left->opcode==OP_K)
        return new_application_load(&B, right->right);

    // Sx(Ky) -> Cxy
    if(left->opcode==OP_APPLY && left->left->opcode==OP_S && right->opcode==OP_APPLY && right->left->opcode==OP_K)
        return new_application_load(new_application_load(&C, left->right), right->right);

    // SxI -> T58x (where T58 = lambda xy . xyy)
    if(left->opcode==OP_APPLY && left->left->opcode==OP_S && right->opcode==OP_I)
        return new_application_load(&T58, left->right);

    // C(Kx) -> T55x (where T55 = lambda xyz . xy)
    if(left->opcode==OP_C && right->opcode==OP_APPLY && right->left->opcode==OP_K)
        return new_application_load(&T55, right->right);

    // C(T24652x) -> T24648x (where T24652 = BB, T24648 = lambda xyzw . xz(yw))
    if(left->opcode==OP_C && right->opcode==OP_APPLY && right->left->opcode==OP_T24652)
        return new_application_load(&T24648, right->right);

    // T61(T61x) -> T365x (where T61 = SI, T365 = lambda xy . y(y(xy)))
    if(left->opcode==OP_T61 && right->opcode==OP_APPLY && right->left->opcode==OP_T61)
        return new_application_load(&T365, right->right);
/*
    // B(T5x) = T5852x (where T5 = SII, T5852 = lambda xyz . xx(yz))
    if(left->opcode==OP_B && right->opcode==OP_APPLY && right->left->opcode==OP_T5)
        return new_application_load(&T5852, right->right);
*/
/*
    // B(Bx) = T25323x (where T25323 = lambda xyzw . x(yzw))
    if(left->opcode==OP_B && right->opcode==OP_APPLY && right->left->opcode==OP_B)
        return new_application_load(&T25323, right->right);
*/

    //C(BSx)yzw = BSxzyw = S(xz)yw = xzw(yw)

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
        {OP_B,       OP_T61,    &T2155},
        {OP_T2155,   OP_K,      &T12},
        {OP_S,       OP_B,      &T3627},
        {OP_T61,     OP_I,      &T5},
        {OP_T58,     OP_I,      &T5},
        {OP_S,       OP_S,      &T27176},
        {OP_T27176,  OP_I,      &T561},
        {OP_B,       OP_T5,     &T561},
        {OP_S,       OP_T9,     &T561},
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
        {OP_S,       OP_T78,    &T3157},
        {OP_S,       OP_T6,     &T55},
        {OP_K,       OP_K,      &T6},
        {OP_K,       OP_T2,     &T4},
        {OP_B,       OP_T329,   &T15578},
        {OP_T339,    OP_I,      &T20},
        {OP_S,       OP_T2064,  &T26787},
        {OP_S,       OP_T5852,  &T46377},
        {OP_T339,    OP_T4,     &T103},
        {OP_T339,    OP_T6,     &T155},
        {OP_C,       OP_B,      &T320},
        {OP_B,       OP_T320,   &T25104},
        {OP_T55,     OP_S,      &T8695},
        {OP_S,       OP_T55,    &T511},
        {OP_S,       OP_T298,   &T3397},
        {OP_C,       OP_T29,    &T530},
        {OP_S,       OP_T185,   &T336},
        {OP_T336,    OP_I,      &T20},
        {OP_T511,    OP_I,      &T19},
        {OP_T55,     OP_K,      &T8},
        {OP_T5,      OP_T98,    &T12076},
        {OP_T98,     OP_T98,    &T12076},
        {OP_S,       OP_T61,    &T4056},
        {OP_T200528, OP_T5,     &T44556},
        {OP_C,       OP_T44556, &T25996},
        {OP_T320,    OP_T5,     &T64},
        {OP_T561,    OP_T64,    &Y},
        {OP_T5,      OP_T584,   &Y},
        {OP_T4212,   OP_T64,    &Y},
        {OP_T24652,  OP_B,      &T25323},
        {OP_C,       OP_T25323, &T25242},
        {OP_T23988,  OP_B,      &T323},
        {OP_T25242,  OP_T12,    &T323},
        {OP_T3157,   OP_T323,   &T1982},
        {OP_T55,     OP_T8,     &T24},
        {OP_T2155,   OP_T5,     &T584},
        {OP_T55,     OP_T5852,  &T41988},
        {OP_C,       OP_T298,   &T297},
        {OP_T12,     OP_I,      &T10},
        {OP_T185,    OP_I,      &T10},
        {OP_T313,    OP_T5,     &T95},
        {OP_K,       OP_T5,     &T9},
        {OP_T3627,   OP_T9,     &T64},
        {OP_S,       OP_T64,    &T4212},
        {OP_K,       OP_T55,    &T78},
        {OP_K,       OP_T98,    &T198},
        {OP_K,       OP_T4,     &T7},
        {OP_K,       OP_T6,     &T11},
        {OP_T6,      OP_T2,     &K},
    };

    for(unsigned int i=0; i<sizeof(rules)/sizeof(rules[0]); ++i)
    {
        if(left->opcode==rules[i].op_left && right->opcode==rules[i].op_right)
            return rules[i].node;
    }

    return new_application(left, right);
}

struct ParseErrorInfo
{
    unsigned int line;
    unsigned int col;
    int ch;
    const char *message;
};

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
            op_stack[op_stack_size++]=new_lazyk_combinator(ch);
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

static struct Node *parse_thechurch_program(FILE *f, struct ParseErrorInfo *error)
{
    unsigned int line=1;
    unsigned int col=0;
    int ignore_rest_of_line=0;

    unsigned int op_stack_size=0;
    unsigned int op_stack_cap=0;
    struct Node **op_stack=NULL;

    unsigned int prio_stack_size=0;
    unsigned int prio_stack_cap=0;
    unsigned int *prio_stack=NULL;

    unsigned int n_stack_size=0;
    unsigned int n_stack_cap=10;
    unsigned int *n_stack=(unsigned int *)malloc(n_stack_cap*sizeof(unsigned int));
    n_stack[n_stack_size++]=0;

    static const int priorities[96]=
    {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1,  0,  0,  2,  1, -1, -1, -1, -1,
         4,  5,  6,  7,  8,  9, 10, 11, 12, 13, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  3, -1,
    };

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
        {
            ignore_rest_of_line=1;
            continue;
        }
        if(isspace(ch))
            continue;

        if(ch<0 || ch>=96 || priorities[ch]==-1)
        {
            error->message="Unexpected character";
            goto err;
        }

        if(ch=='(')
        {
            if(n_stack_size>=n_stack_cap)
            {
                n_stack_cap+=10;
                n_stack=(unsigned int *)realloc(n_stack, n_stack_cap*sizeof(unsigned int));
                if(!n_stack)
                    abort();
            }
            n_stack[n_stack_size++]=0;
            continue;
        }

        int prio;
        if(ch==')')
        {
            if(n_stack_size==1)
            {
                error->message="Unbalanced";
                goto err;
            }

            if(n_stack[n_stack_size-1]%2==0)
            {
                error->message="Unfinished subexpression";
                goto err;
            }

            // fold
            while(n_stack[n_stack_size-1]>1)
            {
                assert(n_stack[n_stack_size-1]>=3);
                assert(op_stack_size>=3);
                assert(prio_stack_size>=1);

                // C+A*B   ->  C+(X)
                //                v
                //               [ ]
                //              /   \
                //            [ ]   [B]
                //           /   \
                //         [*]   [A]

                struct Node *a=op_stack[op_stack_size-3];
                struct Node *o=op_stack[op_stack_size-2];
                struct Node *b=op_stack[op_stack_size-1];

                op_stack[op_stack_size-3]=new_application_load(new_application_load(o, a), b);
                op_stack_size-=2;

                prio_stack_size-=1;

                n_stack[n_stack_size-1]-=2;
            }

            n_stack_size-=1;
            ++n_stack[n_stack_size-1];

            prio=0;
        }
        else
        {
            if(op_stack_size>=op_stack_cap)
            {
                op_stack_cap+=20;
                op_stack=(struct Node **)realloc(op_stack, op_stack_cap*sizeof(struct Node *));
                if(!op_stack)
                    abort();
            }
            op_stack[op_stack_size++]=new_thechurch_combinator(ch);
            ++n_stack[n_stack_size-1];

            prio=priorities[ch];
        }

        if(n_stack[n_stack_size-1]%2==0)
        {
            if(prio_stack_size>=prio_stack_cap)
            {
                prio_stack_cap+=10;
                prio_stack=(unsigned int *)realloc(prio_stack, prio_stack_cap*sizeof(unsigned int));
                if(!prio_stack)
                    abort();
            }
            prio_stack[prio_stack_size++]=prio;
        }
        else if(n_stack[n_stack_size-1]>=5)
        {
            assert(op_stack_size>=5);
            assert(prio_stack_size>=2);

            while(n_stack[n_stack_size-1]>=5 && prio_stack[prio_stack_size-2]>=prio_stack[prio_stack_size-1])
            {
                // A*B+C  -> (X)+C
                //            v
                //           [ ]
                //          /   \
                //        [ ]   [B]
                //       /   \
                //     [*]   [A]
                struct Node *a=op_stack[op_stack_size-5];
                struct Node *o=op_stack[op_stack_size-4];
                struct Node *b=op_stack[op_stack_size-3];

                op_stack[op_stack_size-5]=new_application_load(new_application_load(o, a), b);
                op_stack[op_stack_size-4]=op_stack[op_stack_size-2];
                op_stack[op_stack_size-3]=op_stack[op_stack_size-1];
                op_stack_size-=2;

                prio_stack[prio_stack_size-2]=prio_stack[prio_stack_size-1];
                prio_stack_size-=1;

                n_stack[n_stack_size-1]-=2;
            }
        }
    }

    struct Node *res;
    if(n_stack_size==1 && n_stack[0]==0)
    {
        res=new_thechurch_combinator('1');
    }
    else if(n_stack_size>1)
    {
        ++col;
        error->message="Unexpected";
        goto err;
    }
    else
    {
        if(op_stack_size%2==0)
        {
            error->message="Unfinished subexpression";
            goto err;
        }

        while(op_stack_size>1)
        {
            struct Node *a=op_stack[op_stack_size-3];
            struct Node *o=op_stack[op_stack_size-2];
            struct Node *b=op_stack[op_stack_size-1];

            op_stack[op_stack_size-3]=new_application_load(new_application_load(o, a), b);
            op_stack_size-=2;
        }

        res=op_stack[op_stack_size-1];
    }

    free(op_stack);
    free(prio_stack);
    free(n_stack);

    return res;

err:
    error->line=line;
    error->col=col;
    error->ch=ch;
    free(op_stack);
    free(prio_stack);
    free(n_stack);
    return NULL;
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
    // Common rule: lambda x . M (where M doesn't contain x) -> K M
    if(!contains_variable_zero(p))
    {
        return new_application_load(&K, p);
    }
    // Common rule: lambda x . x -> I
    else if(IS_VAR(p) && contains_variable_zero(p))
    {
        return &I;
    }
    // Common rule: lambda x . M x (where M doesn't contain x) -> M
    else if(IS_A(p) && !contains_variable_zero(p->left) && IS_VAR(p->right) && contains_variable_zero(p->right))
    {
        return p->left;
    }
    // My rule: lambda x . x M (where M doesn't contain x) -> (T = lambda x y . y x) M
    else if(IS_A(p) && IS_VAR(p->left) && contains_variable_zero(p->left) && !contains_variable_zero(p->right))
    {
        return new_application_load(&T12, p->right);
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
                fprintf(stderr, "%04x %04x\n", p->left->opcode, p->right->opcode);
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

static inline int is_thechurch_source(const char *filename)
{
    char *p=strrchr(filename, '.');
    return p && strcmp(p, ".holy")==0;
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
                            "",       "",       "",       "",      "I",      "K",       "S",
                 "B",      "C",    "T12",    "T61",     "T2",   "T298", "T23988",      "T4",
               "T21",     "T6",    "T55", "T24652", "T24299", "T24290",  "T5281", "T200528",
              "T129",   "T313",  "T2155",  "T3627",     "T5", "T27176",   "T561",     "T98",
             "T2064",    "T19",  "T5852",  "T3445",   "T185",  "T8576",    "T29", "T203117",
              "T339",   "T329",  "T3157", "T15578",    "T20", "T26787", "T46377",    "T103",
              "T155",   "T320", "T25104",  "T8695",   "T511",  "T3397",   "T530",    "T336",
                "T8", "T12076",  "T4056", "T44556", "T25996",    "T64",      "Y",  "T25323",
            "T25242",   "T323",  "T1982",    "T24",   "T584", "T41988",   "T297",     "T10",
               "T95",     "T9",  "T4212",    "T78",   "T198",     "T7",    "T11",     "T58",
            "T24648",   "T365",
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

        if(is_lazyk_source(argv[i]) || is_thechurch_source(argv[i]))
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

    struct Node *program=new_input();
    for(int i=1; i<argc; ++i)
    {
        FILE *f=fopen(argv[i], "rt");
        if(!f)
        {
            perror("Failed to open source file");
            return 1;
        }

        struct ParseErrorInfo error;
        error.message=NULL;

        struct Node *node;
        if(is_lazyk_source(argv[i]))
        {
            node=dump(parse_lazyk_program(f));
        }
        else if(is_thechurch_source(argv[i]))
        {
            node=parse_thechurch_program(f, &error);
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
            if(!error.message)
                fprintf(stderr, "%s: Unknown parse error\n", argv[i]);
            else if(error.ch==EOF)
                fprintf(stderr, "%s:%u:%u: %s EOF\n", argv[i], error.line, error.col, error.message);
            else
                fprintf(stderr, "%s:%u:%u: %s '%c'\n", argv[i], error.line, error.col, error.message, error.ch);

            return 1;
        }

        program=new_application(node, program);
    }

    program=new_application(program, &OUT_C);
    if(io_mode==IO_BLC || io_mode==IO_BLC8)
    {
        program=new_application(program, &OUT_S);
        reduce(program);
        return program->opcode==OP_OUT_S ? 0 : 126;
    }
    else
    {
        lazyk_fetch_output_char(program);
        return 126;
    }
}
