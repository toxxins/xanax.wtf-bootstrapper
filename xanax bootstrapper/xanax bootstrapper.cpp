#include <iostream>
#include <string>
#include <fstream>
#include <windows.h>
#include <wininet.h>
#include <shellapi.h>
#include <direct.h>
#include <iomanip>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "shell32.lib")

// --- config ---
const std::string VERSION_URL = "https://pastebin.com/raw/0kW3C34p";
const std::string DOWNLOAD_URL = "https://github.com/toxxins/xanax.wtf/releases/download/testing/v1.0.0.zip";
// ---------------------

// --- colors ---
enum Color {
    BLUE = 1, GREEN = 10, CYAN = 11, RED = 12, PINK = 13, YELLOW = 14, WHITE = 15
};

void SetColor(int c) {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), c);
}

// log helper xd
void Log(std::string msg, int color = WHITE, bool newLine = true) {
    SetColor(color);
    std::cout << msg;
    if (newLine) std::cout << std::endl;
    SetColor(WHITE);
}

const char* ASCII_ART = R"(
 /$$$$                                                   /$$$$
| $$_/                                                  |_  $$
| $$   /$$   /$$  /$$$$$$  /$$$$$$$   /$$$$$$  /$$   /$$  | $$
| $$  |  $$ /$$/ |____  $$| $$__  $$ |____  $$|  $$ /$$/  | $$
| $$   \  $$$$/   /$$$$$$$| $$  \ $$  /$$$$$$$ \  $$$$/   | $$
| $$    >$$  $$  /$$__  $$| $$  | $$ /$$__  $$  >$$  $$   | $$
| $$$$ /$$/\  $$|  $$$$$$$| $$  | $$|  $$$$$$$ /$$/\  $$ /$$$$
|____/|__/  \__/ \_______/|__/  |__/ \_______/|__/  \__/|____/
)";

std::string Sanitize(std::string input) {
    std::string output = "";
    for (char c : input) {
        if (isalnum(c) || c == '.') output += c;
    }
    return output;
}

// fetch version
std::string FetchRemoteText(std::string url) {
    HINTERNET hInternet = InternetOpenA("xanaxUpdater", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) return "";

    HINTERNET hConnect = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);
    if (!hConnect) {
        InternetCloseHandle(hInternet);
        return "";
    }

    char buffer[128];
    DWORD bytesRead;
    std::string result = "";
    while (InternetReadFile(hConnect, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        result += buffer;
    }

    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
    return result;
}

// download w progress bar
bool DownloadFileWithProgress(std::string url, std::string destPath) {
    HINTERNET hInternet = InternetOpenA("xanaxDownloader", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) return false;

    HINTERNET hUrl = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, 0, 0);
    if (!hUrl) {
        InternetCloseHandle(hInternet);
        return false;
    }

    // gets the file size for progress calculation
    char fileSizeBuffer[32];
    DWORD bufferLen = sizeof(fileSizeBuffer);
    DWORD index = 0;
    long totalSize = 0;

    if (HttpQueryInfoA(hUrl, HTTP_QUERY_CONTENT_LENGTH, fileSizeBuffer, &bufferLen, &index)) {
        totalSize = atol(fileSizeBuffer);
    }

    std::ofstream outFile(destPath, std::ios::binary);
    if (!outFile) return false;

    char buffer[4096];
    DWORD bytesRead;
    long totalRead = 0;

    // hide cursor
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &cursorInfo);
    cursorInfo.bVisible = false;
    SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &cursorInfo);

    while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        outFile.write(buffer, bytesRead);
        totalRead += bytesRead;

        if (totalSize > 0) {
            int percent = (int)((totalRead * 100) / totalSize);
            int barWidth = 50;

            std::cout << "\r";
            SetColor(CYAN);
            std::cout << "[";
            int pos = barWidth * percent / 100;
            for (int i = 0; i < barWidth; ++i) {
                if (i < pos) std::cout << "=";
                else if (i == pos) std::cout << ">";
                else std::cout << " ";
            }
            std::cout << "] " << percent << "%";
            SetColor(WHITE);
        }
    }

    std::cout << std::endl;
    outFile.close();
    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);

    // show cursor
    cursorInfo.bVisible = true;
    SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &cursorInfo);

    return true;
}

bool PathExists(const std::string& name) {
    DWORD attrib = GetFileAttributesA(name.c_str());
    return (attrib != INVALID_FILE_ATTRIBUTES);
}

int main() {
    SetConsoleTitleA("xanax.wtf Updater");

    Log(ASCII_ART, CYAN);
    Log("---------------------------------------------------------", CYAN);

    char* appDataBuffer = nullptr;
    size_t len = 0;
    if (_dupenv_s(&appDataBuffer, &len, "LOCALAPPDATA") != 0 || appDataBuffer == nullptr) return 1;
    std::string appData = appDataBuffer;
    free(appDataBuffer);

    // paths
    std::string dirPath = appData + "\\xanax";
    std::string versionFile = dirPath + "\\UPDATE.txt";
    std::string exePath = dirPath + "\\xanax.exe";
    std::string zipPath = dirPath + "\\update.zip";

    // check / create directories
    if (!PathExists(dirPath)) {
        Log("[+] Directory not found. Creating...", YELLOW);
        _mkdir(dirPath.c_str());
    }

    if (!PathExists(versionFile)) {
        Log("[+] UPDATE.txt not found.", YELLOW);
        std::ofstream makeFile(versionFile.c_str());
        makeFile << "v0.0.0";
        makeFile.close();
    }

    Log("[*] Checking for updates...", WHITE);

    // version check
    std::string rawRemote = FetchRemoteText(VERSION_URL);
    std::string remoteClean = Sanitize(rawRemote);

    std::string localClean = "";
    std::ifstream ifs(versionFile.c_str());
    std::string line;
    if (std::getline(ifs, line)) localClean = Sanitize(line);
    ifs.close();

    if (remoteClean.empty()) {
        Log("[-] Error: Could not connect to update server.", RED);
        system("pause");
        return 0;
    }

    Log("    Local:  " + localClean, WHITE);
    Log("    Remote: " + remoteClean, WHITE);

    if (localClean == remoteClean) {
        Log("[+] Up to date!", GREEN);
        Log("[*] Launching xanax...", CYAN);
        Sleep(3000);
        ShellExecuteA(NULL, "open", exePath.c_str(), NULL, NULL, SW_SHOW);
    }
    else {
        Log("[!] Update required.", YELLOW);
        Log("[*] Downloading update...", CYAN);

        if (DownloadFileWithProgress(DOWNLOAD_URL, zipPath)) {
            Log("[*] Extracting...", YELLOW);

            // unzip the update
            std::string cmd = "powershell -Command \"Expand-Archive -Path '" + zipPath + "' -DestinationPath '" + dirPath + "' -Force\" > nul";
            system(cmd.c_str());

            // update UPDATE.txt aka local version
            std::ofstream ofs(versionFile.c_str());
            ofs << rawRemote;
            ofs.close();

            remove(zipPath.c_str());

            Log("[+] Update Complete!", GREEN);
            Log("[*] Starting Xanax", CYAN);
            Sleep(1500);
            ShellExecuteA(NULL, "open", exePath.c_str(), NULL, NULL, SW_SHOW);
        }
        else {
            Log("[-] Download failed. Check internet connection.", RED);
            system("pause");
        }
    }

    return 0;
}