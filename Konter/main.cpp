// main.cpp
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <iomanip>
#include <algorithm>
#include <curl/curl.h>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

using namespace std;

// ============= FIREBASE SETTINGS =============
const string FIREBASE_BASE = "https://konterhp-9de3b-default-rtdb.asia-southeast1.firebasedatabase.app"; // <-- GANTI DENGAN BASE URL MU (tanpa trailing slash)

// Helper URL-encode (simple)
string urlEncode(const string &value) {
    string encoded;
    char hex[4];
    for (unsigned char c : value) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded.push_back(c);
        } else {
            snprintf(hex, sizeof(hex), "%%%02X", c);

            encoded += hex;
        }
    }
    return encoded;
}

// libcurl write callback
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// ================== BINARY TREE FOR CATEGORY ====================
struct Category {
    string name;
    Category* left;
    Category* right;
    Category(string n) : name(n), left(NULL), right(NULL) {}
};

Category* insertCategory(Category* root, string name) {
    if (!root) return new Category(name);
    if (name < root->name) root->left = insertCategory(root->left, name);
    else if (name > root->name) root->right = insertCategory(root->right, name);
    return root;
}

Category* findMin(Category* root){
    while (root && root->left) root = root->left;
    return root;
}

Category* deleteCategory(Category* root, string key){
    if(!root) return root;
    if(key < root->name) root->left = deleteCategory(root->left, key);
    else if(key > root->name) root->right = deleteCategory(root->right, key);
    else {
        if(!root->left) return root->right;
        else if(!root->right) return root->left;
        Category* temp = findMin(root->right);
        root->name = temp->name;
        root->right = deleteCategory(root->right, temp->name);
    }
    return root;
}

void displayCategories(Category* root) {
    if (!root) return;
    displayCategories(root->left);
    cout << " - " << root->name << endl;
    displayCategories(root->right);
}

// ================== STOCK SYSTEM ====================
struct Phone {
    string kategori, varian, nomorSeri, memori, warna;
    long hargaBeli = 0, hargaJual = 0;
    string status = "Belum Terjual";
};

// in-memory storage
vector<Phone> stok;

// Save to CSV (local backup)
void saveToCSV() {
    ofstream file("database.csv");
    file << "Kategori,Varian,Nomor Seri,Memori,Warna,Harga Beli,Harga Jual,Status\n";
    for (auto &p : stok) {
        file << p.kategori << "," << p.varian << "," << p.nomorSeri << "," << p.memori << "," 
             << p.warna << "," << p.hargaBeli << "," << p.hargaJual << "," << p.status << "\n";
    }
    file.close();
}

// =========== FIREBASE REST HELPERS ===========
// PUT (create or replace) /stok/<key>.json
bool firebasePut(const string &key, const json &payload) {
    string url = FIREBASE_BASE + "/stok/" + urlEncode(key) + ".json";
    CURL *curl = curl_easy_init();
    if (!curl) return false;

    string response;
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    string s = payload.dump();
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, s.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        cerr << "Firebase PUT error: " << curl_easy_strerror(res) << endl;
        return false;
    }
    return true;
}

// DELETE /stok/<key>.json
bool firebaseDelete(const string &key) {
    string url = FIREBASE_BASE + "/stok/" + urlEncode(key) + ".json";
    CURL *curl = curl_easy_init();
    if (!curl) return false;
    string response;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    if (res != CURLE_OK) {
        cerr << "Firebase DELETE error: " << curl_easy_strerror(res) << endl;
        return false;
    }
    return true;
}

// GET /stok.json (all)
bool firebaseGetAll(string &outBody) {
    string url = FIREBASE_BASE + "/stok.json";
    CURL *curl = curl_easy_init();
    if (!curl) return false;
    string response;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    if (res != CURLE_OK) {
        cerr << "Firebase GET error: " << curl_easy_strerror(res) << endl;
        return false;
    }
    outBody = response;
    return true;
}

// =========== SYNC HELPERS ===========
// Convert Phone -> json
json phoneToJson(const Phone &p) {
    json j;
    j["kategori"] = p.kategori;
    j["varian"] = p.varian;
    j["nomorSeri"] = p.nomorSeri;
    j["memori"] = p.memori;
    j["warna"] = p.warna;
    j["hargaBeli"] = p.hargaBeli;
    j["hargaJual"] = p.hargaJual;
    j["status"] = p.status;
    return j;
}

// Convert json -> Phone (assumes fields exist)
Phone jsonToPhone(const json &j) {
    Phone p;
    if (j.contains("kategori")) p.kategori = j["kategori"].get<string>();
    if (j.contains("varian")) p.varian = j["varian"].get<string>();
    if (j.contains("nomorSeri")) p.nomorSeri = j["nomorSeri"].get<string>();
    if (j.contains("memori")) p.memori = j["memori"].get<string>();
    if (j.contains("warna")) p.warna = j["warna"].get<string>();
    if (j.contains("hargaBeli")) p.hargaBeli = j["hargaBeli"].get<long>();
    if (j.contains("hargaJual")) p.hargaJual = j["hargaJual"].get<long>();
    if (j.contains("status")) p.status = j["status"].get<string>();
    return p;
}

// Load all data from Firebase to stok vector (on startup)
void loadFromFirebase() {
    string body;
    if (!firebaseGetAll(body)) {
        cout << "Gagal ambil data dari Firebase (network error)\n";
        return;
    }
    if (body == "null" || body.empty()) {
        // no data
        stok.clear();
        return;
    }

    try {
        auto parsed = json::parse(body);
        stok.clear();
        for (auto it = parsed.begin(); it != parsed.end(); ++it) {
            // key = it.key(); value = it.value()
            json val = it.value();
            Phone p = jsonToPhone(val);
            // Ensure nomorSeri exists; if not, use key as nomorSeri
            if (p.nomorSeri.empty()) p.nomorSeri = it.key();
            stok.push_back(p);
        }
        saveToCSV();
    } catch (exception &e) {
        cerr << "Error parse JSON from Firebase: " << e.what() << endl;
    }
}

// =========== LOCAL UI / CRUD (sinkron ke Firebase) ===========
void displayStock() {
    cout << "\n=========== LIST STOCK HP ===========" << endl;
    int i = 1;
    for (auto &p : stok) {
        cout << "#" << i++ << endl;
        cout << "Kategori     : " << p.kategori << endl;
        cout << "Varian       : " << p.varian << endl;
        cout << "Nomor Seri   : " << p.nomorSeri << endl;
        cout << "Memori       : " << p.memori << endl;
        cout << "Warna        : " << p.warna << endl;
        cout << "Harga Beli   : " << p.hargaBeli << endl;
        cout << "Harga Jual   : " << p.hargaJual << endl;
        cout << "Status       : " << p.status << endl;
        cout << "--------------------------------------\n";
    }
}

// Add Stock (POST as PUT /stok/<nomorSeri>.json)
void addStockFirebase(Category* root) {
    Phone p;
    cout << "\n=== Tambah kategori (yang tersedia) ===\n";
    displayCategories(root);

    cout << "\nPilih kategori: ";
    cin >> p.kategori;
    cout << "Varian       : ";
    cin >> ws; getline(cin, p.varian);
    cout << "Nomor Seri   : ";
    getline(cin, p.nomorSeri);
    cout << "Memori       : ";
    getline(cin, p.memori);
    cout << "Warna        : ";
    getline(cin, p.warna);
    cout << "Harga Beli   : ";
    cin >> p.hargaBeli;
    cout << "Harga Jual   : ";
    cin >> p.hargaJual;
    p.status = "Belum Terjual";

    if (p.nomorSeri.empty()) {
        cout << "Nomor seri tidak boleh kosong!\n";
        return;
    }

    // push to firebase using PUT with key = nomorSeri
    json payload = phoneToJson(p);
    bool ok = firebasePut(p.nomorSeri, payload);
    if (ok) {
        // update local vector (replace if exists)
        auto it = find_if(stok.begin(), stok.end(), [&](const Phone &x){ return x.nomorSeri == p.nomorSeri; });
        if (it != stok.end()) *it = p; else stok.push_back(p);
        saveToCSV();
        cout << "\n📌 Stock berhasil ditambahkan & disinkron ke Firebase!\n";
    } else {
        cout << "\n❌ Gagal menyimpan ke Firebase\n";
    }
}

// Edit stock (modify fields and sync via PUT)
void editStockFirebase() {
    string sn;
    cout << "\nMasukkan nomor seri yang ingin diedit: ";
    cin >> ws; getline(cin, sn);

    auto it = find_if(stok.begin(), stok.end(), [&](const Phone &x){ return x.nomorSeri == sn; });
    if (it == stok.end()) {
        cout << "❌ Nomor seri tidak ditemukan!\n";
        return;
    }
    Phone &p = *it;
    cout << "\n=== Edit Data (kosongkan lalu enter untuk tidak mengubah) ===\n";

    cout << "Varian (" << p.varian << "): ";
    string tmp;
    getline(cin, tmp);
    if (!tmp.empty()) p.varian = tmp;

    cout << "Memori (" << p.memori << "): ";
    getline(cin, tmp);
    if (!tmp.empty()) p.memori = tmp;

    cout << "Warna (" << p.warna << "): ";
    getline(cin, tmp);
    if (!tmp.empty()) p.warna = tmp;

    cout << "Harga Beli (" << p.hargaBeli << "): ";
    getline(cin, tmp);
    if (!tmp.empty()) p.hargaBeli = stol(tmp);

    cout << "Harga Jual (" << p.hargaJual << "): ";
    getline(cin, tmp);
    if (!tmp.empty()) p.hargaJual = stol(tmp);

    // sync to firebase
    json payload = phoneToJson(p);
    bool ok = firebasePut(p.nomorSeri, payload);
    if (ok) {
        saveToCSV();
        cout << "\n📌 Data berhasil diupdate & disinkron ke Firebase!\n";
    } else {
        cout << "\n❌ Gagal update ke Firebase\n";
    }
}

// Delete stock (DELETE /stok/<nomorSeri>.json)
void deleteStockFirebase() {
    string sn;
    cout << "\nMasukkan nomor seri yang ingin dihapus: ";
    cin >> ws; getline(cin, sn);

    auto it = find_if(stok.begin(), stok.end(), [&](const Phone &x){ return x.nomorSeri == sn; });
    if (it == stok.end()) {
        cout << "❌ Nomor seri tidak ditemukan!\n";
        return;
    }

    bool ok = firebaseDelete(sn);
    if (ok) {
        stok.erase(it);
        saveToCSV();
        cout << "\n🗑 Produk berhasil dihapus dari Firebase & lokal!\n";
    } else {
        cout << "\n❌ Gagal menghapus di Firebase\n";
    }
}

// Admin display simplified
void displayForAdmin() {
    cout << "\n=========== LIST HP UNTUK ADMIN TOKO ===========" << endl;
    for (auto &p : stok) {
        cout << p.varian << ", " << p.memori << ", " << p.warna 
             << ", SN:" << p.nomorSeri << ", Harga: " << p.hargaJual 
             << " (" << p.status << ")" << endl;
    }
}

// Update status -> set Terjual and sync
void updateStatusFirebase() {
    string sn;
    cout << "\nMasukkan nomor seri: ";
    cin >> ws; getline(cin, sn);

    auto it = find_if(stok.begin(), stok.end(), [&](const Phone &x){ return x.nomorSeri == sn; });
    if (it == stok.end()) {
        cout << "\n❌ Nomor seri tidak ditemukan\n";
        return;
    }
    it->status = "Terjual";
    json payload = phoneToJson(*it);
    if (firebasePut(it->nomorSeri, payload)) {
        saveToCSV();
        cout << "\n📌 Status berhasil diubah menjadi 'Terjual' & disinkron!\n";
    } else {
        cout << "\n❌ Gagal update status di Firebase\n";
    }
}

// ================== LOGIN SYSTEM ====================
bool login(string &role) {
    string user, pass;
    cout << "\n========= LOGIN SYSTEM =========\n";
    cout << "Username : ";
    cin >> user;
    cout << "Password : ";
    cin >> pass;

    if (user == "superadmin" && pass == "12345") { role = "super"; return true; }
    if (user == "admin" && pass == "admin") { role = "admin"; return true; }
    return false;
}

// ================== MAIN MENU ====================
int main() {
    // init curl globally
    curl_global_init(CURL_GLOBAL_ALL);

    Category* root = NULL;
    root = insertCategory(root, "Android");
    root = insertCategory(root, "iPhone");

    // load data from firebase on startup
    cout << "Mengambil data dari Firebase..." << endl;
    loadFromFirebase();
    cout << "Selesai mengambil data. (" << stok.size() << " records)\n";

    string role;
    if (!login(role)) {
        cout << "\n❌ Login gagal!\n";
        curl_global_cleanup();
        return 0;
    }

    int choice;
    do {
        if (role == "super") {
            cout << "\n===== MENU SUPER ADMIN =====\n";
            cout << "1. Tambah kategori\n";
            cout << "2. Hapus kategori\n";
            cout << "3. Tambah stock HP (sync Firebase)\n";
            cout << "4. Edit produk (sync Firebase)\n";
            cout << "5. Hapus produk (sync Firebase)\n";
            cout << "6. Lihat semua stock\n";
            cout << "0. Logout\n";
            cout << "Pilih: ";
            cin >> choice;

            switch (choice) {
                case 1: {
                    string cat;
                    cout << "\nMasukkan kategori baru: ";
                    cin >> ws; getline(cin, cat);
                    root = insertCategory(root, cat);
                    cout << "📌 Kategori berhasil ditambahkan!\n";
                    break;
                }
                case 2: {
                    string cat;
                    cout << "\nMasukkan kategori yang akan dihapus: ";
                    cin >> ws; getline(cin, cat);
                    root = deleteCategory(root, cat);
                    cout << "🗑 Kategori berhasil dihapus!\n";
                    break;
                }
                case 3: addStockFirebase(root); break;
                case 4: editStockFirebase(); break;
                case 5: deleteStockFirebase(); break;
                case 6: displayStock(); break;
                default: break;
            }
        } else if (role == "admin") {
            cout << "\n===== MENU ADMIN TOKO =====\n";
            cout << "1. Lihat daftar HP\n";
            cout << "2. Ubah status menjadi terjual (sync Firebase)\n";
            cout << "0. Logout\n";
            cout << "Pilih: ";
            cin >> choice;

            switch (choice) {
                case 1: displayForAdmin(); break;
                case 2: updateStatusFirebase(); break;
                default: break;
            }
        }

    } while (choice != 0);

    cout << "\n👋 Logout berhasil.\n";

    // cleanup curl
    curl_global_cleanup();
    return 0;
}
