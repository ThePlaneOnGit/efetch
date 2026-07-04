#include <error.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include <unistd.h>
#include "config.h"


// variables
char* os;
char* kernelVersion;
char* shell;

//method for executing a command and get the output
char* exec(const char* cmd) {
    char buffer[16] = {0};
    size_t resultLen = 0;
    size_t resultSize = 16;
    char* result = malloc(resultSize);
    memset(result, 0, 16);

    FILE* pipe = popen(cmd, "r");

    while(!feof(pipe)) {
        if(fgets(buffer, 16, pipe) != NULL) {
            resultLen += strlen(buffer);
            if(resultLen > resultSize) {
                resultSize *= 2;
                result = realloc(result, resultSize);
            }
            strcat(result, buffer);
        }
    }

    return result;
}

unsigned int countOccur(FILE* haystack, const char* needle){
    char buffer[1024];
    unsigned int occurences = 0;
    while(!feof(haystack)) {
        if(fgets(buffer, 1024, haystack) != NULL) {
            unsigned int done = 0;
            char* new;
            while((new = strstr(buffer + done, needle))) {
                done = (new - buffer) + 1;
                occurences++;
            }
        }
    }
    return occurences;
}

char* getCpu(FILE* cpuid){
    //error(1, 0, "displayCpu is not impemented");
    char* line = NULL;
    size_t num = 0;
    for(;;) {
        getline(&line, &num, cpuid);
        if(line == NULL) perror("getline");
        if(*line == 'm' && strstr(line, "model name")) break;
        free(line);
    }

    char* name = strchr(line, ':');
    name++;

    name[strlen(name) - 1] = 0;

    char* name_dup = strdup(name);

    free(line);

    rewind(cpuid);

    unsigned int cpus = countOccur(cpuid, "model name");

    // '(' cpus ')' '\0'
    char cpus_str[5];
    sprintf(cpus_str, "(%u)", cpus);

    // model_name ' ' '(' cpus ')' \0
    char* ret = malloc(strlen(name_dup) + 1 + strlen(cpus_str) + 1 + 1);
    strcpy(ret, name_dup);
    strcat(ret, " ");
    strcat(ret, cpus_str);
    return ret;
}

void getDistro(FILE* osrel){
    char* line = NULL ;
    size_t num = 0    ;
    for (;;) {
        getline(&line, &num, osrel);
        if(line == NULL) perror("getline");
        if(*line == 'I' && strstr(line, "ID=")) break;
        line = NULL;
        free(line);
    }

    char* after = strchr(line, '=');

    after[strlen(after) - 1] = 0;

    if(*(++after) == '"') {
        after++;
        after[strlen(after) - 1] = 0;
    }

    os = strdup(after);
    free(line);
}

void checkOS(void) {
    struct utsname kernInfo = {};

    // uname shouldnt fail, but handle the fail anyways
    if(uname(&kernInfo) < 0) {
        perror("Uname");
    }

    kernelVersion = strdup(kernInfo.release);

    // Handle All Non-linux Unix Releases
    if(strcmp("Linux", kernInfo.sysname) != 0) {
        os = strdup(kernInfo.sysname);
        return;
    }

    FILE* osrel = fopen("/etc/os-release", "r");

    if(osrel == NULL) {
        perror("fopen(\"/etc/os-release\", \"r\")");
    }

    getDistro(osrel);
}

// gets all packages for all supported operating systems
char* getPackages(void) {
    size_t packLen = 0;
    size_t packSize = 32;
    char* packages = malloc(packSize);

#define GET(bin, cmd, name)\
    if(access("/usr/" bin, 0) == 0) {\
        char* tmp = exec("echo -n $(" cmd ")");\
        packLen += strlen(tmp) + strlen(" (" name ") ");\
        if(packLen > packSize) {\
            while(packLen > packSize) packSize *= 2;\
            packages =  realloc(packages, packSize);\
        }\
        strcat(packages, tmp);\
        strcat(packages, " (" name ") ");\
    }

    GET("bin/emerge"       , "ls -d /var/db/pkg/*/* | wc -l"  , "emerge" );
    GET("bin/pacman"       , "pacman -Qq | wc -l"             , "pacman" );
    GET("bin/xbps-install" , "xbps-query -l | wc -l"          , "xbps"   );
    GET("sbin/pkg_info"    , "pkg_info | wc -l"               , "pkg"    );
    GET("bin/dpkg-query"   , "dpkg-query -f '.\n' -W | wc -l" , "dpkg"   );

    // fallback for if no package manager could be found, lists all programs
    // installed under /usr/bin
    if(*packages == 0) {
        GET("bin", "ls /usr/bin | wc -l", "/bin");
    }

    return packages;
}

// method to handle the printing of the information
void print(const char* str1, const char* str2) {
    if(str1 && str1[0]) {
        //std::cout << primaryColor << " " << str1 << " ~ ";
        printf("\x1b[%dm %s ~ ", primaryColor, str1);
    }

    //std::cout << secondaryColor << str2 << resetColor << std::endl;
    printf("\x1b[%dm%s\x1b[%dm\n", secondaryColor, str2, resetColor);
}

char* getUser(void){
    struct passwd* pass = getpwuid(getuid());

    if(pass == NULL) {
        // getuid never fails
        perror("getpwuid");
    }

    if(!shell) {
        shell = pass->pw_shell;
    }
    // NOTE: statically allocated, DO NOT `free(3)`
    return pass->pw_name;
}

char* getHost(void){
    char* hostname = malloc(1024);
    memset(hostname, 0, 1024);
    // 1023 to account for the lack of a terminating NULL byte
    if(gethostname(hostname, 1023) < 0) {
        perror("gethostname");
    }

    return hostname;
}

char* getShell(void){
    if(shell)
        return shell;

    struct passwd* pass = getpwuid(getuid());
    if(pass == NULL) {
        // getuid never fails
        perror("getpwuid");
    }

    shell = pass->pw_shell;
    return shell;
}

int main() {
    checkOS();
    if(os == NULL) return 1;
    if(displayHostname) {
        char* user = getUser();
        char* host = getHost();

        // user @ hostname \0
        char* hostData = malloc(strlen(user) + 1 + strlen(host) + 1);
        strcpy(hostData, user);
        strcat(hostData, "@");
        strcat(hostData, host);

        print("", hostData);
        free(hostData);
        free(host);
    }

    if(displayOperatingSystem) {
        print("os", os);
    }

    if(displayShell) {
        print("sh", getShell());
    }

    if(displayPackages) {
        print("pkgs", getPackages());
    }

    if(displayKernelVersion) {
        print("kernel", kernelVersion);
    }

    if(displayCpu) {
        FILE* cpuin = fopen("/proc/cpuinfo", "r");
        print("cpu", getCpu(cpuin));
        fclose(cpuin);
    }

    return 0;
}
