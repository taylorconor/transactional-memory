//---------------------------------------------------------------
// File: BST.h
// Purpose: Header file for a demonstration of a binary tree
// Programming Language: C++
// Author: Dr. Rick Coleman
//---------------------------------------------------------------
#ifndef BST_H
#define BST_H

#include <iostream>
using namespace std;

// Define a structure to be used as the tree node
struct TreeNode
{
    int      Key;
    TreeNode *left;
    TreeNode *right;
};

class BST
{
    private:
        TreeNode *root;

    public:
        BST();
        ~BST();
        bool isEmpty();
        TreeNode *SearchTree(int Key);
        int Insert(TreeNode *newNode);
        int Insert(int Key);
        int Delete(int Key);
        void PrintOne(TreeNode *T);
        void PrintTree();
    private:
        void ClearTree(TreeNode *T);
        TreeNode *DupNode(TreeNode * T);
        void PrintAll(TreeNode *T);
};

#endif
