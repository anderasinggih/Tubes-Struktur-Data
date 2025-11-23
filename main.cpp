#include <iostream>
#include "Tree.h"
using namespace std;

int main() {
    Category* root = nullptr;
    int choice;
    string category, product;

    do {
        cout << "\n===== E-Commerce Category Tree =====\n";
        cout << "1. Tambah Kategori\n";
        cout << "2. Cari Kategori\n";
        cout << "3. Hapus Kategori\n";
        cout << "4. Tambahkan Produk ke Kategori\n";
        cout << "5. Tampilkan Produk dalam Kategori\n";
        cout << "6. Tampilkan In-Order\n";
        cout << "7. Tampilkan Pre-Order\n";
        cout << "8. Tampilkan Post-Order\n";
        cout << "9. Tampilkan Semua Produk di Semua Kategori\n";
        cout << "0. Exit\n";
        

        cout << "Pilih menu: ";
        cin >> choice;

        switch(choice) {
            case 1:
                cout << "Nama kategori: ";
                cin >> category;
                root = insertCategory(root, category);
                break;
            case 2:
                cout << "Nama kategori: ";
                cin >> category;
                if (searchCategory(root, category))
                    cout << "Kategori ditemukan!\n";
                else
                    cout << "Kategori tidak ada!\n";
                break;
            case 3:
                cout << "Masukkan kategori yang dihapus: ";
                cin >> category;
                root = deleteCategory(root, category);
                break;
            case 4:
                cout << "Kategori tujuan: ";
                cin >> category;
                cout << "Nama produk: ";
                cin >> product;
                addProduct(root, category, product);
                break;
            case 5:
                cout << "Kategori: ";
                cin >> category;
                showProducts(root, category);
                break;
            case 6:
                inorder(root);
                cout << endl;
                break;
            case 7:
                preorder(root);
                cout << endl;
                break;
            case 8:
                postorder(root);
                cout << endl;
                break;
            case 9:
                showAllProducts(root);
                break;

        }
    } while(choice != 0);

    return 0;
}
