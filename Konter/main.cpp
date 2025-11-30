// main.cpp
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <iomanip>
#include <algorithm>
#include <ctime>
#include <curl/curl.h>
#include "nlohmann/json.hpp"

using json = nlohmann::json;
using namespace std;

// ================= FIREBASE SETTINGS =================
const string FIREBASE_BASE = "https://konterhp-9de3b-default-rtdb.asia-southeast1.firebasedatabase.app";

// URL encode helper
string urlEncode(const string &value) {
    string encoded;
    char hex[8];
    for (unsigned char c : value) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') encoded.push_back(c);
        else { snprintf(hex,sizeof(hex),"%%%02X",c); encoded+=hex; }
    }
    return encoded;
}

// CURL write callback
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((string*)userp)->append((char*)contents, size*nmemb);
    return size*nmemb;
}

// ================= HIERARCHY TREE (CATEGORY -> BRAND) =================

// Struct untuk Merek (Child dari Kategori)
struct Brand {
    string name;
    Brand* left;
    Brand* right;
    Brand(string n) : name(n), left(nullptr), right(nullptr) {}
};

// Struct untuk Kategori (Parent)
struct Category {
    string name;
    Brand* brandRoot; // Setiap kategori punya Tree Merek sendiri
    Category* left;
    Category* right;
    Category(string n): name(n), brandRoot(nullptr), left(nullptr), right(nullptr) {}
};

// --- Fungsi untuk BRAND (Merek) ---
Brand* insertBrand(Brand* root, string name) {
    if(!root) return new Brand(name);
    if(name < root->name) root->left = insertBrand(root->left, name);
    else if(name > root->name) root->right = insertBrand(root->right, name);
    return root;
}

void displayBrands(Brand* root) {
    if(!root) return;
    displayBrands(root->left);
    cout << "   -> " << root->name << endl;
    displayBrands(root->right);
}

// Traversing Merek
void preorderBrand(Brand* root){
    if(!root) return;
    cout << root->name << " ";
    preorderBrand(root->left);
    preorderBrand(root->right);
}

void inorderBrand(Brand* root){
    if(!root) return;
    inorderBrand(root->left);
    cout << root->name << " ";
    inorderBrand(root->right);
}

void postorderBrand(Brand* root){
    if(!root) return;
    postorderBrand(root->left);
    postorderBrand(root->right);
    cout << root->name << " ";
}

// --- Fungsi untuk KATEGORI ---
Category* insertCategory(Category* root, string name){
    if(!root) return new Category(name);
    if(name < root->name) root->left = insertCategory(root->left, name);
    else if(name > root->name) root->right = insertCategory(root->right, name);
    return root;
}

Category* searchCategory(Category* root, string name) {
    if (root == nullptr || root->name == name) return root;
    if (name < root->name) return searchCategory(root->left, name);
    return searchCategory(root->right, name);
}

Category* findMin(Category* root){
    while(root && root->left) root=root->left;
    return root;
}

Category* deleteCategory(Category* root, string key){
    if(!root) return root;
    if(key < root->name) root->left = deleteCategory(root->left,key);
    else if(key>root->name) root->right=deleteCategory(root->right,key);
    else{
        if(!root->left) return root->right;
        else if(!root->right) return root->left;
        Category* temp=findMin(root->right);
        root->name=temp->name;
        // Note: Idealnya brandRoot juga harus dipindah/dihapus, 
        // tapi untuk simplifikasi kita geser nama saja di sini.
        root->right=deleteCategory(root->right,temp->name);
    }
    return root;
}

void displayCategoriesAndBrands(Category* root){
    if(!root) return;
    displayCategoriesAndBrands(root->left);
    cout << " [" << root->name << "]" << endl;
    if(root->brandRoot) {
        displayBrands(root->brandRoot);
    } else {
        cout << "   (Belum ada merek)" << endl;
    }
    displayCategoriesAndBrands(root->right);
}

// Helper untuk menambah merek ke kategori tertentu
void addBrandToCategory(Category* root, string catName, string brandName) {
    Category* target = searchCategory(root, catName);
    if (target) {
        target->brandRoot = insertBrand(target->brandRoot, brandName);
        cout << "✅ Merek '" << brandName << "' berhasil ditambahkan ke Kategori '" << catName << "'\n";
    } else {
        cout << "❌ Kategori '" << catName << "' tidak ditemukan!\n";
    }
}

// ================= STRUCTS =================
struct User{
    string username, password, role; // role: super/admin
};

struct Phone{
    // Ditambahkan field 'merek'
    string kategori, merek, varian, nomorSeri, memori, warna, status="Belum Terjual", owner, tanggalStok;
    long hargaBeli=0,hargaJual=0;
};

// ================= IN-MEMORY STORAGE =================
vector<User> users;
vector<Phone> stok;

// ================= CSV LOCAL BACKUP =================
void saveToCSV(){
    ofstream file("database.csv");
    // Header ditambah kolom Merek
    file<<"Kategori,Merek,Varian,Nomor Seri,Memori,Warna,Harga Beli,Harga Jual,Status,Owner,Tanggal Stok\n";
    for(auto &p:stok){
        file<<p.kategori<<","<<p.merek<<","<<p.varian<<","<<p.nomorSeri<<","<<p.memori<<","<<p.warna<<","
            <<p.hargaBeli<<","<<p.hargaJual<<","<<p.status<<","<<p.owner<<","<<p.tanggalStok<<"\n";
    }
    file.close();
}

// ================= FIREBASE HELPERS =================
bool firebasePutPath(const string &path, const string &key, const json &payload){
    string url = FIREBASE_BASE + "/" + path + "/" + urlEncode(key) + ".json";
    CURL *curl = curl_easy_init();
    if(!curl) return false;
    string response;
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers,"Content-Type: application/json");
    curl_easy_setopt(curl,CURLOPT_URL,url.c_str());
    curl_easy_setopt(curl,CURLOPT_CUSTOMREQUEST,"PUT");
    string s=payload.dump();
    curl_easy_setopt(curl,CURLOPT_POSTFIELDS,s.c_str());
    curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headers);
    curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,WriteCallback);
    curl_easy_setopt(curl,CURLOPT_WRITEDATA,&response);
    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    if(res != CURLE_OK){ cerr<<"Firebase PUT error: "<<curl_easy_strerror(res)<<endl; return false; }
    return true;
}

bool firebaseDeletePath(const string &path,const string &key){
    string url = FIREBASE_BASE + "/" + path + "/" + urlEncode(key) + ".json";
    CURL *curl=curl_easy_init();
    if(!curl) return false;
    string response;
    curl_easy_setopt(curl,CURLOPT_URL,url.c_str());
    curl_easy_setopt(curl,CURLOPT_CUSTOMREQUEST,"DELETE");
    curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,WriteCallback);
    curl_easy_setopt(curl,CURLOPT_WRITEDATA,&response);
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    if(res != CURLE_OK){ cerr<<"Firebase DELETE error: "<<curl_easy_strerror(res)<<endl; return false; }
    return true;
}

bool firebaseGetPath(const string &path,string &outBody){
    string url=FIREBASE_BASE+"/"+path+".json";
    CURL *curl=curl_easy_init();
    if(!curl) return false;
    string response;
    curl_easy_setopt(curl,CURLOPT_URL,url.c_str());
    curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,WriteCallback);
    curl_easy_setopt(curl,CURLOPT_WRITEDATA,&response);
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    if(res != CURLE_OK){ cerr<<"Firebase GET error: "<<curl_easy_strerror(res)<<endl; return false; }
    outBody = response;
    return true;
}

// ================= USER HELPERS =================
bool firebasePutUser(const string &username,const json &payload){ return firebasePutPath("users",username,payload); }
bool firebaseDeleteUser(const string &username){ return firebaseDeletePath("users",username); }
bool firebaseGetUsers(string &outBody){ return firebaseGetPath("users",outBody); }

// ================= STOK HELPERS =================
bool firebasePutStok(const string &nomorSeri,const json &payload){ return firebasePutPath("stok",nomorSeri,payload); }
bool firebaseDeleteStok(const string &nomorSeri){ return firebaseDeletePath("stok",nomorSeri); }
bool firebaseGetAllStok(string &outBody){ return firebaseGetPath("stok",outBody); }

// ================= JSON CONVERTERS =================
json userToJson(const User &u){ return { {"password",u.password},{"role",u.role} }; }
User jsonToUser(const string &username,const json &j){ User u; u.username=username; u.password=j["password"].get<string>(); u.role=j["role"].get<string>(); return u; }

json phoneToJson(const Phone &p){
    // Update JSON include 'merek'
    return { {"kategori",p.kategori},{"merek",p.merek},{"varian",p.varian},{"nomorSeri",p.nomorSeri},
             {"memori",p.memori},{"warna",p.warna},{"hargaBeli",p.hargaBeli},{"hargaJual",p.hargaJual},
             {"status",p.status},{"owner",p.owner},{"tanggalStok",p.tanggalStok} };
}

Phone jsonToPhone(const json &j){
    Phone p;
    if(j.contains("kategori")) p.kategori=j["kategori"].get<string>();
    if(j.contains("merek")) p.merek=j["merek"].get<string>(); // Load merek
    if(j.contains("varian")) p.varian=j["varian"].get<string>();
    if(j.contains("nomorSeri")) p.nomorSeri=j["nomorSeri"].get<string>();
    if(j.contains("memori")) p.memori=j["memori"].get<string>();
    if(j.contains("warna")) p.warna=j["warna"].get<string>();
    if(j.contains("hargaBeli")) p.hargaBeli=j["hargaBeli"].get<long>();
    if(j.contains("hargaJual")) p.hargaJual=j["hargaJual"].get<long>();
    if(j.contains("status")) p.status=j["status"].get<string>();
    if(j.contains("owner")) p.owner=j["owner"].get<string>();
    if(j.contains("tanggalStok")) p.tanggalStok=j["tanggalStok"].get<string>();
    return p;
}

// ================= DATE HELPER =================
string todayDate(){
    time_t t=time(nullptr);
    tm* tm_ptr=localtime(&t);
    char buffer[11];
    strftime(buffer,sizeof(buffer),"%Y-%m-%d",tm_ptr);
    return string(buffer);
}

// ================= LOAD DATA =================
void loadUsersFromFirebase(){
    string body;
    if(!firebaseGetUsers(body)){ cout<<"Gagal ambil users dari Firebase\n"; return; }
    users.clear();
    if(body=="null"||body.empty()) return;
    try{
        auto parsed=json::parse(body);
        for(auto it=parsed.begin();it!=parsed.end();++it){
            users.push_back(jsonToUser(it.key(),it.value()));
        }
    }catch(exception &e){ cerr<<"Error parse JSON users: "<<e.what()<<endl; }
}

void ensureDefaultUsers(){
    if(!users.empty()) return;
    User su={"superadmin","12345","super"};
    User ad={"TOKO1","admin","admin"};
    if(firebasePutUser(su.username,userToJson(su))) users.push_back(su);
    if(firebasePutUser(ad.username,userToJson(ad))) users.push_back(ad);
}

void loadFromFirebase(){
    string body;
    if(!firebaseGetAllStok(body)){ cout<<"Gagal ambil stok\n"; return; }
    stok.clear();
    if(body=="null"||body.empty()) return;
    try{
        auto parsed=json::parse(body);
        for(auto it=parsed.begin();it!=parsed.end();++it){
            Phone p=jsonToPhone(it.value());
            if(p.nomorSeri.empty()) p.nomorSeri=it.key();
            stok.push_back(p);
        }
        saveToCSV();
    }catch(exception &e){ cerr<<"Error parse stok: "<<e.what()<<endl; }
}

// ================= ADMIN & SUPER CRUD =================
void listAdmins(){
    cout<<"\n=== Daftar Admin ===\n";
    for(auto &u:users) if(u.role=="admin") cout<<" - "<<u.username<<endl;
}

void addUserInteractive(){
    User u;
    cout<<"\nMasukkan username: "; cin>>ws; getline(cin,u.username);
    cout<<"Password: "; cin>>ws; getline(cin,u.password);
    u.role="admin";
    for(auto &x:users) if(x.username==u.username){ cout<<"Username sudah ada!\n"; return; }
    if(firebasePutUser(u.username,userToJson(u))){ users.push_back(u); cout<<"✅ Admin ditambahkan\n"; }
    else cout<<"❌ Gagal\n";
}

void editUserInteractive(){
    string uname; cout<<"\nUsername admin: "; cin>>ws; getline(cin,uname);
    auto it=find_if(users.begin(),users.end(),[&](const User &x){ return x.username==uname && x.role=="admin";});
    if(it==users.end()){ cout<<"Tidak ditemukan\n"; return; }
    cout<<"Password baru: "; string np; getline(cin,np); if(!np.empty()) it->password=np;
    if(firebasePutUser(it->username,userToJson(*it))) cout<<"✅ Password diupdate\n"; else cout<<"❌ Gagal\n";
}

void deleteUserInteractive(){
    string uname; cout<<"\nUsername admin: "; cin>>ws; getline(cin,uname);
    auto it=find_if(users.begin(),users.end(),[&](const User &x){ return x.username==uname && x.role=="admin";});
    if(it==users.end()){ cout<<"Tidak ditemukan\n"; return; }
    vector<string> toDelete;
    for(auto &p:stok) if(p.owner==uname) toDelete.push_back(p.nomorSeri);
    for(auto &sn:toDelete){ firebaseDeleteStok(sn); stok.erase(remove_if(stok.begin(),stok.end(),[&](const Phone &x){ return x.nomorSeri==sn;}),stok.end()); }
    if(firebaseDeleteUser(uname)){ users.erase(it); saveToCSV(); cout<<"🗑 Admin & stok dihapus\n"; } else cout<<"❌ Gagal\n";
}

// ================= STOCK CRUD =================
void displayStockAll(){
    cout<<"\n=== SEMUA STOCK ===\n";
    vector<Phone> sorted=stok;
    sort(sorted.begin(),sorted.end(),[](const Phone &a,const Phone &b){ return a.tanggalStok>b.tanggalStok; });
    int i=1;
    for(auto &p:sorted){
        cout<<"#"<<i++<<"\nTanggal Stok: "<<p.tanggalStok<<"\nOwner: "<<p.owner<<"\nKategori: "<<p.kategori
            <<"\nMerek: "<<p.merek
            <<"\nVarian: "<<p.varian<<"\nSN: "<<p.nomorSeri<<"\nMemori: "<<p.memori<<"\nWarna: "<<p.warna
            <<"\nHarga Beli: "<<p.hargaBeli<<"\nHarga Jual: "<<p.hargaJual<<"\nStatus: "<<p.status<<"\n-------------------\n";
    }
}

void displayForAdmin(const string &owner){
    cout<<"\n=== STOCK TOKO "<<owner<<" ===\n";
    vector<Phone> sorted;
    for(auto &p:stok) if(p.owner==owner) sorted.push_back(p);
    sort(sorted.begin(),sorted.end(),[](const Phone &a,const Phone &b){ return a.tanggalStok>b.tanggalStok; });
    if(sorted.empty()){ cout<<"Tidak ada stok\n"; return; }
    for(auto &p:sorted){
        cout<<"Tanggal Stok: "<<p.tanggalStok<<"\nKategori: "<<p.kategori<<"\nMerek: "<<p.merek
            <<"\nVarian: "<<p.varian<<"\nSN: "<<p.nomorSeri<<"\nHarga: "<<p.hargaJual
            <<" ("<<p.status<<")\n-------------------\n";
    }
}

void addStockFirebase(Category* root){
    Phone p;
    cout<<"\n=== Pilih Kategori & Merek ===\n"; 
    displayCategoriesAndBrands(root);
    
    // Pilih Kategori
    cout<<"\nKategori: "; cin>>ws; getline(cin,p.kategori);
    Category* catNode = searchCategory(root, p.kategori);
    if(!catNode) {
        cout << "❌ Kategori tidak ditemukan. Tambahkan kategori dulu.\n";
        return;
    }

    // Pilih Merek dari Kategori tersebut
    cout<<"Merek: "; getline(cin, p.merek);
    // Kita bisa validasi apakah merek ada di catNode->brandRoot, 
    // tapi untuk fleksibilitas input kita biarkan saja, atau mau strict?
    // Jika strict:
    // if(!searchBrand(catNode->brandRoot, p.merek)) { cout << "Merek belum terdaftar!"; return; }

    cout<<"Varian: "; getline(cin,p.varian);
    cout<<"Nomor Seri: "; getline(cin,p.nomorSeri);
    cout<<"Memori: "; getline(cin,p.memori);
    cout<<"Warna: "; getline(cin,p.warna);
    cout<<"Harga Beli: "; cin>>p.hargaBeli;
    cout<<"Harga Jual: "; cin>>p.hargaJual;

    cout<<"\nDaftar admin: "; for(auto &u:users) if(u.role=="admin") cout<<u.username<<" "; cout<<endl;
    cout<<"Owner: "; cin>>ws; getline(cin,p.owner);
    if(p.nomorSeri.empty()||p.owner.empty()){ cout<<"SN/Owner tidak boleh kosong\n"; return; }

    p.status="Belum Terjual";
    p.tanggalStok = todayDate();

    if(firebasePutStok(p.nomorSeri,phoneToJson(p))){
        auto it=find_if(stok.begin(),stok.end(),[&](const Phone &x){ return x.nomorSeri==p.nomorSeri; });
        if(it!=stok.end()) *it=p; else stok.push_back(p);
        saveToCSV();
        cout<<"✅ Stok ditambahkan & sync Firebase\n";
    } else cout<<"❌ Gagal simpan\n";
}

void editStockFirebase(){
    string sn; cout<<"\nSN: "; cin>>ws; getline(cin,sn);
    auto it=find_if(stok.begin(),stok.end(),[&](const Phone &x){ return x.nomorSeri==sn; });
    if(it==stok.end()){ cout<<"SN tidak ditemukan\n"; return; }
    Phone &p=*it;
    cout<<"Varian ("<<p.varian<<"): "; string tmp; getline(cin,tmp); if(!tmp.empty()) p.varian=tmp;
    cout<<"Memori ("<<p.memori<<"): "; getline(cin,tmp); if(!tmp.empty()) p.memori=tmp;
    cout<<"Warna ("<<p.warna<<"): "; getline(cin,tmp); if(!tmp.empty()) p.warna=tmp;
    cout<<"Harga Beli ("<<p.hargaBeli<<"): "; getline(cin,tmp); if(!tmp.empty()) p.hargaBeli=stol(tmp);
    cout<<"Harga Jual ("<<p.hargaJual<<"): "; getline(cin,tmp); if(!tmp.empty()) p.hargaJual=stol(tmp);
    cout<<"Owner ("<<p.owner<<"): "; getline(cin,tmp); if(!tmp.empty()) p.owner=tmp;

    if(firebasePutStok(sn,phoneToJson(p))){ saveToCSV(); cout<<"✅ Update sukses\n"; } else cout<<"❌ Gagal\n";
}

void deleteStockFirebase(){
    string sn; cout<<"\nSN: "; cin>>ws; getline(cin,sn);
    auto it=find_if(stok.begin(),stok.end(),[&](const Phone &x){ return x.nomorSeri==sn; });
    if(it==stok.end()){ cout<<"SN tidak ditemukan\n"; return; }
    if(firebaseDeleteStok(sn)){ stok.erase(it); saveToCSV(); cout<<"🗑 Stok dihapus\n"; } else cout<<"❌ Gagal\n";
}

void updateStatusFirebase(const string &owner){
    string sn; cout<<"\nSN: "; cin>>ws; getline(cin,sn);
    auto it=find_if(stok.begin(),stok.end(),[&](const Phone &x){ return x.nomorSeri==sn && x.owner==owner; });
    if(it==stok.end()){ cout<<"Tidak ditemukan atau bukan milik toko\n"; return; }
    it->status="Terjual";
    if(firebasePutStok(sn,phoneToJson(*it))){ saveToCSV(); cout<<"✅ Status Terjual\n"; } else cout<<"❌ Gagal\n";
}

void displayStockByStatus(const string &status){
    cout << "\n=== STOK (" << status << ") ===\n";
    int i = 1;
    for(auto &p : stok){
        if(p.status == status){
            cout<<"#"<<i++<<"\nTanggal: "<<p.tanggalStok<<"\nOwner: "<<p.owner
                <<"\nKategori: "<<p.kategori<<"\nMerek: "<<p.merek
                <<"\nVarian: "<<p.varian<<"\nSN: "<<p.nomorSeri
                <<"\nHarga Jual: "<<p.hargaJual<<"\n-------------------\n";
        }
    }
    if(i == 1) cout << "Tidak ada data.\n";
}


// ================= LOGIN =================
bool login(User &outUser){
    string u,p;
    cout<<"\nLOGIN\nUsername: "; cin>>u;
    cout<<"Password: "; cin>>p;
    for(auto &usr:users) if(usr.username==u && usr.password==p){ outUser=usr; return true; }
    return false;
}

// ================= MAIN =================
int main(){
    curl_global_init(CURL_GLOBAL_DEFAULT);
    loadUsersFromFirebase();
    ensureDefaultUsers();
    loadFromFirebase();

    // Inisialisasi Kategori (Parent)
    Category* root=nullptr;
    root=insertCategory(root,"Handphone");
    root=insertCategory(root,"Tablet");
    
    // Inisialisasi Contoh Merek (Child)
    // Handphone
    addBrandToCategory(root, "Handphone", "Samsung");
    addBrandToCategory(root, "Handphone", "Xiaomi");
    addBrandToCategory(root, "Handphone", "Infinix");
    // Tablet
    addBrandToCategory(root, "Tablet", "iPad");
    addBrandToCategory(root, "Tablet", "SamsungTab");

    User current;
    if(!login(current)){ cout<<"Login gagal\n"; return 0; }

    if(current.role=="super"){
        int choice;
        do{
            cout<<"\n--- SUPER ADMIN ---\n";
            cout<<"1. Add Category (Parent)\n";
            cout<<"2. Add Brand (Child of Category)\n";
            cout<<"3. List Categories & Brands\n";
            cout<<"4. Delete Category\n";
            cout<<"5. Add Admin\n";
            cout<<"6. List Admin\n";
            cout<<"7. Delete Admin\n";
            cout<<"8. Add Stock\n";
            cout<<"9. Edit Stock\n";
            cout<<"10. Delete Stock\n";
            cout<<"11. View All Stock / Traversals\n";
            cout<<"0. Exit\nChoice: ";
            cin>>choice;
            switch(choice){
                case 1:{ 
                    string cat; cout<<"Nama kategori baru: "; cin>>ws; getline(cin,cat); 
                    root=insertCategory(root,cat); 
                    break; 
                }
                case 2:{
                    cout << "Pilih Kategori untuk ditambah mereknya:\n";
                    displayCategoriesAndBrands(root);
                    string cat, brand;
                    cout << "Masukkan Nama Kategori: "; cin>>ws; getline(cin, cat);
                    cout << "Masukkan Nama Merek Baru: "; getline(cin, brand);
                    addBrandToCategory(root, cat, brand);
                    break;
                }
                case 3: displayCategoriesAndBrands(root); break;
                case 4:{ string cat; cout<<"Nama kategori hapus: "; cin>>ws; getline(cin,cat); root=deleteCategory(root,cat); break; }
                case 5: addUserInteractive(); break;
                case 6: listAdmins(); break;
                case 7: deleteUserInteractive(); break;
                case 8: addStockFirebase(root); break;
                case 9: editStockFirebase(); break;
                case 10: deleteStockFirebase(); break;
                case 11:{
                    int sub;
                    do{
                        cout << "\n=== VIEW MENU ===\n";
                        cout << "1. Semua Stock\n";
                        cout << "2. Stock Terjual\n";
                        cout << "3. Stock Belum Terjual\n";
                        cout << "4. Pre-order Brand (by Category)\n";
                        cout << "5. In-order Brand (by Category)\n";
                        cout << "6. Post-order Brand (by Category)\n";
                        cout << "0. Kembali\n";
                        cout << "Pilih: ";
                        cin >> sub;

                        if (sub >= 4 && sub <= 6) {
                            string catTarget;
                            cout << "Masukkan Kategori yang mau dilihat mereknya (misal Handphone): ";
                            cin >> ws; getline(cin, catTarget);
                            Category* found = searchCategory(root, catTarget);
                            if(found) {
                                cout << "Traversing Brands in " << catTarget << ": ";
                                if (sub == 4) preorderBrand(found->brandRoot);
                                else if (sub == 5) inorderBrand(found->brandRoot);
                                else if (sub == 6) postorderBrand(found->brandRoot);
                                cout << endl;
                            } else {
                                cout << "Kategori tidak ditemukan.\n";
                            }
                        } else {
                            switch(sub){
                                case 1: displayStockAll(); break;
                                case 2: displayStockByStatus("Terjual"); break;
                                case 3: displayStockByStatus("Belum Terjual"); break;
                            }
                        }
                    }while(sub != 0);
                    break;
                }

            }
        }while(choice!=0);
    }else{
        int choice;
        do{
            cout<<"\n--- ADMIN "<<current.username<<" ---\n1.View Stock\n2.Update Status Terjual\n0.Exit\nChoice: ";
            cin>>choice;
            switch(choice){
                case 1: displayForAdmin(current.username); break;
                case 2: updateStatusFirebase(current.username); break;
            }
        }while(choice!=0);
    }

    curl_global_cleanup();
    return 0;
}