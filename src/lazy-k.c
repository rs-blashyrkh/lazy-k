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
    struct Node   *hash_next;
    struct Node   *left;
    struct Node   *right;
    int          (*apply)(struct Node *a);
    unsigned int   mark:1;
    unsigned int   immortal:1;
};

struct ProtectedNode
{
    struct ProtectedNode *next;
    struct Node          *node;
};

// TODO: grow on demand
#ifndef BUCKETS
# define BUCKETS 65537
#endif

#ifndef GC_EVERY
# define GC_EVERY 25000
#endif

// Global variables
// {
static struct Node *bucket_heads[BUCKETS]={NULL};
static struct ProtectedNode *protected_nodes=NULL;
// TODO: use +1 and *2 (and probably a^b and a^a) ops
static struct Node *numeral[257];
// }

static void dump_node(const struct Node *n, FILE *f);
static void dump_node_prompt(const char *prompt, const struct Node *n, FILE *f);
static void reduce(struct Node *expr);

static int apply_I(struct Node *a);
static int apply_K(struct Node *a);
static int apply_K1(struct Node *a);
static int apply_S(struct Node *a);
static int apply_S1(struct Node *a);
static int apply_S2(struct Node *a);
static int apply_B(struct Node *a);
static int apply_B1(struct Node *a);
static int apply_B2(struct Node *a);
static int apply_C(struct Node *a);
static int apply_C1(struct Node *a);
static int apply_C2(struct Node *a);
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
        mark_node(node->left);
        mark_node(node->right);
    }
}

static void gc(void)
{
    // Mark all protected nodes and all their subtrees
    for(struct ProtectedNode *p=protected_nodes; p!=NULL; p=p->next)
        mark_node(p->node);

    unsigned int freed=0;
    unsigned int kept=0;
    // Unhash and free all unmarked nodes, unmark all marked nodes
    for(unsigned int bucket=0; bucket<BUCKETS; ++bucket)
    {
        struct Node **pp=bucket_heads+bucket;
        while((*pp)!=NULL)
        {
            struct Node *p=*pp;
            if(p->mark || p->immortal)
            {
                p->mark=0;
                pp=&p->hash_next;
                ++kept;
            }
            else
            {
                *pp=p->hash_next;
                free(p);
                ++freed;
            }
        }
    }
    //fprintf(stderr, "GC: %u freed, %u kept\n", freed, kept);
}

static inline unsigned int find_bucket(struct Node *left, struct Node *right, int (*apply)(struct Node *a))
{
    unsigned int hash=2235019355U;
    hash=((hash<<13)+hash+(unsigned int)(intptr_t)left)^(hash>>19);
    hash=((hash<<13)+hash+(unsigned int)(intptr_t)right)^(hash>>19);
    hash=((hash<<13)+hash+(unsigned int)(intptr_t)apply)^(hash>>19);
//    hash=((hash<<13)+hash+opcode)^(hash>>19);

    return hash%BUCKETS;
}

static inline struct Node *new_node(struct Node *left, struct Node *right, int (*apply)(struct Node *a))
{
    const unsigned int bucket=find_bucket(left, right, apply);
    struct Node *p=bucket_heads[bucket];
    while(p)
    {
        if(p->left==left && p->right==right && p->apply==apply)
            return p;
        p=p->hash_next;
    }

    p=(struct Node *)malloc(sizeof(struct Node));
    p->hash_next=bucket_heads[bucket];
    p->left=left;
    p->right=right;
    p->apply=apply;
    p->mark=0;
    p->immortal=0;
    bucket_heads[bucket]=p;

    return p;
}

static inline void unhash_node(struct Node *node)
{
    const unsigned int bucket=find_bucket(node->left, node->right, node->apply);
    struct Node **p=bucket_heads+bucket;
    while(*p && *p!=node)
        p=&(*p)->hash_next;
    if(*p==node)
    {
        *p=node->hash_next;
        node->hash_next=NULL;
    }
}

static inline void hash_node(struct Node *node)
{
    const unsigned int bucket=find_bucket(node->left, node->right, node->apply);
    node->hash_next=bucket_heads[bucket];
    bucket_heads[bucket]=node;
}

static inline void replace_node_components(
    struct Node   *node,
    struct Node   *left,
    struct Node   *right,
    int          (*apply)(struct Node *a))
{
    unhash_node(node);
    node->left=left;
    node->right=right;
    node->apply=apply;
    hash_node(node);
}

static inline void replace_node(
    struct Node   *node,
    struct Node   *orig)
{
    replace_node_components(node, orig->left, orig->right, orig->apply);
}



static inline struct Node *new_application(struct Node *left, struct Node *right)
{
    return new_node(left, right, NULL);
}

static inline struct Node *new_combinator(char ch)
{
    ch=tolower(ch);
    if(ch=='i')
        return new_node(NULL, NULL, apply_I);
    else if(ch=='k')
        return new_node(NULL, NULL, apply_K);
    else if(ch=='s')
        return new_node(NULL, NULL, apply_S);
    else if(ch=='b')
        return new_node(NULL, NULL, apply_B);
    else if(ch=='c')
        return new_node(NULL, NULL, apply_C);
    else
        return NULL;
}

static inline struct Node *new_output_sink(void)
{
    return new_node(NULL, NULL, apply_output);
}

static inline struct Node *new_atom_X(void)
{
    return new_node(NULL, NULL, apply_atom_X);
}

static inline struct Node *new_atom_Y(void)
{
    return new_node(NULL, NULL, apply_atom_Y);
}

// Find all nodes, remove them from the hash table, chain together into single-linked list
// and return the head of the list (and in the darkness bind them, of course)
static struct Node *find_all_nodes(struct Node *left, struct Node *right, int (*apply)(struct Node *a))
{
    struct Node *res=NULL;

    const unsigned int bucket=find_bucket(left, right, apply);
    struct Node **pp=bucket_heads+bucket;
    while(*pp!=NULL)
    {
        struct Node *p=*pp;
        if(p->left==left && p->right==right && p->apply==apply)
        {
            *pp=p->hash_next;
            p->hash_next=res;
            res=p;
        }
        else
        {
            pp=&p->hash_next;
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
    replace_node_components(a, a->right, NULL, apply_K1);
    return 0;
}

static int apply_K1(struct Node *a)
{
    replace_node(a, a->left->left);
    return 0;
}

static int apply_S(struct Node *a)
{
    replace_node_components(a, a->right, NULL, apply_S1);
    return 0;
}

static int apply_S1(struct Node *a)
{
    replace_node_components(a, a->left->left, a->right, apply_S2);
    return 0;
}

static int apply_S2(struct Node *a)
{
    replace_node_components(a, new_application(a->left->left, a->right), new_application(a->left->right, a->right), NULL);
    return 0;
}

static int apply_B(struct Node *a)
{
    replace_node_components(a, a->right, NULL, apply_B1);
    return 0;
}

static int apply_B1(struct Node *a)
{
    replace_node_components(a, a->left->left, a->right, apply_B2);
    return 0;
}

static int apply_B2(struct Node *a)
{
    replace_node_components(a, a->left->left, new_application(a->left->right, a->right), NULL);
    return 0;
}

static int apply_C(struct Node *a)
{
    replace_node_components(a, a->right, NULL, apply_C1);
    return 0;
}

static int apply_C1(struct Node *a)
{
    replace_node_components(a, a->left->left, a->right, apply_C2);
    return 0;
}

static int apply_C2(struct Node *a)
{
    replace_node_components(a, new_application(a->left->left, a->right), a->left->right, NULL);
    return 0;
}

static int apply_input_cont(struct Node *a)
{
    struct Node *node=find_all_nodes(NULL, NULL, apply_input_cont);
    if(!node)
        return 0;

    struct Node *left=new_application(
        new_combinator('c'),
        new_application(
            new_application(
                new_combinator('c'),
                new_combinator('i')),
            new_node(NULL, NULL, apply_input_char)));
    struct Node *right=new_node(NULL, NULL, apply_input_cont);

    while(node)
    {
        struct Node *next=node->hash_next;
        replace_node_components(node, left, right, NULL);

        node=next;
    }

    return 0;
}

static int apply_input_char(struct Node *a)
{
    struct Node *node=find_all_nodes(NULL, NULL, apply_input_char);
    if(!node)
        return 0;

    int code=fgetc(stdin);
    if(code<0 || code>255)
        code=256;

    while(node)
    {
        struct Node *next=node->hash_next;
        replace_node(node, numeral[code]);

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
    replace_node_components(a, new_application(new_combinator('c'), new_combinator('i')), new_output_sink(), NULL);
    return 0;
}

static int apply_atom_X(struct Node *a)
{
    replace_node_components(a, a->right, a->left, NULL);
    return 0;
}

static int apply_atom_Y(struct Node *a)
{
    return 1;
}


static void dump_node(const struct Node *n, FILE *f)
{
    if(!n->apply)
    {
        fputc('`', f);
        dump_node(n->left, f);
        dump_node(n->right, f);
    }
    else if(n->apply==apply_I)
    {
        fputc('i', f);
    }
    else if(n->apply==apply_K)
    {
        fputc('k', f);
    }
    else if(n->apply==apply_K1)
    {
        fprintf(f, "[`k");
        dump_node(n->left, f);
        fputc(']', f);
    }
    else if(n->apply==apply_S)
    {
        fputc('s', f);
    }
    else if(n->apply==apply_S1)
    {
        fprintf(f, "[`s");
        dump_node(n->left, f);
        fputc(']', f);
    }
    else if(n->apply==apply_S2)
    {
        fprintf(f, "[``s");
        dump_node(n->left, f);
        dump_node(n->right, f);
        fputc(']', f);
    }
    else if(n->apply==apply_B)
    {
        fputc('b', f);
    }
    else if(n->apply==apply_B1)
    {
        fprintf(f, "[`b");
        dump_node(n->left, f);
        fputc(']', f);
    }
    else if(n->apply==apply_B2)
    {
        fprintf(f, "[``b");
        dump_node(n->left, f);
        dump_node(n->right, f);
        fputc(']', f);
    }
    else if(n->apply==apply_C)
    {
        fputc('c', f);
    }
    else if(n->apply==apply_C1)
    {
        fprintf(f, "[`c");
        dump_node(n->left, f);
        fputc(']', f);
    }
    else if(n->apply==apply_C2)
    {
        fprintf(f, "[``c");
        dump_node(n->left, f);
        dump_node(n->right, f);
        fputc(']', f);
    }
    else if(n->apply==apply_input_cont)
    {
        fprintf(f, "<INPUT CONTINUATION>");
    }
    else if(n->apply==apply_input_char)
    {
        fprintf(f, "<INPUT FGETC>");
    }
    else if(n->apply==apply_output)
    {
        fprintf(f, "<OUTPUT SINK>");
    }
    else if(n->apply==apply_atom_X)
    {
        fputc('X', f);
    }
    else if(n->apply==apply_atom_Y)
    {
        fputc('Y', f);
    }
}

static void dump_node_prompt(const char *prompt, const struct Node *n, FILE *f)
{
    fprintf(f, "%s", prompt);
    dump_node(n, f);
    fputc('\n', f);
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
                struct Node *a=new_application(op_stack[op_stack_size-2], op_stack[op_stack_size-1]);
                --n_stack_size;
                ++n_stack[n_stack_size-1];
                --op_stack_size;
                op_stack[op_stack_size-1]=a;
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
    numeral[0]=new_application(new_combinator('k'), new_combinator('i'));
    numeral[0]->immortal=1;
    for(int i=1; i<=256; ++i)
    {
        numeral[i]=new_application(new_application(new_combinator('s'), new_combinator('b')), numeral[i-1]);
        numeral[i]->immortal=1;
    }

    struct Node *program=new_node(NULL, NULL, apply_input_cont);
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
