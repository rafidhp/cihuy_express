#include <iostream>
#include <stack>
#include <queue>
#include <cstdlib>
#include <ctime>
#include <set>
#include <map>
#include <vector>
#include <fstream>
#include <regex>
#include <algorithm>
#include <iomanip>
#ifdef _WIN32
    #include <windows.h>
    #include <fcntl.h>
    #include <io.h>
#else
    #include <unistd.h>
#endif

using namespace std;

// map size
const int WIDTH = 30;
const int HEIGHT = 15;
char game_map[HEIGHT][WIDTH];

// emoji
const string EMOJI_COURIER = "🛵"; // motorcycle
const string EMOJI_PACKAGE = "📦"; // package
const string EMOJI_HOUSE = "🏠";   // house
const string EMOJI_WALL = "🧱";    // wall
const string EMOJI_CLOCK = "⏰";   // clock
const string EMOJI_ROAD = "  ";    // street

bool use_emojis = false;

// courier start position
int courierX = WIDTH / 2;
int courierY = HEIGHT / 2;

stack<char> carriedPackages; // stack untuk menyimpan paket yang sedang dibawa
set<string> registered_users; // menyimpan daftar pengguna yang sudah terdaftar
vector<pair<int, int> > house_locations; // menyimpan koordinat lokasi semua rumah
map<pair<int, int>, vector<pair<int, int> > > graph; // menyimpan peta jalur antar lokasi dalam bentuk graf
map<string, int> user_scores;
bool all_packages_delivered();
bool is_time_up();
int get_remaining_time();
void show_win_animation();
void post_game_options();
void show_leaderboard();
void show_intro();
void ask_house_count();

string current_user;
int score = 0;
int old_highscore = 0;
int house_count = 3; // number of houses (default 3)
time_t start_time;
int TIME_LIMIT = 60;
int paketCount = 1;
int delivered_packages = 0; // tracking paket yang sudah diantar
int lives = 3; // jumlah nyawa
const int MAX_LIVES = 3; // maksimal nyawa

// helper function to check if terminal supports UTF-8
bool check_utf8_support() {
    #ifdef _WIN32
        // check if Windows terminal supports UTF-8
        UINT originalCP = GetConsoleOutputCP();
        bool success = SetConsoleOutputCP(CP_UTF8);
        if (success) {
            SetConsoleOutputCP(originalCP);
            return true;
        }
        return false;
    #else
        // most Unix-like systems support UTF-8
        const char *term = getenv("TERM");
        if (term && (string(term).find("xterm") != string::npos ||
                    string(term).find("utf") != string::npos))
            return true;
        return false;
    #endif
}

void setup_terminal() {
    #ifdef _WIN32
        // set UTF-8 code page
        SetConsoleOutputCP(CP_UTF8);
        // enable virtual terminal processing for ANSI colors
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD dwMode = 0;
        GetConsoleMode(hOut, &dwMode);
        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, dwMode);
    #endif
}

void load_users() {
    ifstream infile("users.txt");

    string username;
    int highscore;

    while (infile >> username >> highscore) {
        registered_users.insert(username);
        user_scores[username] = highscore;
    }
}

void save_user(const string &username, int score) {
    user_scores[username] = score;

    ofstream outfile("users.txt", ios::trunc);
    for (map<string, int>::const_iterator it = user_scores.begin(); it != user_scores.end(); ++it) {
        outfile << it->first << " " << it->second << endl;
    }
}

void login_or_regis() {
    load_users();
    string username;
    regex username_pattern("^[A-Za-z0-9_]{3,}$");

    do {
        cout << "\nMasukkan username Anda (huruf, angka, underscore, min 3 karakter): "; getline(cin, username);

        // trim spaces from username
        username.erase(0, username.find_first_not_of(" \t\n\r\f\v"));
        username.erase(username.find_last_not_of(" \t\n\r\f\v") + 1);

        if(username.empty()) {
            cout << "\nUsername tidak boleh kosong. Silakan coba lagi!\n";
        } else if(username == "q" || username == "Q") {
            cout << "\n 👋 Terima kasih telah bermain " << current_user << "! Sampai jumpa lagi! 👋\n\n";
            exit(0);
        } else if (!regex_match(username, username_pattern)) {
            cout << "\nUsername tidak valid. Silakan coba lagi.\n";
        }
    } while (!regex_match(username, username_pattern));

    current_user = username;

    if (registered_users.count(username)) {
        old_highscore = user_scores[username];
        cout << "\nSelamat datang kembali " << username << "! High Score kamu sebelumnya: " << user_scores[username] << " point!" << endl << endl;
    } else {
        cout << "\nRegistrasi baru untuk " << username << " berhasil! Selamat bermain!\n\n";
        user_scores[username] = 0;
        old_highscore = 0;
        save_user(username, 0);
    }

    #ifdef _WIN32
        system("pause");
        cout << "\n";
    #else
        cout << "Tekan ENTER untuk mulai...";
        cin.ignore();
        cin.get();
    #endif
}

// build graph for map navigation
void build_graph() {
    graph.clear();

    // define directions: up, right, down, left
    int dx[4] = {0, 1, 0, -1};
    int dy[4] = {-1, 0, 1, 0};

    // check each cell and create edges to neighbors
    for (int y = 1; y < HEIGHT - 1; y++) {
        for (int x = 1; x < WIDTH - 1; x++) {
            // skip walls
            if (game_map[y][x] == '#')
                continue;

            pair<int, int> current = make_pair(y, x);
            vector<pair<int, int> > neighbors; // fixed spacing

            // check all four directions
            for (int d = 0; d < 4; d++) {
                int ny = y + dy[d];
                int nx = x + dx[d];

                // if the neighbor is valid and not a wall, add it as an edge
                if (ny >= 1 && ny < HEIGHT - 1 && nx >= 1 && nx < WIDTH - 1 && game_map[ny][nx] != '#') {
                    neighbors.push_back(make_pair(ny, nx));
                }
            }

            // add all neighbors to the graph
            graph[current] = neighbors;
        }
    }
}

int calculate_shortest_path(int startY, int startX, int targetY, int targetX) {
    queue<pair<int, int> > q; // fixed spacing
    int dist[HEIGHT][WIDTH];
    bool visited[HEIGHT][WIDTH] = {false};

    for (int i = 0; i < HEIGHT; i++) {
        for (int j = 0; j < WIDTH; j++) {
            dist[i][j] = -1;
        }
    }

    pair<int, int> start = make_pair(startY, startX);
    q.push(start);
    dist[startY][startX] = 0;
    visited[startY][startX] = true;

    while (!q.empty()) {
        pair<int, int> current = q.front();
        int y = current.first;
        int x = current.second;
        q.pop();

        if (y == targetY && x == targetX) {
            return dist[y][x]; // shortest path found
        }

        // use the graph structure for navigation
        vector<pair<int, int> > neighbors = graph[make_pair(y, x)]; // fixed spacing
        for (vector<pair<int, int> >::iterator it = neighbors.begin(); it != neighbors.end(); ++it) {
            int ny = it->first;
            int nx = it->second;

            if (!visited[ny][nx]) {
                visited[ny][nx] = true;
                dist[ny][nx] = dist[y][x] + 1;
                q.push(make_pair(ny, nx));
            }
        }
    }

    return -1; // no path found
}

// find nearest house from current position
pair<int, int> find_nearest_house() {
    int min_dist = INT_MAX;
    pair<int, int> nearest = make_pair(-1, -1);

    for (size_t i = 0; i < house_locations.size(); i++) {
        int dist = calculate_shortest_path(courierY, courierX, 
                                        house_locations[i].first, 
                                        house_locations[i].second);
        if (dist > 0 && dist < min_dist) {
            min_dist = dist;
            nearest = house_locations[i];
        }
    }
    return nearest;
}

void show_animation() {
    const string frames[] = {
        " \n  Horeee!!! Paket berhasil diantar!\n     \\(^_^)/ \n        |\n       / \\\n        ",
        "\n  Paket diterima dengan bahagia!\n     (^o^)/\n      \\|\n      / \\\n        ",
        "\n  Terima kasih, kurir hebat! Cihuyyyy!!\n     (^-^)\n       |\n      / \\\n       "};

    for (int i = 0; i < 3; ++i) {
        #ifdef _WIN32
            system("cls");
        #else
            system("clear");
        #endif

        cout << frames[i] << endl;
        cout << "\nSkor: " << score << endl;

        #ifdef _WIN32
            Sleep(700);
        #else
            usleep(700000); // 0.7 detik per frame
        #endif
    }
}

void add_house_at_random_location() {
    int attempts = 0;
    while (attempts < 100) { // limit attempts to avoid infinite loop
        int x = rand() % (WIDTH - 2) + 1;
        int y = rand() % (HEIGHT - 2) + 1;

        if (game_map[y][x] == ' ' && !(x == courierX && y == courierY)) {
            game_map[y][x] = 'H';
            house_locations.push_back(make_pair(y, x));
            break;
        }
        attempts++;
    }
}

void add_package() {
    int attempts = 0;
    while (attempts < 100) {
        int x = rand() % (WIDTH - 2) + 1;
        int y = rand() % (HEIGHT - 2) + 1;

        if (game_map[y][x] == ' ' && !(x == courierX && y == courierY)) {
            game_map[y][x] = 'P';
            break;
        }
        attempts++;
    }
}

// fungsi untuk menghasilkan peta
void generateMap() {
    for (int i = 0; i < HEIGHT; i++) {
        for (int j = 0; j < WIDTH; j++) {
            if (i == 0 || i == HEIGHT - 1 || j == 0 || j == WIDTH - 1) {
                game_map[i][j] = '#'; // tembok pinggir
            } else {
                game_map[i][j] = ' '; // jalan
            }
        }
    }

    // clear house locations
    house_locations.clear();

    // add multiple houses at random locations
    for (int i = 0; i < house_count; i++) {
        add_house_at_random_location();
    }

    // added some random walls inside the map
    int wallCount = WIDTH / 2; // random number of walls (scaled to map size)
    int wallAdded = 0;

    while (wallAdded < wallCount) {
        int x = rand() % (WIDTH - 2) + 1;
        int y = rand() % (HEIGHT - 2) + 1;

        // make sure its not at the courier or home location
        bool isHouse = false;
        for (vector<pair<int, int> >::iterator it = house_locations.begin(); it != house_locations.end(); ++it) { // Fixed spacing
            if (it->first == y && it->second == x) {
                isHouse = true;
                break;
            }
        }

        // make sure not at the courier or home location
        if (game_map[y][x] == ' ' && !(x == courierX && y == courierY) && !isHouse) {
            game_map[y][x] = '#'; // walls in map
            wallAdded++;
        }
    }

    // adding some packages randomly
    int paketCount = house_count + 1; // number of packages (one more than houses)
    int added = 0;

    while (added < paketCount) {
        int x = rand() % (WIDTH - 2) + 1;
        int y = rand() % (HEIGHT - 2) + 1;

        // make sure its not on top of the house, courier, or wall
        bool isHouse = false;
        for (vector<pair<int, int> >::iterator it = house_locations.begin(); it != house_locations.end(); ++it) { // Fixed spacing
            if (it->first == y && it->second == x) {
                isHouse = true;
                break;
            }
        }

        // make sure it is not on top of the house, courier, or wall
        if (game_map[y][x] == ' ' && !(x == courierX && y == courierY) && !isHouse) {
            game_map[y][x] = 'P';
            added++;
        }
    }

    // build navigation graph
    build_graph();
}

// check if a position is a house
bool is_house(int y, int x) {
    for (vector<pair<int, int> >::iterator it = house_locations.begin(); it != house_locations.end(); ++it) { // Fixed spacing
        if (it->first == y && it->second == x) {
            return true;
        }
    }
    return false;
}

// function to display map with emoji support
void printMap() {
    #ifdef _WIN32
        system("cls");
    #else
        system("clear");
    #endif

    for (int i = 0; i < HEIGHT; i++) {
        for (int j = 0; j < WIDTH; j++) {
            if (i == courierY && j == courierX) {
                if (use_emojis) {
                    cout << EMOJI_COURIER;
                } else {
                    cout << "C ";
                }
            }
            else if (is_house(i, j)) {
                if (use_emojis) {
                    cout << EMOJI_HOUSE;
                } else {
                    cout << "H ";
                }
            } else {
                if (use_emojis) {
                    if (game_map[i][j] == '#') {
                        cout << EMOJI_WALL;
                    } else if (game_map[i][j] == 'P') {
                        cout << EMOJI_PACKAGE;
                    } else {
                        cout << EMOJI_ROAD;
                    }
                } else {
                    cout << game_map[i][j] << " ";
                }
            }
        }
        cout << endl;
    }

    cout << "\nSkor: " << score << endl;
    cout << "Paket dibawa: " << carriedPackages.size() << "/3" << endl;
    cout << "Paket diantar: " << delivered_packages << endl;
    cout << "❤️  Nyawa: " << lives << "/" << MAX_LIVES << endl;
    cout << EMOJI_CLOCK << " Sisa waktu: " << get_remaining_time() << " detik" << endl;

   // display direction to nearest house if carrying packages
if (!house_locations.empty() && !carriedPackages.empty()) {
    pair<int, int> nearest = find_nearest_house();

    if (nearest.first != -1) {
        string direction = "";

        // Ubah dari mata angin ke arah sederhana
        if (nearest.first < courierY)
            direction += "Atas";
        else if (nearest.first > courierY)
            direction += "Bawah";

        if (nearest.second < courierX) {
            if (!direction.empty())
                direction += "-";
            direction += "Kiri";
        } else if (nearest.second > courierX) {
            if (!direction.empty())
                direction += "-";
            direction += "Kanan";
        }

        cout << "Arah ke rumah terdekat: " << direction << endl;
    }
}

    cout << "\nKontrol: WASD = Bergerak, Q = Keluar" << endl;
}

// function for direction control
void moveCourier(char direction) {
    // calculate target position
    int nextX = courierX;
    int nextY = courierY;

    if (direction == 'w')
        nextY--;
    if (direction == 's')
        nextY++;
    if (direction == 'a')
        nextX--;
    if (direction == 'd')
        nextX++;

    // check if it hits the wall
    if (game_map[nextY][nextX] == '#') {
        lives--; // kurangi nyawa
        
        #ifdef _WIN32
            system("cls");
        #else
            system("clear");
        #endif

        cout << "💥 OUCH! Kamu menabrak tembok! 💥" << endl;
        cout << "❤️  Nyawa tersisa: " << lives << "/" << MAX_LIVES << endl;
        
        if (lives <= 0) {
            // Game Over jika nyawa habis
            cout << "\n💀 GAME OVER! Nyawa kamu sudah habis! 💀" << endl;
            cout << "Skor akhir: " << score << endl;
            cout << "High score sebelumnya: " << old_highscore << " point" << endl;

            if (score > old_highscore) {
                cout << "\n🎉 Selamat! Skor baru kamu (" << score << ") adalah rekor baru! 🎉\n";
                cout << "Total paket berhasil diantar: " << delivered_packages << endl;
                save_user(current_user, score);
                old_highscore = score;
            } else {
                cout << "\nSkor kamu belum mengalahkan rekor sebelumnya 😢\n";
                cout << "Skor tertinggi kamu tetap: " << old_highscore << " point\n";
                cout << "Total paket berhasil diantar: " << delivered_packages << endl;
            }

            #ifdef _WIN32
                cout << "\n";
                system("pause");
            #else
                cout << "\nTekan ENTER untuk melanjutkan...";
                cin.ignore();
                cin.get();
            #endif

            post_game_options();
        } else {
            // Masih ada nyawa, lanjutkan permainan
            cout << "\nHati-hati! Jangan menabrak tembok lagi!" << endl;
            
            #ifdef _WIN32
                Sleep(2000); // tunggu 2 detik
            #else
                usleep(2000000);
            #endif
            
            return; // tidak bergerak jika menabrak tembok
        }
    }

    // courier position update (hanya jika tidak menabrak tembok)
    courierX = nextX;
    courierY = nextY;
}


void pickUpPackage() {
    if (carriedPackages.size() < 3) {
        if (game_map[courierY][courierX] == 'P') {
            carriedPackages.push('P');
            game_map[courierY][courierX] = ' '; // remove package from map
            cout << "Paket diambil! (" << carriedPackages.size() << "/3)" << endl;

            #ifdef _WIN32
                Sleep(500);
            #else
                usleep(500000); // 0.5 second feedback
            #endif
        }
    }
}

// remove a house from locations
void remove_house(int y, int x) {
    for (vector<pair<int, int> >::iterator it = house_locations.begin(); it != house_locations.end(); ++it) { // Fixed spacing
        if (it->first == y && it->second == x) {
            house_locations.erase(it);
            break;
        }
    }
}

// function to deliver packages
void deliverPackage() {
    if (is_house(courierY, courierX) && !carriedPackages.empty()) {
        carriedPackages.pop();
        delivered_packages++;

        int level_scores[] = {10, 8, 6, 4, 2};
        score += level_scores[min(house_count - 1, 4)];

        // remove current house
        remove_house(courierY, courierX);
        game_map[courierY][courierX] = ' '; // remove from map

        show_animation();

        // add a new house n package at random location
        add_house_at_random_location();
        add_package();

        // rebuild the graph after map changes
        build_graph();

        TIME_LIMIT += 5;
    }
}

bool all_packages_delivered() {
    // Cek apakah masih ada paket 'P' di map
    for (int i = 0; i < HEIGHT; i++) {
        for (int j = 0; j < WIDTH; j++) {
            if (game_map[i][j] == 'P') {
                return false;
            }
        }
    }
    // Cek apakah courier masih membawa paket
    return carriedPackages.empty();
}

void show_win_animation() {
    const string win_frame[] = {
        "\n🏆 SELAMAT! KAMU TELAH MENGANTARKAN SEMUA PAKET! 🏆\n",
        "     \\(^_^)/     🎉🎉🎉\n\n",
        "   Kurir terbaik sepanjang masa!\n",
    };

    #ifdef _WIN32
        system("cls");
    #else
        system("clear");
    #endif

    for (int i = 0; i < 3; ++i) {
        cout << win_frame[i];

        #ifdef _WIN32
            Sleep(700);
        #else
            usleep(700000); // 0.7 seconds between lines
        #endif
    }

    cout << "Skor akhir kamu: " << score << " point" << endl;
    cout << "High score sebelumnya: " << old_highscore << " point" << endl;

    if (score > old_highscore) {
        cout << "\n🎉 Selamat! Skor baru kamu (" << score << ") adalah rekor baru! 🎉\n";
        save_user(current_user, score);
        old_highscore = score;
    } else {
        cout << "\nSkor kamu belum mengalahkan rekor sebelumnya 😢\n";
        cout << "Skor tertinggi kamu tetap: " << old_highscore << " point\n";
    }

    #ifdef _WIN32
        cout << "\n";
        system("pause");
    #else
        cout << "\nTekan ENTER untuk mulai...";
        cin.ignore();
        cin.get();
    #endif

    post_game_options();
}

void post_game_options() {
    string choice;

    while (true) {
        #ifdef _WIN32
            system("cls");
        #else
            system("clear");
        #endif

        cout << "=======================================================" << endl;
        cout << "          CIHUY EXPRESS - DELIVERY GAME                " << endl;
        cout << "=======================================================" << endl;
        cout << "\n[1] Main lagiiii!!!\n[2] Lihat leaderbord\n[3] Keluar ah cape\n\nSilakan pilih opsi (1-3): ";
        cin >> choice;

        if (choice == "1") {
            score = 0;
            delivered_packages = 0;
            lives = MAX_LIVES; // TAMBAHAN: Reset nyawa
            carriedPackages = stack<char>();
            TIME_LIMIT = 45;

            courierX = WIDTH / 2;
            courierY = HEIGHT / 2;

            show_intro();
            ask_house_count();
            generateMap();
            start_time = time(NULL);
            break;
        } else if (choice == "2") {
            show_leaderboard();
            break;
        } else if (choice == "3") {
            cout << "\n 👋 Terima kasih telah bermain " << current_user << "! Sampai jumpa lagi! 👋\n\n";
            exit(0);
        } else {
            cout << "❌ Pilihan tidak valid! Silakan pilih 1, 2, atau 3." << endl;

            #ifdef _WIN32
                Sleep(2000);
            #else
                usleep(2000000);
            #endif
        }
    }
}

void show_leaderboard() {
    vector<pair<string, int> > ph_leaderboard, ta_leaderboard;

    // Separate scores by mode
    for (map<string, int>::iterator it = user_scores.begin(); it != user_scores.end(); ++it) {
        size_t pos = it->first.find_last_of('_');
        string username = it->first.substr(0, pos);
        string mode = it->first.substr(pos + 1);
        
        if (mode == "PH") {
            ph_leaderboard.push_back(make_pair(username, it->second));
        } else if (mode == "TA") {
            ta_leaderboard.push_back(make_pair(username, it->second));
        }
    }

    // Sort both leaderboards
    for (size_t i = 0; i < ph_leaderboard.size(); ++i) {
        for (size_t j = i + 1; j < ph_leaderboard.size(); ++j) {
            if (ph_leaderboard[i].second < ph_leaderboard[j].second) {
                pair<string, int> temp = ph_leaderboard[i];
                ph_leaderboard[i] = ph_leaderboard[j];
                ph_leaderboard[j] = temp;
            }
        }
    }
    
    for (size_t i = 0; i < ta_leaderboard.size(); ++i) {
        for (size_t j = i + 1; j < ta_leaderboard.size(); ++j) {
            if (ta_leaderboard[i].second < ta_leaderboard[j].second) {
                pair<string, int> temp = ta_leaderboard[i];
                ta_leaderboard[i] = ta_leaderboard[j];
                ta_leaderboard[j] = temp;
            }
        }
    }

    // Find current user's rank in both modes
    int ph_rank = -1, ta_rank = -1;
    for (size_t i = 0; i < ph_leaderboard.size(); ++i) {
        if (ph_leaderboard[i].first == current_user) {
            ph_rank = i + 1;
            break;
        }
    }
    for (size_t i = 0; i < ta_leaderboard.size(); ++i) {
        if (ta_leaderboard[i].first == current_user) {
            ta_rank = i + 1;
            break;
        }
    }

    #ifdef _WIN32
        system("cls");
    #else
        system("clear");
    #endif

    cout << "\n============= 📊 LEADERBOARD 📊 =============\n";
    
    // Show Package Hunt leaderboard
    cout << "\n🏆 PACKAGE HUNT MODE 🏆\n";
    cout << "  Rank | Name         | Score\n";
    cout << "-------|--------------|--------\n";
    for (size_t i = 0; i < ph_leaderboard.size() && i < 5; ++i) {
        cout << "   " << (i+1) << "   | " << setw(12) << left << ph_leaderboard[i].first
             << " | " << ph_leaderboard[i].second << " point" << endl;
    }
    
    if (ph_rank != -1) {
        cout << "-------------------------------------------------\n";
        cout << "Posisi kamu, " << current_user << ": peringkat ke-" << ph_rank << " dari " << ph_leaderboard.size() << " pemain\n";
        cout << "=================================================\n";
    }
    
    // Show Time Attack leaderboard
    cout << "\n⚡ TIME ATTACK MODE ⚡\n";
    cout << "  Rank | Name         | Score\n";
    cout << "-------|--------------|--------\n";
    for (size_t i = 0; i < ta_leaderboard.size() && i < 5; ++i) {
        cout << "   " << (i+1) << "   | " << setw(12) << left << ta_leaderboard[i].first
             << " | " << ta_leaderboard[i].second << " point" << endl;
    }

    if (ta_rank != -1) {
        cout << "-------------------------------------------------\n";
        cout << "Posisi kamu, " << current_user << ": peringkat ke-" << ta_rank << " dari " << ta_leaderboard.size() << " pemain\n";
        cout << "=================================================\n";
    }

    cout << "\n========================================\n";

    #ifdef _WIN32
        cout << "\n";
        system("pause");
    #else
        cout << "\nTekan ENTER untuk kembali...";
        cin.ignore();
        cin.get();
    #endif

    post_game_options();
}

void show_intro() {
    #ifdef _WIN32
        system("cls");
    #else
        system("clear");
    #endif

    cout << "================================================================================" << endl;
    cout << "                          CIHUY EXPRESS - DELIVERY GAME                         " << endl;
    cout << "================================================================================" << endl;
    cout << "Kamu adalah seorang kurir yang harus mengantar paket ke berbagai rumah di kota." << endl;
    cout << "Ambil paket dan antarkan ke rumah sebelum waktunya habis untuk mendapatkan poin!" << endl;
    cout << "Setiap kamu berhasil mengantar paket ke rumah, kamu akan dapat waktu tambahan!" << endl;
    cout << "Jangan lupa, hindari tembok yang menghalangi jalanmu!  " << endl;
    cout << endl;

    if (use_emojis) {
        cout << "Petunjuk:" << endl;
        cout << EMOJI_COURIER << " = Kurir (kamu)" << endl;
        cout << EMOJI_PACKAGE << " = Paket (ambil ini!)" << endl;
        cout << EMOJI_HOUSE << " = Rumah (antar paket ke sini!)" << endl;
        cout << EMOJI_WALL << " = Tembok (hindari ini!)" << endl;
        cout << EMOJI_CLOCK << " = Waktu / time (sisa waktu kamu!)" << endl;
    } else {
        cout << "Petunjuk:" << endl;
        cout << "C = Kurir (kamu)" << endl;
        cout << "P = Paket (ambil ini!)" << endl;
        cout << "H = Rumah (antar paket ke sini!)" << endl;
        cout << "# = Tembok (hindari ini!)" << endl;
        cout << "T = Waktu / time (sisa waktu kamu!)" << endl;
    }

    cout << endl;
    cout << "Level: " << endl;
    cout << "1. Mudah         ==>> 5 rumah, 6 paket, score perpaket 2 point" << endl;
    cout << "2. Biasa aja     ==>> 4 rumah, 5 paket, score perpaket 4 point" << endl;
    cout << "3. Sedang        ==>> 3 rumah, 4 paket, score perpaket 6 point" << endl;
    cout << "4. Lumayan Sulit ==>> 2 rumah, 3 paket, score perpaket 8 point" << endl;
    cout << "5. Sulit         ==>> 1 rumah, 2 paket, score perpaket 10 point" << endl;

    cout << endl;
    cout << "Kontrol: W (atas), A (kiri), S (bawah), D (kanan)" << endl;
    cout << "=======================================================" << endl << endl;
}

void ask_house_count() {
    string input;
    while (true) {
        cout << "Pilih level berapa yang ingin kamu tantang (q untuk quit)? (1-5): "; getline(cin, input);

        // trim spaces from username
        input.erase(0, input.find_first_not_of(" \t\n\r\f\v"));
        input.erase(input.find_last_not_of(" \t\n\r\f\v") + 1);

        if(input == "1") {
            house_count = 5;
            break;
        } else if(input == "2") {
            house_count = 4;
            break;
        } else if(input == "3") {
            house_count = 3;
            break;
        } else if(input == "4") {
            house_count = 2;
            break;
        } else if(input == "5") {
            house_count = 1;
            break;
        } else if(input == "q" || input == "Q") {
            cout << "\n 👋 Terima kasih telah bermain " << current_user << "! Sampai jumpa lagi! 👋\n\n";
            exit(0);
        } else {
            cout << "Level tidak tersedia! Silakan pilih level 1-5." << endl;
        }
    }
}

bool is_time_up() {
    time_t now = time(NULL);
    return difftime(now, start_time) >= TIME_LIMIT;
}

int get_remaining_time() {
    time_t now = time(NULL);
    int remaining = TIME_LIMIT - static_cast<int>(difftime(now, start_time));
    return remaining > 0 ? remaining : 0;
}

int main() {
    // setup terminal for emojis if supported
    setup_terminal();
    use_emojis = check_utf8_support();

    login_or_regis();
    srand(time(0));

        lives = MAX_LIVES; // : Inisialisasi nyawa


    show_intro();
    ask_house_count();

    generateMap();

    cout << EMOJI_CLOCK << " Waktu kamu adalah: " << TIME_LIMIT << " detik " << EMOJI_CLOCK << "\n" << endl;

    #ifdef _WIN32
        system("pause");
    #else
        cout << "Tekan ENTER untuk mulai...";
        cin.ignore();
        cin.get();
    #endif

    start_time = time(NULL);

    while (true) {
        printMap();
        string input;
        cout << "\nMasukkan gerakan (W/A/S/D) atau Q untuk keluar: "; getline(cin, input);

        if (input.empty()) {
            cout << "❌ Input tidak boleh kosong. Silakan masukkan arah (W, A, S, D):\n"; getline(cin, input);
        }

        // convert to lowercase untuk konsistensi
        char move = tolower(input[0]);

        if (is_time_up()) {
            cout << "\n" << EMOJI_CLOCK << "Waktu habis! Kamu gagal mengantar semua paket!" << endl;
            cout << "Skor akhir kamu: " << score << " point" << endl;
            cout << "Total paket berhasil diantar: " << delivered_packages << endl;

            cout << "High score sebelumnya: " << old_highscore << " point" << endl;

            if (score > old_highscore) {
                cout << "\n🎉 Selamat! Skor baru kamu (" << score << ") adalah rekor baru! 🎉\n";
                cout << "Total paket berhasil diantar: " << delivered_packages << endl;
                save_user(current_user, score);
                old_highscore = score;
            } else {
                cout << "\nSkor kamu belum mengalahkan rekor sebelumnya 😢\n";
                cout << "Skor tertinggi kamu tetap: " << old_highscore << " point\n";
            }

            #ifdef _WIN32
                cout << "\n";
                system("pause");
            #else
                cout << "\nTekan ENTER untuk mulai...";
                cin.ignore();
                cin.get();
            #endif

            post_game_options();
        }

        if (move == 'q') {
            cout << "Game berakhir! Skor akhir: " << score << " point" << endl;

            cout << "High score sebelumnya: " << old_highscore << " point" << endl;

            cout << "Total paket berhasil diantar: " << delivered_packages << endl;

            if (score > old_highscore) {
                cout << "\n🎉 Selamat! Skor baru kamu (" << score << ") adalah rekor baru! 🎉\n";
                save_user(current_user, score);
                old_highscore = score;
            } else {
                cout << "\nSkor kamu belum mengalahkan rekor sebelumnya 😢\n";
                cout << "Skor tertinggi kamu tetap: " << old_highscore << " point\n";
            }

            #ifdef _WIN32
                cout << "\n";
                system("pause");
            #else
                cout << "\nTekan ENTER untuk mulai...";
                cin.ignore();
                cin.get();
            #endif

            post_game_options();
        }

        if (move == 'w' || move == 'a' || move == 's' || move == 'd') {
            moveCourier(move); // move the courier
            pickUpPackage(); // Take the package if there is one
            deliverPackage(); // deliver the package once it has reached its destination
        } else {
            cout << "Input tidak valid! Gunakan W/A/S/D untuk bergerak atau Q untuk keluar." << endl;

            #ifdef _WIN32
                Sleep(700);
            #else
                usleep(7000000);
            #endif

            continue; // return to the beginning of the loop without sleep at the end
        }

        #ifdef _WIN32
            Sleep(200);
        #else
            usleep(200000); // game speed (200ms)
        #endif
    }
}
