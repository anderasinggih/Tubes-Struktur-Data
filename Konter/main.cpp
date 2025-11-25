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
const string FIREBASE_BASE = "https://konterhp-9de3b-default-rtdb.asia-southeast1.firebasedatabase.app"; // <-- base URL (without trailing slash)

// Helper URL-encode (safe)
string urlEncode(const string &value) {
    string encoded;
    char hex[8];
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

// ================== USER & STOCK STRUCTS ====================
struct User {
    string username;
    string password;
    string role; // "super" or "admin"
};

struct Phone {
    string kategori, varian, nomorSeri, memori, warna;
    long hargaBeli = 0, hargaJual = 0;
    string status = "Belum Terjual";
    string owner; // username toko (e.g. TOKO1)
};

// in-memory storage
vector<User> users;
vector<Phone> stok;

// Save to CSV (local backup)
void saveToCSV() {
    ofstream file("database.csv");
    file << "Kategori,Varian,Nomor Seri,Memori,Warna,Harga Beli,Harga Jual,Status,Owner\n";
    for (auto &p : stok) {
        file << p.kategori << "," << p.varian << "," << p.nomorSeri << "," << p.memori << "," 
             << p.warna << "," << p.hargaBeli << "," << p.hargaJual << "," << p.status << "," << p.owner << "\n";
    }
    file.close();
}

// =========== FIREBASE REST HELPERS ===========
// Generic: PUT /<node>/<key>.json
bool firebasePutPath(const string &path, const string &key, const json &payload) {
    string url = FIREBASE_BASE + "/" + path + "/" + urlEncode(key) + ".json";
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

// Generic: DELETE /<path>/<key>.json
bool firebaseDeletePath(const string &path, const string &key) {
    string url = FIREBASE_BASE + "/" + path + "/" + urlEncode(key) + ".json";
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

// Generic: GET /<path>.json
bool firebaseGetPath(const string &path, string &outBody) {
    string url = FIREBASE_BASE + "/" + path + ".json";
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

// ========= users helpers =========
bool firebasePutUser(const string &username, const json &payload) {
    return firebasePutPath("users", username, payload);
}
bool firebaseDeleteUser(const string &username) {
    return firebaseDeletePath("users", username);
}
bool firebaseGetUsers(string &outBody) {
    return firebaseGetPath("users", outBody);
}

// ========= stok helpers =========
bool firebasePutStok(const string &nomorSeri, const json &payload) {
    return firebasePutPath("stok", nomorSeri, payload);
}
bool firebaseDeleteStok(const string &nomorSeri) {
    return firebaseDeletePath("stok", nomorSeri);
}
bool firebaseGetAllStok(string &outBody) {
    return firebaseGetPath("stok", outBody);
}

// =========== SYNC HELPERS ===========
// Convert User -> json
json userToJson(const User &u) {
    json j;
    j["password"] = u.password;
    j["role"] = u.role;
    return j;
}

// Convert json -> User (key is username)
User jsonToUser(const string &username, const json &j) {
    User u;
    u.username = username;
    if (j.contains("password")) u.password = j["password"].get<string>();
    if (j.contains("role")) u.role = j["role"].get<string>();
    return u;
}

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
    j["owner"] = p.owner;
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
    if (j.contains("owner")) p.owner = j["owner"].get<string>();
    return p;
}

// Load users from Firebase
void loadUsersFromFirebase() {
    string body;
    if (!firebaseGetUsers(body)) {
        cout << "Gagal ambil users dari Firebase (network error)\n";
        return;
    }
    users.clear();
    if (body == "null" || body.empty()) {
        // empty users
        return;
    }
    try {
        auto parsed = json::parse(body);
        for (auto it = parsed.begin(); it != parsed.end(); ++it) {
            string username = it.key();
            json val = it.value();
            User u = jsonToUser(username, val);
            users.push_back(u);
        }
    } catch (exception &e) {
        cerr << "Error parse JSON users: " << e.what() << endl;
    }
}

// Ensure default users exist (create if none)
void ensureDefaultUsers() {
    if (!users.empty()) return;
    cout << "No users found in Firebase — membuat akun default (superadmin/admin)\n";
    User su; su.username = "superadmin"; su.password = "12345"; su.role = "super";
    User ad; ad.username = "TOKO1"; ad.password = "admin"; ad.role = "admin";
    if (firebasePutUser(su.username, userToJson(su))) {
        users.push_back(su);
    }
    if (firebasePutUser(ad.username, userToJson(ad))) {
        users.push_back(ad);
    }
}

// Load stok from Firebase
void loadFromFirebase() {
    string body;
    if (!firebaseGetAllStok(body)) {
        cout << "Gagal ambil data dari Firebase (network error)\n";
        return;
    }
    if (body == "null" || body.empty()) {
        stok.clear();
        return;
    }

    try {
        auto parsed = json::parse(body);
        stok.clear();
        for (auto it = parsed.begin(); it != parsed.end(); ++it) {
            json val = it.value();
            Phone p = jsonToPhone(val);
            if (p.nomorSeri.empty()) p.nomorSeri = it.key();
            stok.push_back(p);
        }
        saveToCSV();
    } catch (exception &e) {
        cerr << "Error parse JSON from Firebase: " << e.what() << endl;
    }
}

// =========== LOCAL UI / CRUD (sinkron ke Firebase) ===========

void listAdmins() {
    cout << "\n=== Daftar Akun Admin Toko ===\n";
    for (auto &u : users) {
        if (u.role == "admin") cout << " - " << u.username << "\n";
    }
}

void addUserInteractive() {
    User u;
    cout << "\nMasukkan username (contoh: TOKO2): ";
    cin >> ws; getline(cin, u.username);
    cout << "Password: ";
    cin >> ws; getline(cin, u.password);
    u.role = "admin";

    // check duplicate
    for (auto &x : users) if (x.username == u.username) {
        cout << "Username sudah ada!\n"; return;
    }

    if (firebasePutUser(u.username, userToJson(u))) {
        users.push_back(u);
        cout << "✅ Admin berhasil ditambahkan ke Firebase.\n";
    } else {
        cout << "❌ Gagal menambahkan admin ke Firebase.\n";
    }
}

void editUserInteractive() {
    string uname;
    cout << "\nMasukkan username admin yang ingin diubah: ";
    cin >> ws; getline(cin, uname);
    auto it = find_if(users.begin(), users.end(), [&](const User &x){ return x.username == uname && x.role == "admin"; });
    if (it == users.end()) { cout << "User tidak ditemukan atau bukan admin.\n"; return; }
    cout << "Masukkan password baru (kosongkan untuk tidak mengubah): ";
    string np; getline(cin, np);
    if (!np.empty()) it->password = np;
    if (firebasePutUser(it->username, userToJson(*it))) {
        cout << "✅ Password user berhasil diupdate di Firebase.\n";
    } else cout << "❌ Gagal update user di Firebase.\n";
}

void deleteUserInteractive() {
    string uname;
    cout << "\nMasukkan username admin yang ingin dihapus: ";
    cin >> ws; getline(cin, uname);
    auto it = find_if(users.begin(), users.end(), [&](const User &x){ return x.username == uname && x.role == "admin"; });
    if (it == users.end()) { cout << "User tidak ditemukan atau bukan admin.\n"; return; }

    // delete stocks owned by this user
    vector<string> toDelete;
    for (auto &p : stok) if (p.owner == uname) toDelete.push_back(p.nomorSeri);
    for (auto &sn : toDelete) {
        firebaseDeleteStok(sn);
        // remove from local stok
        stok.erase(remove_if(stok.begin(), stok.end(), [&](const Phone &x){ return x.nomorSeri == sn; }), stok.end());
    }

    if (firebaseDeleteUser(uname)) {
        users.erase(it);
        saveToCSV();
        cout << "🗑 User dan semua stok miliknya telah dihapus.\n";
    } else {
        cout << "❌ Gagal menghapus user di Firebase.\n";
    }
}

// Display all stock (superadmin)
void displayStockAll() {
    cout << "\n=========== LIST STOCK HP (SEMUA) ===========\n";
    int i = 1;
    for (auto &p : stok) {
        cout << "#" << i++ << endl;
        cout << "Owner        : " << p.owner << endl;
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

// Add Stock (with owner selection)
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

    // choose owner (list admin usernames)
    cout << "\nPilih owner (username admin). Daftar admin:\n";
    for (auto &u : users) if (u.role == "admin") cout << " - " << u.username << "\n";
    cout << "Masukkan owner: ";
    cin >> ws; getline(cin, p.owner);

    if (p.nomorSeri.empty()) {
        cout << "Nomor seri tidak boleh kosong!\n";
        return;
    }
    if (p.owner.empty()) {
        cout << "Owner tidak boleh kosong!\n";
        return;
    }

    p.status = "Belum Terjual";
    json payload = phoneToJson(p);
    bool ok = firebasePutStok(p.nomorSeri, payload);
    if (ok) {
        auto it = find_if(stok.begin(), stok.end(), [&](const Phone &x){ return x.nomorSeri == p.nomorSeri; });
        if (it != stok.end()) *it = p; else stok.push_back(p);
        saveToCSV();
        cout << "\n📌 Stock berhasil ditambahkan & disinkron ke Firebase!\n";
    } else {
        cout << "\n❌ Gagal menyimpan ke Firebase\n";
    }
}

// Edit stock (only superadmin)
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

    // allow change owner
    cout << "Owner (" << p.owner << "): ";
    getline(cin, tmp);
    if (!tmp.empty()) p.owner = tmp;

    json payload = phoneToJson(p);
    bool ok = firebasePutStok(p.nomorSeri, payload);
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

    bool ok = firebaseDeleteStok(sn);
    if (ok) {
        stok.erase(it);
        saveToCSV();
        cout << "\n🗑 Produk berhasil dihapus dari Firebase & lokal!\n";
    } else {
        cout << "\n❌ Gagal menghapus di Firebase\n";
    }
}

// Admin display simplified (only owner's stok)
void displayForAdmin(const string &owner) {
    cout << "\n=========== LIST HP UNTUK " << owner << " ===========" << endl;
    bool any = false;
    for (auto &p : stok) {
        if (p.owner == owner) {
            any = true;
            cout << p.varian << ", " << p.memori << ", " << p.warna 
                 << ", SN:" << p.nomorSeri << ", Harga: " << p.hargaJual 
                 << " (" << p.status << ")" << endl;
        }
    }
    if (!any) cout << "Tidak ada stok untuk " << owner << endl;
}

// Update status -> set Terjual and sync (admin can)
void updateStatusFirebase(const string &owner) {
    string sn;
    cout << "\nMasukkan nomor seri: ";
    cin >> ws; getline(cin, sn);

    auto it = find_if(stok.begin(), stok.end(), [&](const Phone &x){ return x.nomorSeri == sn && x.owner == owner; });
    if (it == stok.end()) {
        cout << "\n❌ Nomor seri tidak ditemukan atau bukan milik toko Anda\n";
        return;
    }
    it->status = "Terjual";
    json payload = phoneToJson(*it);
    if (firebasePutStok(it->nomorSeri, payload)) {
        saveToCSV();
        cout << "\n📌 Status berhasil diubah menjadi 'Terjual' & disinkron!\n";
    } else {
        cout << "\n❌ Gagal update status di Firebase\n";
    }
}

// ================== LOGIN SYSTEM (Firebase-based) ====================
bool login(User &outUser) {
    string u, p;
    cout << "\n========= LOGIN SYSTEM =========\n";
    cout << "Username : ";
    cin >> u;
    cout << "Password : ";
    cin >> p;

    for (auto &usr : users) {
        if (usr.username == u && usr.password == p) {
            outUser = usr;
            return true;
        }
    }
    return false;
}

// ================== MAIN MENU ====================
int main() {
    // init curl globally
    curl_global_init(CURL_GLOBAL_ALL);

    // initial categories
    Category* root = NULL;
    root = insertCategory(root, "Android");
    root = insertCategory(root, "iPhone");

    // load data from firebase on startup
    cout << "Mengambil users dari Firebase..." << endl;
    loadUsersFromFirebase();
    ensureDefaultUsers(); // create default if empty
    cout << "Users loaded: " << users.size() << endl;

    cout << "Mengambil stok dari Firebase..." << endl;
    loadFromFirebase();
    cout << "Selesai mengambil data. (" << stok.size() << " records)\n";

    User current;
    if (!login(current)) {
        cout << "\n❌ Login gagal!\n";
        curl_global_cleanup();
        return 0;
    }

    cout << "\n=== Selamat datang: " << current.username << " (" << current.role << ") ===\n";

    int choice;
    do {
        if (current.role == "super") {
            cout << "\n===== MENU SUPER ADMIN =====\n";
            cout << "1. Tambah kategori\n";
            cout << "2. Hapus kategori\n";
            cout << "3. Tambah stock HP (sync Firebase)\n";
            cout << "4. Edit produk (sync Firebase)\n";
            cout << "5. Hapus produk (sync Firebase)\n";
            cout << "6. Lihat semua stock\n";
            cout << "7. Kelola akun admin toko\n";
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
                case 6: displayStockAll(); break;
                case 7: {
                    int sub;
                    cout << "\n--- Kelola Akun Admin ---\n";
                    cout << "1. Tambah admin\n";
                    cout << "2. Edit admin (password)\n";
                    cout << "3. Hapus admin\n";
                    cout << "4. Lihat daftar admin\n";
                    cout << "0. Kembali\n";
                    cout << "Pilih: ";
                    cin >> sub;
                    switch (sub) {
                        case 1: addUserInteractive(); break;
                        case 2: editUserInteractive(); break;
                        case 3: deleteUserInteractive(); break;
                        case 4: listAdmins(); break;
                        default: break;
                    }
                    break;
                }
                default: break;
            }
        } else if (current.role == "admin") {
            cout << "\n===== MENU ADMIN TOKO (" << current.username << ") =====\n";
            cout << "1. Lihat daftar HP (hanya milik toko)\n";
            cout << "2. Ubah status menjadi terjual (hanya milik toko)\n";
            cout << "0. Logout\n";
            cout << "Pilih: ";
            cin >> choice;

            switch (choice) {
                case 1: displayForAdmin(current.username); break;
                case 2: updateStatusFirebase(current.username); break;
                default: break;
            }
        }

    } while (choice != 0);

    cout << "\n👋 Logout berhasil.\n";

    // cleanup curl
    curl_global_cleanup();
    return 0;
}
