#ifndef TREE_H
#define TREE_H
#include <iostream>
#include <vector>
using namespace std;

struct Category {
    string name;
    vector<string> products;
    Category *left, *right;
};

// Function prototypes
Category* insertCategory(Category* root, string name);
Category* searchCategory(Category* root, string name);
Category* deleteCategory(Category* root, string name);
Category* findMin(Category* node);

void addProduct(Category* root, string categoryName, string productName);
void showProducts(Category* root, string categoryName);

void inorder(Category* root);
void preorder(Category* root);
void postorder(Category* root);
void showAllProducts(Category* root);


#endif
