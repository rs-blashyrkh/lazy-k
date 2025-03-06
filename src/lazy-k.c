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
#include <ctype.h>


struct Node
{
    struct Node   *next;
    struct Node   *left;
    struct Node   *right;
    int          (*apply)(struct Node *a);
    unsigned int   special:1;
    unsigned int   mark:1;
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
static int apply_input_cont(struct Node *a);
static int apply_input_char(struct Node *a);
static int apply_output(struct Node *a);
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
    all_node_list=p;

    if(special)
    {
        p->left=special_node_list;
        special_node_list=p;
    }

    return p;
}

static struct Node I={NULL, NULL, NULL, apply_I, 0, 0};
static struct Node K={NULL, NULL, NULL, apply_K, 0, 0};
static struct Node S={NULL, NULL, NULL, apply_S, 0, 0};
static struct Node B={NULL, NULL, NULL, apply_B, 0, 0};
static struct Node C={NULL, NULL, NULL, apply_C, 0, 0};
static struct Node O={NULL, NULL, NULL, apply_O, 0, 0};

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
    return new_node(NULL, NULL, apply_output, 0);
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
    // It slows down a bit (cost of extra checks), so it's commented out
    // if(a->right->apply==apply_I)
    // {
    //     a->apply=apply_O;
    // }
    // else
    {
        a->apply=apply_Sx;
    }

    return 0;
}

static int apply_Sx(struct Node *a)
{
    // Doesn't affect performance, so it's commented out
    // if(a->right->apply==apply_I)
    // {
    //     a->right=a->left->right;
    //     a->apply=apply_Wx;
    // }
    // else
    {
        a->left=a->left->right;
        a->apply=apply_Sxy;
    }

    return 0;
}

static int apply_Sxy(struct Node *a)
{
    replace_application(a, new_application(a->left->left, a->right), new_application(a->left->right, a->right));
    return 0;
}

static int apply_B(struct Node *a)
{
    a->apply=apply_Bx;
    return 0;
}

static int apply_Bx(struct Node *a)
{
    a->left=a->left->right;
    a->apply=apply_Bxy;
    return 0;
}

static int apply_Bxy(struct Node *a)
{
    replace_application(a, a->left->left, new_application(a->left->right, a->right));
    return 0;
}

static int apply_C(struct Node *a)
{
    a->apply=apply_Cx;
    return 0;
}

static int apply_Cx(struct Node *a)
{
    a->left=a->left->right;
    a->apply=apply_Cxy;
    return 0;
}

static int apply_Cxy(struct Node *a)
{
    replace_application(a, new_application(a->left->left, a->right), a->left->right);
    return 0;
}

static int apply_O(struct Node *a)
{
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
    replace_application(a, new_application(a->left->right, a->right), a->right);
    return 0;
}

static int apply_input_cont(struct Node *a)
{
    struct Node *node=find_special_nodes(apply_input_cont);
    if(!node)
        return 0;

    struct Node *left=new_application(&C, new_application(new_application(&C, &I), new_input_char()));
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

static int apply_output(struct Node *a)
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
    a->left=new_application(&C, &I);
    a->right=new_output_sink();
    return 0;
}

static int apply_atom_X(struct Node *a)
{
    struct Node *tmp=a->right;
    a->right=a->left;
    a->left=tmp;
    return 0;
}

static int apply_atom_Y(struct Node *a)
{
    return 1;
}


// TODO: return parsing error info (line, col, message)
static struct Node *parse_file(FILE *f)
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

                if(left->apply==apply_S && right->apply==apply_I)
                    op_stack[op_stack_size-1]=&O;
                else
                    op_stack[op_stack_size-1]=new_application(left, right);
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


int main(int argc, char *argv[])
{
    struct Node *program=new_input_cont();
    for(int i=1; i<argc; ++i)
    {
        FILE *f=fopen(argv[i], "rt");
        if(!f)
        {
            perror("Failed to open source file");
            return 1;
        }

        struct Node *node=parse_file(f);
        fclose(f);

        if(!node)
        {
            fprintf(stderr, "Failed to parse source file\n");
            return 1;
        }

        program=new_application(node, program);
    }

    program=new_application(program, new_output_sink());

    reduce(program);

    return 126;
}
