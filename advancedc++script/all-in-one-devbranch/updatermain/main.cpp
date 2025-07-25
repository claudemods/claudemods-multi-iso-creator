#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <array>
#include <memory>
#include <string>

#define COLOR_GREEN "\033[38;2;0;255;0m"
#define COLOR_CYAN "\033[38;2;0;255;255m"
#define COLOR_RED "\033[38;2;255;0;0m"
#define COLOR_YELLOW "\033[38;2;255;255;0m"
#define COLOR_RESET "\033[0m"

char detected_distro[64] = "";
const char* executable_name = "cmi.bin";
bool commands_completed = false;
bool loading_complete = false;
char current_version[64] = "unknown";
char downloaded_version[64] = "unknown";
char installed_version[64] = "unknown";

void silent_command(const char* cmd) {
    char full_cmd[512];
    snprintf(full_cmd, sizeof(full_cmd), "%s >/dev/null 2>&1", cmd);
    system(full_cmd);
}

std::string run_command(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }
    return result;
}

void* execute_update_thread(void* /*arg*/) {
    while (!loading_complete) usleep(10000);

    // 1. GIT CLONE
    silent_command("cd /home/$USER/ && git clone https://github.com/claudemods/claudemods-multi-iso-konsole-script.git   ");

    // 2. CURRENT VERSION
    try {
        std::string version_output = run_command("cat /home/$USER/.config/cmi/version.txt");
        strncpy(current_version, version_output.empty() ? "not installed" : version_output.c_str(), 
               sizeof(current_version) - 1);
    } catch (...) {
        strcpy(current_version, "not installed");
    }

    // 3. DETECT DISTRO (EXPLICIT CACHYOS AND KDE NEON SUPPORT)
    try {
        std::string distro_output = run_command("cat /etc/os-release | grep '^ID=' | cut -d'=' -f2 | tr -d '\"'");
        if (distro_output == "arch" || distro_output == "cachyos") {
            strcpy(detected_distro, distro_output.c_str()); // Store actual distro name
        } else if (distro_output == "ubuntu") {
            strcpy(detected_distro, "ubuntu");
        } else if (distro_output == "debian") {
            strcpy(detected_distro, "debian");
        } else if (distro_output == "trixie") {
            strcpy(detected_distro, "trixie");
        } else if (distro_output == "neon") { // KDE Neon detection
            strcpy(detected_distro, "neon");
        }
    } catch (...) {
        strcpy(detected_distro, "unknown");
    }

    // 4. DOWNLOADED VERSION
    if (strcmp(detected_distro, "arch") == 0 || strcmp(detected_distro, "cachyos") == 0) {
        try {
            std::string version_output = run_command(
                "cat /home/$USER/claudemods-multi-iso-konsole-script/advancedc++script/all-in-one-devbranch/version/arch/version.txt");
            strncpy(downloaded_version, version_output.c_str(), sizeof(downloaded_version) - 1);
        } catch (...) {
            strcpy(downloaded_version, "unknown");
        }
    } else if (strcmp(detected_distro, "ubuntu") == 0 || strcmp(detected_distro, "neon") == 0) {
        try {
            std::string version_output = run_command(
                "cat /home/$USER/claudemods-multi-iso-konsole-script/advancedc++script/all-in-one-devbranch/version/ubuntu/version.txt");
            strncpy(downloaded_version, version_output.c_str(), sizeof(downloaded_version) - 1);
        } catch (...) {
            strcpy(downloaded_version, "unknown");
        }
    } else if (strcmp(detected_distro, "debian") == 0) {
        try {
            std::string version_output = run_command(
                "cat /home/$USER/claudemods-multi-iso-konsole-script/advancedc++script/all-in-one-devbranch/version/debian/version.txt");
            strncpy(downloaded_version, version_output.c_str(), sizeof(downloaded_version) - 1);
        } catch (...) {
            strcpy(downloaded_version, "unknown");
        }
    }

    // INSTALLATION PROCESS
    silent_command("rm -rf /home/$USER/.config/cmi");
    silent_command("mkdir -p /home/$USER/.config/cmi");

    // EXPLICIT HANDLING FOR CACHYOS (USES ARCH FILES)
    if (strcmp(detected_distro, "arch") == 0 || strcmp(detected_distro, "cachyos") == 0) {
        silent_command("cp /home/$USER/claudemods-multi-iso-konsole-script/advancedc++script/all-in-one-devbranch/version/arch/version.txt /home/$USER/.config/cmi/");
        silent_command("unzip -o /home/$USER/claudemods-multi-iso-konsole-script/advancedcscript/buildimages/build-image-arch.zip -d /home/$USER/.config/cmi/");
    } else if (strcmp(detected_distro, "ubuntu") == 0 || strcmp(detected_distro, "neon") == 0) {
        silent_command("cp /home/$USER/claudemods-multi-iso-konsole-script/advancedc++script/all-in-one-devbranch/version/ubuntu/version.txt /home/$USER/.config/cmi/");
        silent_command("unzip -o /home/$USER/claudemods-multi-iso-konsole-script/advancedcscript/buildimages/build-image-noble.zip -d /home/$USER/.config/cmi/");
    } else if (strcmp(detected_distro, "debian") == 0) || strcmp(detected_distro, "trixie") == 0) {
        silent_command("cp /home/$USER/claudemods-multi-iso-konsole-script/advancedc++script/all-in-one-devbranch/version/debian/version.txt /home/$USER/.config/cmi/");
        silent_command("unzip -o /home/$USER/claudemods-multi-iso-konsole-script/advancedcscript/buildimages/build-image-debian.zip -d /home/$USER/.config/cmi/");
    }

    // FINAL STEPS WITH DISTRO DETECTION
    if (strcmp(detected_distro, "arch") == 0 || strcmp(detected_distro, "cachyos") == 0) {
        // Arch/CachyOS-specific commands
        silent_command("cd /home/$USER/claudemods-multi-iso-konsole-script/advancedc++script/all-in-one-devbranch && qmake && make >/dev/null 2>&1");
        silent_command("sudo cp /home/$USER/claudemods-multi-iso-konsole-script/advancedc++script/all-in-one-devbranch/cmi.bin /usr/bin/cmi.bin");
        silent_command("cp -r /home/$USER/claudemods-multi-iso-konsole-script/advancedc++script/all-in-one-devbranch/calamares-per-distro /home/$USER/.config/cmi");
        silent_command("cp -r /home/$USER/claudemods-multi-iso-konsole-script/guide/readme.txt /home/$USER/.config/cmi");
        silent_command("cp -r /home/$USER/claudemods-multi-iso-konsole-script/changesc++.txt /home/$USER/.config/cmi");
        silent_command("cp /home/$USER/claudemods-multi-iso-konsole-script/advancedc++script/all-in-one-devbranch/installermain/patch.sh /home/$USER/.config/cmi >/dev/null 2>&1");
    } else if (strcmp(detected_distro, "ubuntu") == 0 || strcmp(detected_distro, "neon") == 0) {
        // Ubuntu/KDE Neon-specific commands
        silent_command("cd /home/$USER/claudemods-multi-iso-konsole-script/advancedc++script/all-in-one-devbranch && qmake6 && make >/dev/null 2>&1");
        silent_command("sudo cp /home/$USER/claudemods-multi-iso-konsole-script/advancedc++script/all-in-one-devbranch/cmi.bin /usr/bin/cmi.bin");
        silent_command("cp -r /home/$USER/claudemods-multi-iso-konsole-script/advancedc++script/all-in-one-devbranch/calamares-per-distro /home/$USER/.config/cmi");
        silent_command("cp -r /home/$USER/claudemods-multi-iso-konsole-script/guide/readme.txt /home/$USER/.config/cmi");
        silent_command("cp -r /home/$USER/claudemods-multi-iso-konsole-script/changesc++.txt /home/$USER/.config/cmi");
        silent_command("cp /home/$USER/claudemods-multi-iso-konsole-script/advancedc++script/all-in-one-devbranch/installermain/patch.sh /home/$USER/.config/cmi >/dev/null 2>&1");
    } else if (strcmp(detected_distro, "debian") == 0) {
        // Debian-specific commands
        silent_command("cd /home/$USER/claudemods-multi-iso-konsole-script/advancedc++script/all-in-one-devbranch && qmake6 && make >/dev/null 2>&1");
        silent_command("sudo cp /home/$USER/claudemods-multi-iso-konsole-script/advancedc++script/all-in-one-devbranch/cmi.bin /usr/bin/cmi.bin");
        silent_command("cp -r /home/$USER/claudemods-multi-iso-konsole-script/advancedc++script/all-in-one-devbranch/calamares-per-distro /home/$USER/.config/cmi");
        silent_command("cp -r /home/$USER/claudemods-multi-iso-konsole-script/guide/readme.txt /home/$USER/.config/cmi");
        silent_command("cp -r /home/$USER/claudemods-multi-iso-konsole-script/changesc++.txt /home/$USER/.config/cmi");
        silent_command("cp /home/$USER/claudemods-multi-iso-konsole-script/advancedc++script/all-in-one-devbranch/installermain/patch.sh /home/$USER/.config/cmi >/dev/null 2>&1");
    } else {
        std::cout << COLOR_RED << "Unsupported distribution: " << detected_distro << COLOR_RESET << std::endl;
        commands_completed = true;
        return nullptr;
    }

    // Cleanup
    silent_command("rm -rf /home/$USER/claudemods-multi-iso-konsole-script");

    // GET INSTALLED VERSION
    try {
        std::string installed_version_output = run_command("cat /home/$USER/.config/cmi/version.txt");
        strncpy(installed_version, installed_version_output.c_str(), sizeof(installed_version) - 1);
    } catch (...) {
        strcpy(installed_version, "unknown");
    }

    commands_completed = true;
    return nullptr;
}

void show_loading_bar() {
    std::cout << COLOR_GREEN << "Progress: [" << COLOR_RESET;
    for (int i = 0; i < 50; i++) {
        std::cout << COLOR_YELLOW << "=" << COLOR_RESET;
        std::cout.flush();
        usleep(50000);
    }
    std::cout << COLOR_GREEN << "] 100%\n" << COLOR_RESET;
    loading_complete = true;
}

int main() {
    pthread_t thread;
    pthread_create(&thread, nullptr, execute_update_thread, nullptr);

    show_loading_bar();

    while (!commands_completed) usleep(10000);
    pthread_join(thread, nullptr);

    std::cout << COLOR_GREEN << "\nInstallation complete!\n" << COLOR_RESET;
    std::cout << COLOR_GREEN << "Executable installed to: /usr/bin/cmi.bin\n" << COLOR_RESET;
    std::cout << COLOR_GREEN << "Configuration files placed in: /home/$USER/.config/cmi/\n" << COLOR_RESET;
    std::cout << COLOR_GREEN << "Detected distro: " << detected_distro << COLOR_RESET << std::endl;
    std::cout << COLOR_GREEN << "Current version: " << current_version << COLOR_RESET << std::endl;
    std::cout << COLOR_GREEN << "Downloaded version: " << downloaded_version << COLOR_RESET << std::endl;
    std::cout << COLOR_GREEN << "Installed version: " << installed_version << COLOR_RESET << std::endl;

    std::cout << COLOR_CYAN << "\nLaunch now? (y/n): " << COLOR_RESET;
    char response;
    std::cin >> response;

    if (response == 'y' || response == 'Y') {
        system(executable_name);
    }

    return EXIT_SUCCESS;
}
