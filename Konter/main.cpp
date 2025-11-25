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

// ================= BINARY TREE CATEGORY =================
struct Category {
    string name;
    Category* left;
    Category* right;
    Category(string n): name(n), left(nullptr), right(nullptr) {}
};

Category* insertCategory(Category* root, string name){
    if(!root) return new Category(name);
    if(name < root->name) root->left = insertCategory(root->left, name);
    else if(name > root->name) root->right = insertCategory(root->right, name);
    return root;
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
        root->right=deleteCategory(root->right,temp->name);
    }
    return root;
}

void displayCategories(Category* root){
    if(!root) return;
    displayCategories(root->left);
    cout<<" - "<<root->name<<endl;
    displayCategories(root->right);
}

// ================= STRUCTS =================
struct User{
    string username, password, role; // role: super/admin
};

struct Phone{
    string kategori,varian,nomorSeri,memori,warna,status="Belum Terjual",owner,tanggalStok;
    long hargaBeli=0,hargaJual=0;
};

// ================= IN-MEMORY STORAGE =================
vector<User> users;
vector<Phone> stok;

// ================= CSV LOCAL BACKUP =================
void saveToCSV(){
    ofstream file("database.csv");
    file<<"Kategori,Varian,Nomor Seri,Memori,Warna,Harga Beli,Harga Jual,Status,Owner,Tanggal Stok\n";
    for(auto &p:stok){
        file<<p.kategori<<","<<p.varian<<","<<p.nomorSeri<<","<<p.memori<<","<<p.warna<<","
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
    return { {"kategori",p.kategori},{"varian",p.varian},{"nomorSeri",p.nomorSeri},
             {"memori",p.memori},{"warna",p.warna},{"hargaBeli",p.hargaBeli},{"hargaJual",p.hargaJual},
             {"status",p.status},{"owner",p.owner},{"tanggalStok",p.tanggalStok} };
}

Phone jsonToPhone(const json &j){
    Phone p;
    if(j.contains("kategori")) p.kategori=j["kategori"].get<string>();
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
        cout<<"Tanggal Stok: "<<p.tanggalStok<<"\nVarian: "<<p.varian<<"\nSN: "<<p.nomorSeri<<"\nHarga: "<<p.hargaJual
            <<" ("<<p.status<<")\n-------------------\n";
    }
}

void addStockFirebase(Category* root){
    Phone p;
    cout<<"\n=== Pilih Kategori ===\n"; displayCategories(root);
    cout<<"Kategori: "; cin>>p.kategori;
    cout<<"Varian: "; cin>>ws; getline(cin,p.varian);
    cout<<"Nomor Seri: "; getline(cin,p.nomorSeri);
    cout<<"Memori: "; getline(cin,p.memori);
    cout<<"Warna: "; getline(cin,p.warna);
    cout<<"Harga Beli: "; cin>>p.hargaBeli;
    cout<<"Harga Jual: "; cin>>p.hargaJual;

    cout<<"\nDaftar admin: "; for(auto &u:users) if(u.role=="admin") cout<<u.username<<" "; cout<<endl;
    cout<<"Owner: "; cin>>ws; getline(cin,p.owner);
    if(p.nomorSeri.empty()||p.owner.empty()){ cout<<"SN/Owner tidak boleh kosong\n"; return; }

    p.status="Belum Terjual";
    p.tanggalStok = todayDate(); // otomatis tanggal hari ini

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

    Category* root=nullptr;
    root=insertCategory(root,"Android");
    root=insertCategory(root,"iPhone");

    User current;
    if(!login(current)){ cout<<"Login gagal\n"; return 0; }

    if(current.role=="super"){
        int choice;
        do{
            cout<<"\n--- SUPER ADMIN ---\n1.Add Category\n2.List Categories\n3.Delete Category\n4.Add Admin\n5.List Admin\n6.Delete Admin\n7.Add Stock\n8.Edit Stock\n9.Delete Stock\n10.View All Stock\n0.Exit\nChoice: ";
            cin>>choice;
            switch(choice){
                case 1:{ string cat; cout<<"Nama kategori: "; cin>>ws; getline(cin,cat); root=insertCategory(root,cat); break; }
                case 2: displayCategories(root); break;
                case 3:{ string cat; cout<<"Nama kategori hapus: "; cin>>ws; getline(cin,cat); root=deleteCategory(root,cat); break; }
                case 4: addUserInteractive(); break;
                case 5: listAdmins(); break;
                case 6: deleteUserInteractive(); break;
                case 7: addStockFirebase(root); break;
                case 8: editStockFirebase(); break;
                case 9: deleteStockFirebase(); break;
                case 10: displayStockAll(); break;
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
