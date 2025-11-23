#include "Tree.h"

// Insert kategori ke dalam tree
Category* insertCategory(Category* root, string name) {
    if (!root) {
        root = new Category{name, {}, nullptr, nullptr};
        return root;
    }

    if (name < root->name) root->left = insertCategory(root->left, name);
    else if (name > root->name) root->right = insertCategory(root->right, name);

    return root;
}

// Cari kategori
Category* searchCategory(Category* root, string name) {
    if (!root || root->name == name) return root;

    if (name < root->name) return searchCategory(root->left, name);
    return searchCategory(root->right, name);
}

// Cari node terkecil (helper delete)
Category* findMin(Category* node) {
    while (node && node->left)
        node = node->left;
    return node;
}

// Hapus kategori
Category* deleteCategory(Category* root, string name) {
    if (!root) return root;

    if (name < root->name) root->left = deleteCategory(root->left, name);
    else if (name > root->name) root->right = deleteCategory(root->right, name);
    else {
        if (!root->left) {
            Category* temp = root->right;
            delete root;
            return temp;
        }
        else if (!root->right) {
            Category* temp = root->left;
            delete root;
            return temp;
        }

        Category* temp = findMin(root->right);
        root->name = temp->name;
        root->products = temp->products;
        root->right = deleteCategory(root->right, temp->name);
    }
    return root;
}

// Tambahkan produk ke kategori
void addProduct(Category* root, string categoryName, string productName) {
    Category* found = searchCategory(root, categoryName);
    if (found) {
        found->products.push_back(productName);
        cout << "Produk \"" << productName << "\" berhasil ditambahkan ke kategori " << categoryName << endl;
    } else {
        cout << "Kategori tidak ditemukan!" << endl;
    }
}

// Menampilkan produk dalam kategori
void showProducts(Category* root, string categoryName) {
    Category* found = searchCategory(root, categoryName);
    if (!found) {
        cout << "Kategori tidak ditemukan!" << endl;
        return;
    }

    cout << "\nProduk dalam kategori " << categoryName << ":\n";
    if (found->products.empty()) {
        cout << "- (Belum ada produk)\n";
        return;
    }

    for (auto& p : found->products)
        cout << "- " << p << endl;
}

// Traversal
void inorder(Category* root) {
    if (!root) return;
    inorder(root->left);
    cout << root->name << " ";
    inorder(root->right);
}

void preorder(Category* root) {
    if (!root) return;
    cout << root->name << " ";
    preorder(root->left);
    preorder(root->right);
}

void postorder(Category* root) {
    if (!root) return;
    postorder(root->left);
    postorder(root->right);
    cout << root->name << " ";
}

void showAllProducts(Category* root) {
    if (!root) return;

    showAllProducts(root->left);

    cout << "\nKategori: " << root->name << endl;
    if (root->products.empty()) {
        cout << "  - (Tidak ada produk)" << endl;
    } else {
        for (auto &p : root->products) {
            cout << "  - " << p << endl;
        }
    }

    showAllProducts(root->right);
}
