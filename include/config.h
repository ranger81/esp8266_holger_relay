#include <FS.h>
#include <LittleFS.h>

struct Config {
    char host[100];
    char port[6];
    char path[255];
    char interval[7];
    char cacert[2000];
} config;

// Saves an object to a file in LittleFS
template <typename T>
bool saveObjectToFS(T &obj, const char *filename) {
    if (LittleFS.begin()) {
        File file = LittleFS.open(filename, "w");
        if (!file) {
            return false;
        }
        size_t size = sizeof(obj);
        size_t bytesWritten = file.write((uint8_t *)&obj, size);
        file.close();
        LittleFS.end();
        return bytesWritten == size;
    }
    return false;
}

// Loads an object from a file in LittleFS
template <typename T>
bool loadObjectFromFS(T &obj, const char *filename) {
    if (LittleFS.begin()) {
        if (!LittleFS.exists(filename)) {
            return false;
        }
        File file = LittleFS.open(filename, "r");
        if (!file) {
            return false;
        }
        size_t size = sizeof(obj);
        size_t bytesRead = file.readBytes((char *)&obj, size);
        file.close();
        LittleFS.end();
        return bytesRead == size;
    }
    return false;
}