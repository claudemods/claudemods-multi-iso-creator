#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <ctime>
#include <libgen.h>
#include <dirent.h>
#include <cerrno>
#include <climits>
#include <fcntl.h>
#include <termios.h>
#include <vector>
#include <string>
#include <fstream>
#include <memory>
#include <array>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <pwd.h>
#include <sstream>
#include <filesystem>

#define MAX_PATH 4096
#define MAX_CMD 16384
#define BLUE "\033[34m"
#define GREEN "\033[32m"
#define RED "\033[31m"
#define RESET "\033[0m"
#define COLOR_CYAN "\033[38;2;0;255;255m"
#define COLOR_GOLD "\033[38;2;36;255;255m"
#define PASSWORD_MAX 100
#define COLOR_RESET "\033[0m"

using namespace std;

struct termios original_term;
enum Distro { ARCH, UBUNTU, DEBIAN, CACHYOS, NEON, UNKNOWN };
atomic<bool> time_thread_running(true);
mutex time_mutex;
string current_time_str;
bool should_reset = false;

string get_kernel_version();
string read_clone_dir();
bool dir_exists(const string &path);
void execute_command(const string& cmd);
Distro detect_distro();
string get_distro_name(Distro distro);
void edit_calamares_branding();
bool is_init_generated(Distro distro);
string get_clone_dir_status();
string get_init_status(Distro distro);
string get_iso_name();
string get_iso_name_status();
void message_box(const string &title, const string &message);
void error_box(const string &title, const string &message);
void progress_dialog(const string &message);
void enable_raw_mode();
void disable_raw_mode();
string get_highlight_color(Distro distro);
string get_input(const string &prompt_text, bool echo = true);
string prompt(const string &prompt_text);
string password_prompt(const string &prompt_text);
void update_time_thread();
void print_banner(Distro distro);
int get_key();
void print_blue(const string &text);
void install_dependencies_arch();
void generate_initrd_arch();
void edit_grub_cfg_arch();
void edit_isolinux_cfg_arch();
void install_calamares_arch();
void install_dependencies_cachyos();
void install_calamares_cachyos();
void install_dependencies_ubuntu();
void copy_vmlinuz_ubuntu();
void generate_initrd_ubuntu();
void edit_grub_cfg_ubuntu();
void edit_isolinux_cfg_ubuntu();
void install_calamares_ubuntu();
void install_dependencies_debian();
void copy_vmlinuz_debian();
void generate_initrd_debian();
void edit_grub_cfg_debian();
void edit_isolinux_cfg_debian();
void install_calamares_debian();
void clone_system(const string &clone_dir);
void create_squashfs_image(Distro distro);
void delete_clone_system_temp(Distro distro);
void set_clone_directory();
void install_one_time_updater();
void squashfs_menu(Distro distro);
void create_iso(Distro distro);
void run_iso_in_qemu();
void iso_creator_menu(Distro distro);
void create_command_files();
void remove_command_files();
void command_installer_menu(Distro distro);
void setup_script_menu(Distro distro);
void save_iso_name(const string &name);
void set_iso_name();
string dialog_input(const string &title, const string &prompt, int height = 0, int width = 0);
string dialog_password(const string &title, const string &prompt, int height = 0, int width = 0);
int dialog_menu(const string &title, const vector<string> &items, int height = 0, int width = 0);
void dialog_msgbox(const string &title, const string &message, int height = 0, int width = 0);

string dialog_input(const string &title, const string &prompt, int height, int width) {
    string command = "dialog --colors --title \"" + title + "\" --inputbox \"" + prompt + "\" " +
    to_string(height) + " " + to_string(width) + " 2>&1 >/dev/tty";

    array<char, 128> buffer;
    string result;
    unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) {
        throw runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    // Remove trailing newline if present
    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }

    return result;
}

string dialog_password(const string &title, const string &prompt, int height, int width) {
    string command = "dialog --colors --insecure --title \"" + title + "\" --passwordbox \"" + prompt + "\" " +
    to_string(height) + " " + to_string(width) + " 2>&1 >/dev/tty";

    array<char, 128> buffer;
    string result;
    unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) {
        throw runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    // Remove trailing newline if present
    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }

    return result;
}

int dialog_menu(const string &title, const vector<string> &items, int height, int width) {
    string command = "dialog --colors --title \"" + title + "\" --menu \"\" " +
    to_string(height) + " " + to_string(width) + " " + to_string(items.size()) + " ";

    for (size_t i = 0; i < items.size(); i++) {
        command += "\"" + to_string(i) + "\" \"" + items[i] + "\" ";
    }

    command += "2>&1 >/dev/tty";

    array<char, 128> buffer;
    string result;
    unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) {
        throw runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    if (result.empty()) {
        return -1; // User cancelled
    }

    // Remove trailing newline if present
    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }

    return stoi(result);
}

void dialog_msgbox(const string &title, const string &message, int height, int width) {
    string command = "dialog --colors --title \"" + title + "\" --msgbox \"" + message + "\" " +
    to_string(height) + " " + to_string(width) + " 2>&1 >/dev/tty";
    system(command.c_str());
}

string get_input(const string &prompt_text, bool echo) {
    return dialog_input("Input", prompt_text, 0, 0);
}

bool is_init_generated(Distro distro) {
    string init_path;
    if (distro == UBUNTU || distro == DEBIAN) {
        init_path = "/home/" + string(getenv("USER")) + "/.config/cmi/build-image-" +
        (distro == UBUNTU ? "noble" : "debian") + "/live/initrd.img-" + get_kernel_version();
    } else {
        init_path = "/home/" + string(getenv("USER")) + "/.config/cmi/build-image-arch/live/initramfs-linux.img";
    }
    struct stat st;
    return stat(init_path.c_str(), &st) == 0;
}

string get_clone_dir_status() {
    string clone_dir = read_clone_dir();
    if (clone_dir.empty()) {
        return RED + string("✗ Clone directory not set (Use Option 4 in Setup Script Menu)") + RESET;
    } else {
        return GREEN + string("✓ Clone directory: ") + clone_dir + RESET;
    }
}

string get_init_status(Distro distro) {
    if (is_init_generated(distro)) {
        return GREEN + string("✓ Initramfs generated") + RESET;
    } else {
        return RED + string("✗ Initramfs not generated (Use Option 1 in Setup Script Menu)") + RESET;
    }
}

string get_iso_name() {
    string file_path = "/home/" + string(getenv("USER")) + "/.config/cmi/isoname.txt";
    ifstream f(file_path);
    if (!f) return "";

    string name;
    getline(f, name);
    return name;
}

string get_iso_name_status() {
    string iso_name = get_iso_name();
    if (iso_name.empty()) {
        return RED + string("✗ ISO name not set (Use Option 3 in Setup Script Menu)") + RESET;
    } else {
        return GREEN + string("✓ ISO name: ") + iso_name + RESET;
    }
}

bool dir_exists(const string &path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

void message_box(const string &title, const string &message) {
    dialog_msgbox(title, message, 0, 0);
}

void error_box(const string &title, const string &message) {
    dialog_msgbox(title, message, 0, 0);
}

void progress_dialog(const string &message) {
    dialog_msgbox("Progress", message, 0, 0);
}

void enable_raw_mode() {
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
}

void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSANOW, &original_term);
}

void execute_command(const string& cmd) {
    cout << COLOR_CYAN;
    fflush(stdout);
    string full_cmd = " " + cmd;
    int status = system(full_cmd.c_str());
    cout << COLOR_RESET;
    if (status != 0) {
        cerr << RED << "Error executing: " << full_cmd << RESET << endl;
        exit(1);
    }
}

Distro detect_distro() {
    ifstream os_release("/etc/os-release");
    if (!os_release.is_open()) return UNKNOWN;

    string line;
    while (getline(os_release, line)) {
        if (line.find("ID=") == 0) {
            if (line.find("arch") != string::npos) return ARCH;
            if (line.find("ubuntu") != string::npos) return UBUNTU;
            if (line.find("debian") != string::npos) return DEBIAN;
            if (line.find("cachyos") != string::npos) return CACHYOS;
            if (line.find("neon") != string::npos) return NEON;
        }
    }
    return UNKNOWN;
}

string get_highlight_color(Distro distro) {
    switch(distro) {
        case ARCH: return "\033[34m";
        case UBUNTU: return "\033[38;2;255;165;0m";
        case DEBIAN: return "\033[31m";
        case CACHYOS: return "\033[34m";
        default: return "\033[36m";
    }
}

string get_distro_name(Distro distro) {
    switch(distro) {
        case ARCH: return "Arch";
        case UBUNTU: return "Ubuntu";
        case DEBIAN: return "Debian";
        case CACHYOS: return "CachyOS";
        case NEON: return "KDE Neon";
        default: return "Unknown";
    }
}

string get_kernel_version() {
    string version = "unknown";
    FILE* fp = popen("uname -r", "r");
    if (!fp) {
        perror("Failed to get kernel version");
        return version;
    }
    char buffer[256];
    if (fgets(buffer, sizeof(buffer), fp)) {
        version = buffer;
        version.erase(version.find_last_not_of("\n") + 1);
    }
    pclose(fp);
    return version;
}

string read_clone_dir() {
    string file_path = "/home/" + string(getenv("USER")) + "/.config/cmi/clonedir.txt";
    ifstream f(file_path, ios::in | ios::binary);
    if (!f) return "";

    f.seekg(0, ios::end);
    size_t size = f.tellg();
    f.seekg(0, ios::beg);

    if (size == 0) return "";

    string dir_path(size, '\0');
    f.read(&dir_path[0], size);

    if (!dir_path.empty() && dir_path.back() == '\n') {
        dir_path.pop_back();
    }
    if (!dir_path.empty() && dir_path.back() != '/') {
        dir_path += '/';
    }

    return dir_path;
}

void save_clone_dir(const string &dir_path) {
    string config_dir = "/home/" + string(getenv("USER")) + "/.config/cmi";
    if (!dir_exists(config_dir)) {
        string mkdir_cmd = "mkdir -p " + config_dir;
        execute_command(mkdir_cmd);
    }

    string full_clone_path = dir_path;
    if (full_clone_path.back() != '/') {
        full_clone_path += '/';
    }

    string file_path = config_dir + "/clonedir.txt";
    ofstream f(file_path, ios::out | ios::trunc);
    if (!f) {
        perror("Failed to open clonedir.txt");
        return;
    }
    f << full_clone_path;
    f.close();

    string clone_folder = full_clone_path + "clone_system_temp";
    if (!dir_exists(clone_folder)) {
        string mkdir_cmd = "mkdir -p " + clone_folder;
        execute_command(mkdir_cmd);
    }

    for (int i = 5; i > 0; i--) {
        sleep(1);
    }
    should_reset = true;
}

void save_iso_name(const string &name) {
    string config_dir = "/home/" + string(getenv("USER")) + "/.config/cmi";
    if (!dir_exists(config_dir)) {
        string mkdir_cmd = "mkdir -p " + config_dir;
        execute_command(mkdir_cmd);
    }

    string file_path = config_dir + "/isoname.txt";
    ofstream f(file_path, ios::out | ios::trunc);
    if (!f) {
        perror("Failed to open isoname.txt");
        return;
    }
    f << name;
    f.close();
    should_reset = true;
}

void set_iso_name() {
    string name = dialog_input("ISO Name", "Enter ISO name (without extension):");
    if (name.empty()) {
        error_box("Error", "ISO name cannot be empty");
        return;
    }
    save_iso_name(name);
}

void print_banner(Distro distro) {
    cout << RED;
    cout <<
    "░█████╗░██╗░░░░░░█████╗░██╗░░░██╗██████╗░███████╗███╗░░░███╗░█████╗░██████╗░░██████╗\n"
    "██╔══██╗██║░░░░░██╔══██╗██║░░░██║██╔══██╗██╔════╝████╗░████║██╔══██╗██╔══██╗██╔════╝\n"
    "██║░░╚═╝██║░░░░░██║░░██║██║░░░██║██║░░██║█████╗░░██╔████╔██║██║░░██║██║░░██║╚█████╗░\n"
    "██║░░██╗██║░░░░░██║░░██║██║░░░██║██║░░██║██╔══╝░░██║╚██╔╝██║██║░░██║██║░░██║░╚═══██╗\n"
    "╚█████╔╝███████╗██║░░██║╚██████╔╝██████╔╝███████╗██║░╚═╝░██║╚█████╔╝██████╔╝██████╔╝\n"
    "░╚════╝░╚══════╝╚═╝░░╚═╝░╚═════╝░╚═════╝░╚══════╝╚═╝░░░░░╚═╝░╚════╝░╚═════╝░╚═════╝░\n";
        cout << RESET;
        cout << RED << "Claudemods Multi Iso Creator Advanced C++ Script v2.0 DevBranch 01-07-2025" << RESET << endl;

        // Display current distribution and kernel version above time
        cout << GREEN << "Current Distribution: " << get_distro_name(distro) << RESET << endl;
        cout << GREEN << "Current Kernel: " << get_kernel_version() << RESET << endl;

        {
            lock_guard<mutex> lock(time_mutex);
            cout << GREEN << "Current UK Time: " << current_time_str << RESET << endl;
        }

        cout << get_clone_dir_status() << endl;
        cout << get_init_status(distro) << endl;
        cout << get_iso_name_status() << endl;

        cout << GREEN << "Disk Usage:" << RESET << endl;
        string cmd = "df -h /";
        unique_ptr<FILE, int(*)(FILE*)> pipe(popen(cmd.c_str(), "r"), pclose);
        if (pipe) {
            char buffer[128];
            while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
                cout << GREEN << buffer << RESET;
            }
        }
}

int get_key() {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 10000;

    int retval = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);

    if (retval == -1) {
        perror("select()");
        return 0;
    } else if (retval) {
        int c = getchar();
        if (c == '\033') {
            getchar();
            return getchar();
        }
        return c;
    }
    return 0;
}

void print_blue(const string &text) {
    cout << BLUE << text << RESET << endl;
}

string prompt(const string &prompt_text) {
    return dialog_input("Input", prompt_text, 0, 0);
}

string password_prompt(const string &prompt_text) {
    return dialog_password("Password", prompt_text, 0, 0);
}

void update_time_thread() {
    while (time_thread_running) {
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        char datetime[50];
        strftime(datetime, sizeof(datetime), "%d/%m/%Y %H:%M:%S", t);

        {
            lock_guard<mutex> lock(time_mutex);
            current_time_str = datetime;
        }

        sleep(1);
    }
}

int show_menu(const string &title, const vector<string> &items, int selected, Distro distro = ARCH) {
    vector<string> dialog_items;
    for (size_t i = 0; i < items.size(); i++) {
        dialog_items.push_back(to_string(i) + " \"" + items[i] + "\"");
    }

    string command = "dialog --colors --title \"" + title + "\" --menu \"\" 0 0 0 ";
    for (const auto& item : dialog_items) {
        command += item + " ";
    }
    command += "2>&1 >/dev/tty";

    array<char, 128> buffer;
    string result;
    unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) {
        throw runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    if (result.empty()) {
        return -1; // User cancelled
    }

    // Remove trailing newline if present
    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }

    return stoi(result);
}

void install_dependencies_arch() {
    progress_dialog("Installing dependencies...");
    const string packages =
    "arch-install-scripts "
    "bash-completion "
    "dosfstools "
    "erofs-utils "
    "findutils "
    "grub "
    "jq "
    "libarchive "
    "libisoburn "
    "lsb-release "
    "lvm2 "
    "mkinitcpio-archiso "
    "mkinitcpio-nfs-utils "
    "mtools "
    "nbd "
    "pacman-contrib "
    "parted "
    "procps-ng "
    "pv "
    "python "
    "rsync "
    "squashfs-tools "
    "sshfs "
    "syslinux "
    "xdg-utils "
    "bash-completion "
    "zsh-completions "
    "kernel-modules-hook "
    "virt-manager ";
    execute_command("sudo pacman -S --needed --noconfirm " + packages);
    message_box("Success", "Dependencies installed successfully.");
}

void generate_initrd_arch() {
    progress_dialog("Generating Initramfs And Copying Vmlinuz (Arch)...");
    execute_command("cd /home/$USER/.config/cmi/build-image-arch >/dev/null 2>&1 && sudo mkinitcpio -c live.conf -g /home/$USER/.config/cmi/build-image-arch/live/initramfs-linux.img");
    execute_command("sudo cp /boot/vmlinuz-linux* /home/$USER/.config/cmi/build-image-arch/live/ 2>/dev/null");
    message_box("Success", "Initramfs And Vmlinuz generated successfully.");
}

void edit_grub_cfg_arch() {
    progress_dialog("Opening grub.cfg (arch)...");
    execute_command("nano /home/$USER/.config/cmi/build-image-arch/boot/grub/grub.cfg");
    message_box("Success", "grub.cfg opened for editing.");
}

void edit_isolinux_cfg_arch() {
    progress_dialog("Opening isolinux.cfg (arch)...");
    execute_command("nano /home/$USER/.config/cmi/build-image-arch/isolinux/isolinux.cfg");
    message_box("Success", "isolinux.cfg opened for editing.");
}

void install_calamares_arch() {
    progress_dialog("Installing Calamares for Arch Linux...");
    execute_command("cd /home/$USER/.config/cmi/calamares-per-distro/arch >/dev/null 2>&1 && sudo pacman -U calamares-3.3.14-5-x86_64_REPACKED.pkg.tar.zst calamares-oem-kde-settings-20240616-3-any.pkg.tar calamares-tools-0.1.0-1-any.pkg.tar ckbcomp-1.227-2-any.pkg.tar.zst");
    message_box("Success", "Calamares installed successfully for Arch Linux.");
}

void edit_calamares_branding() {
    progress_dialog("Opening Calamares branding configuration...");
    execute_command("sudo nano /usr/share/calamares/branding/claudemods/branding.desc");
    message_box("Success", "Calamares branding configuration opened for editing.");
}

void install_dependencies_cachyos() {
    progress_dialog("Installing dependencies...");
    const string packages =
    "arch-install-scripts "
    "bash-completion "
    "dosfstools "
    "erofs-utils "
    "findutils "
    "grub "
    "jq "
    "libarchive "
    "libisoburn "
    "lsb-release "
    "lvm2 "
    "mkinitcpio-archiso "
    "mkinitcpio-nfs-utils "
    "mtools "
    "nbd "
    "pacman-contrib "
    "parted "
    "procps-ng "
    "pv "
    "python "
    "rsync "
    "squashfs-tools "
    "sshfs "
    "syslinux "
    "xdg-utils "
    "bash-completion "
    "zsh-completions "
    "kernel-modules-hook "
    "virt-manager ";
    execute_command("sudo pacman -S --needed --noconfirm " + packages);
    message_box("Success", "Dependencies installed successfully.");
}

void install_calamares_cachyos() {
    progress_dialog("Installing Calamares for CachyOS...");
    execute_command("sudo pacman -U --noconfirm calamares-3.3.14-5-x86_64_REPACKED.pkg.tar.zst calamares-oem-kde-settings-20240616-3-any.pkg.tar calamares-tools-0.1.0-1-any.pkg.tar ckbcomp-1.227-2-any.pkg.tar.zst");
    message_box("Success", "Calamares installed successfully for CachyOS.");
}

void install_dependencies_ubuntu() {
    progress_dialog("Installing Ubuntu dependencies...");
    const string packages =
    "cryptsetup "
    "dmeventd "
    "isolinux "
    "libaio-dev "
    "libcares2 "
    "libdevmapper-event1.02.1 "
    "liblvm2cmd2.03 "
    "live-boot "
    "live-boot-doc "
    "live-boot-initramfs-tools "
    "live-config-systemd "
    "live-tools "
    "lvm2 "
    "pxelinux "
    "syslinux "
    "syslinux-common "
    "thin-provisioning-tools "
    "squashfs-tools "
    "xorriso ";

    execute_command("sudo apt install -y " + packages);
    message_box("Success", "Ubuntu dependencies installed successfully.");
}

void copy_vmlinuz_ubuntu() {
    string kernel_version = get_kernel_version();
    execute_command("sudo cp /boot/vmlinuz-" + kernel_version + " /home/$USER/.config/cmi/build-image-noble/live/");
    message_box("Success", "Vmlinuz copied successfully for Ubuntu.");
}

void generate_initrd_ubuntu() {
    progress_dialog("Generating Initramfs for Ubuntu...");
    execute_command("sudo mkinitramfs -o \"/home/$USER/.config/cmi/build-image-noble/live/initrd.img-$(uname -r)\" \"$(uname -r)\"");
    execute_command("sudo cp /boot/vmlinuz* /home/$USER/.config/cmi/build-image-noble/live/ 2>/dev/null");
    message_box("Success", "Ubuntu initramfs And Vmlinuz generated successfully.");
}

void edit_grub_cfg_ubuntu() {
    progress_dialog("Opening Ubuntu grub.cfg...");
    execute_command("sudo nano /home/$USER/.config/cmi/build-image-noble/boot/grub/grub.cfg");
    message_box("Success", "Ubuntu grub.cfg opened for editing.");
}

void edit_isolinux_cfg_ubuntu() {
    progress_dialog("Opening Ubuntu isolinux.cfg...");
    execute_command("sudo nano /home/$USER/.config/cmi/build-image-noble/isolinux/isolinux.cfg");
    message_box("Success", "Ubuntu isolinux.cfg opened for editing.");
}

void install_calamares_ubuntu() {
    progress_dialog("Installing Calamares for Ubuntu...");
    execute_command("sudo apt install -y calamares-settings-ubuntu-common calamares");
    message_box("Success", "Calamares installed successfully for Ubuntu.");
}

void install_dependencies_debian() {
    progress_dialog("Installing Debian dependencies...");
    const string packages =
    "cryptsetup "
    "dmeventd "
    "isolinux "
    "libaio1 "
    "libc-ares2 "
    "libdevmapper-event1.02.1 "
    "liblvm2cmd2.03 "
    "live-boot "
    "live-boot-doc "
    "live-boot-initramfs-tools "
    "live-config-systemd "
    "live-tools "
    "lvm2 "
    "pxelinux "
    "syslinux "
    "syslinux-common "
    "thin-provisioning-tools "
    "squashfs-tools "
    "xorriso ";
    execute_command("sudo apt install -y " + packages);
    message_box("Success", "Debian dependencies installed successfully.");
}

void copy_vmlinuz_debian() {
    string kernel_version = get_kernel_version();
    execute_command("sudo cp /boot/vmlinuz-" + kernel_version + " /home/$USER/.config/cmi/build-image-debian/live/");
    message_box("Success", "Vmlinuz copied successfully for Debian.");
}

void generate_initrd_debian() {
    progress_dialog("Generating Initramfs for Debian...");
    execute_command("sudo mkinitramfs -o \"/home/$USER/.config/cmi/build-image-debian/live/initrd.img-$(uname -r)\" \"$(uname -r)\"");
    execute_command("sudo cp /boot/vmlinuz* /home/$USER/.config/cmi/build-image-debian/live/ 2>/dev/null");
    message_box("Success", "Debian initramfs And Vmlinuz generated successfully.");
}

void edit_grub_cfg_debian() {
    progress_dialog("Opening Debian grub.cfg...");
    execute_command("sudo nano /home/$USER/.config/cmi/build-image-debian/boot/grub/grub.cfg");
    message_box("Success", "Debian grub.cfg opened for editing.");
}

void edit_isolinux_cfg_debian() {
    progress_dialog("Opening Debian isolinux.cfg...");
    execute_command("nano /home/$USER/.config/cmi/build-image-debian/isolinux/isolinux.cfg");
    message_box("Success", "Debian isolinux.cfg opened for editing.");
}

void install_calamares_debian() {
    progress_dialog("Installing Calamares for Debian...");
    execute_command("sudo apt install -y calamares-settings-debian calamares");
    message_box("Success", "Calamares installed successfully for Debian.");
}

void clone_system(const string &clone_dir) {
    string full_clone_path = clone_dir;
    if (full_clone_path.back() != '/') {
        full_clone_path += '/';
    }
    full_clone_path += "clone_system_temp";

    if (!dir_exists(full_clone_path)) {
        string mkdir_cmd = "mkdir -p " + full_clone_path;
        execute_command(mkdir_cmd);
    }

    string parent_dir = clone_dir;
    if (parent_dir.back() == '/') {
        parent_dir.pop_back();
    }
    size_t last_slash = parent_dir.find_last_of('/');
    if (last_slash != string::npos) {
        parent_dir = parent_dir.substr(last_slash + 1);
    }

    string command = "sudo rsync -aHAXSr --numeric-ids --info=progress2 "
    "--exclude=/etc/udev/rules.d/70-persistent-cd.rules "
    "--exclude=/etc/udev/rules.d/70-persistent-net.rules "
    "--exclude=/etc/mtab "
    "--exclude=/etc/fstab "
    "--exclude=/dev/* "
    "--exclude=/proc/* "
    "--exclude=/sys/* "
    "--exclude=/tmp/* "
    "--exclude=/run/* "
    "--exclude=/mnt/* "
    "--exclude=/media/* "
    "--exclude=/lost+found "
    "--exclude=clone_system_temp "
    "--include=dev "
    "--include=proc "
    "--include=tmp "
    "--include=sys "
    "--include=run "
    "--include=dev "
    "--include=proc "
    "--include=tmp "
    "--include=sys "
    "--include=usr "
    "--include=etc "
    "/ " + full_clone_path;

    cout << GREEN << "Cloning system into directory: " << full_clone_path << RESET << endl;
    execute_command(command);
}

void create_squashfs_image(Distro distro) {
    string clone_dir = read_clone_dir();
    if (clone_dir.empty()) {
        error_box("Error", "No clone directory specified. Please set it in Setup Script menu.");
        return;
    }

    string full_clone_path = clone_dir;
    if (full_clone_path.back() != '/') {
        full_clone_path += '/';
    }
    full_clone_path += "clone_system_temp";

    string output_path;
    if (distro == UBUNTU) {
        output_path = "/home/$USER/.config/cmi/build-image-noble/live/filesystem.sfs";
    } else if (distro == DEBIAN) {
        output_path = "/home/$USER/.config/cmi/build-image-debian/live/filesystem.sfs";
    } else {
        output_path = "/home/$USER/.config/cmi/build-image-arch/arch/x86_64/airootfs.sfs";
    }

    string command = "sudo mksquashfs " + full_clone_path + " " + output_path + " "
    "-comp xz -Xbcj x86 -b 1M -no-duplicates -no-recovery "
    "-always-use-fragments -wildcards -xattrs";

    cout << GREEN << "Creating SquashFS image from: " << full_clone_path << RESET << endl;
    execute_command(command);

    string del_cmd = "sudo rm -rf " + full_clone_path;
    execute_command(del_cmd);
}

void delete_clone_system_temp(Distro distro) {
    string clone_dir = read_clone_dir();
    if (clone_dir.empty()) {
        error_box("Error", "No clone directory specified. Please set it in Setup Script menu.");
        return;
    }

    string full_clone_path = clone_dir;
    if (full_clone_path.back() != '/') {
        full_clone_path += '/';
    }
    full_clone_path += "clone_system_temp";

    string command = "sudo rm -rf " + full_clone_path;
    cout << GREEN << "Deleting clone directory: " << full_clone_path << RESET << endl;
    execute_command(command);

    string squashfs_path;
    if (distro == UBUNTU) {
        squashfs_path = "/home/$USER/.config/cmi/build-image-noble/live/filesystem.sfs";
    } else if (distro == DEBIAN) {
        squashfs_path = "/home/$USER/.config/cmi/build-image-debian/live/filesystem.sfs";
    } else {
        squashfs_path = "/home/$USER/.config/cmi/build-image-arch/arch/x86_64/airootfs.sfs";
    }

    struct stat st;
    if (stat(squashfs_path.c_str(), &st) == 0) {
        command = "sudo rm -f " + squashfs_path;
        cout << GREEN << "Deleting SquashFS image: " << squashfs_path << RESET << endl;
        execute_command(command);
    } else {
        cout << GREEN << "SquashFS image does not exist: " << squashfs_path << RESET << endl;
    }
}

void set_clone_directory() {
    string dir_path = dialog_input("Clone Directory", "Enter full path for clone directory e.g /home/$USER/Pictures:");
    if (dir_path.empty()) {
        error_box("Error", "Directory path cannot be empty");
        return;
    }

    if (dir_path.back() != '/') {
        dir_path += '/';
    }

    save_clone_dir(dir_path);
}

void install_one_time_updater() {
    progress_dialog("Installing one-time updater...");
    execute_command("./home/$USER/.config/cmi/patch.sh");
    message_box("Success", "One-time updater installed successfully in /home/$USER/.config/cmi");
}

void squashfs_menu(Distro distro) {
    vector<string> items = {
        "Max compression (xz)",
        "Create SquashFS from clone directory",
        "Delete clone directory and SquashFS image",
        "Back to Main Menu"
    };

    while (true) {
        int choice = dialog_menu("SquashFS Creator", items, 0, 0);
        if (choice == -1 || choice == 3) {
            return;
        }

        switch (choice) {
            case 0:
            {
                string clone_dir = read_clone_dir();
                if (clone_dir.empty()) {
                    error_box("Error", "No clone directory specified. Please set it in Setup Script menu.");
                    break;
                }
                if (!dir_exists(clone_dir + "/clone_system_temp")) {
                    clone_system(clone_dir);
                }
                create_squashfs_image(distro);
            }
            break;
            case 1:
                create_squashfs_image(distro);
                break;
            case 2:
                delete_clone_system_temp(distro);
                break;
        }
    }
}

void create_iso(Distro distro) {
    string iso_name = get_iso_name();
    if (iso_name.empty()) {
        error_box("Error", "ISO name not set. Please set it in Setup Script menu (Option 3)");
        return;
    }

    string output_dir = dialog_input("Output Directory", "Enter the output directory path:");
    if (output_dir.empty()) {
        error_box("Input Error", "Output directory cannot be empty.");
        return;
    }

    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d_%H%M", t);

    std::ostringstream oss;
    oss << output_dir << std::filesystem::path::preferred_separator
    << iso_name << "_amd64_" << timestamp << ".iso";
    string iso_file_name = oss.str();

    string build_image_dir;
    if (distro == UBUNTU) {
        build_image_dir = "/home/" + string(getenv("USER")) + "/.config/cmi/build-image-noble";
    } else if (distro == DEBIAN) {
        build_image_dir = "/home/" + string(getenv("USER")) + "/.config/cmi/build-image-debian";
    } else {
        build_image_dir = "/home/" + string(getenv("USER")) + "/.config/cmi/build-image-arch";
    }

    if (!dir_exists(output_dir)) {
        execute_command("mkdir -p " + output_dir);
    }

    oss.str("");
    oss << "sudo xorriso -as mkisofs -o " << iso_file_name
    << " -V 2025 -iso-level 3";

    if (distro == UBUNTU) {
        oss << " -isohybrid-mbr /usr/lib/ISOLINUX/isohdpfx.bin"
        << " -c isolinux/boot.cat"
        << " -b isolinux/isolinux.bin -no-emul-boot -boot-load-size 4 -boot-info-table"
        << " -eltorito-alt-boot -e boot/grub/efi.img -no-emul-boot -isohybrid-gpt-basdat";
    } else if (distro == DEBIAN) {
        oss << " -isohybrid-mbr /usr/lib/ISOLINUX/isohdpfx.bin"
        << " -c isolinux/boot.cat"
        << " -b isolinux/isolinux.bin -no-emul-boot -boot-load-size 4 -boot-info-table"
        << " -eltorito-alt-boot -e boot/grub/efiboot.img -no-emul-boot -isohybrid-gpt-basdat";
    } else {
        oss << " -isohybrid-mbr /usr/lib/syslinux/bios/isohdpfx.bin"
        << " -c isolinux/boot.cat"
        << " -b isolinux/isolinux.bin -no-emul-boot -boot-load-size 4 -boot-info-table"
        << " -eltorito-alt-boot -e boot/grub/efiboot.img -no-emul-boot -isohybrid-gpt-basdat";
    }

    oss << " " << build_image_dir;

    string xorriso_command = oss.str();
    execute_command(xorriso_command);

    message_box("Success", "ISO creation completed.");

    string choice = dialog_input("Continue", "Press 'm' to go back to main menu or Enter to exit:");
    if (!choice.empty() && (choice[0] == 'm' || choice[0] == 'M')) {
        execute_command("ruby /opt/claudemods-iso-konsole-script/demo.rb");
    }
}

void run_iso_in_qemu() {
    execute_command("ruby /opt/claudemods-iso-konsole-script/Supported-Distros/qemu.rb");
}

void iso_creator_menu(Distro distro) {
    vector<string> items = {
        "Create ISO",
        "Run ISO in QEMU",
        "Back to Main Menu"
    };

    while (true) {
        int choice = dialog_menu("ISO Creator Menu", items, 0, 0);
        if (choice == -1 || choice == 2) {
            return;
        }

        switch (choice) {
            case 0:
                create_iso(distro);
                break;
            case 1:
                run_iso_in_qemu();
                break;
        }
    }
}

void create_command_files() {
    char exe_path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path)-1);
    if (len == -1) {
        perror("readlink");
        return;
    }
    exe_path[len] = '\0';
    string command = "bash -c 'cat > /usr/bin/gen-init << \"EOF\"\n"
    "#!/bin/sh\n"
    "if [ \"$1\" = \"--help\" ]; then\n"
    "  echo \"Usage: gen-init\"\n"
    "  echo \"Generate initcpio configuration\"\n"
    "  exit 0\n"
    "fi\n"
    "exec " + string(exe_path) + " 5\n"
    "EOF\n"
    "chmod 755 /usr/bin/gen-init'";
    execute_command("sudo " + command);

    command = "bash -c 'cat > /usr/bin/edit-isocfg << \"EOF\"\n"
    "#!/bin/sh\n"
    "if [ \"$1\" = \"--help\" ]; then\n"
    "  echo \"Usage: edit-isocfg\"\n"
    "  echo \"Edit isolinux.cfg file\"\n"
    "  exit 0\n"
    "fi\n"
    "exec " + string(exe_path) + " 6\n"
    "EOF\n"
    "chmod 755 /usr/bin/edit-isocfg'";
    execute_command("sudo " + command);

    command = "bash -c 'cat > /usr/bin/edit-grubcfg << \"EOF\"\n"
    "#!/bin/sh\n"
    "if [ \"$1\" = \"--help\" ]; then\n"
    "  echo \"Usage: edit-grubcfg\"\n"
    "  echo \"Edit grub.cfg file\"\n"
    "  exit 0\n"
    "fi\n"
    "exec " + string(exe_path) + " 7\n"
    "EOF\n"
    "chmod 755 /usr/bin/edit-grubcfg'";
    execute_command("sudo " + command);

    command = "bash -c 'cat > /usr/bin/setup-script << \"EOF\"\n"
    "#!/bin/sh\n"
    "if [ \"$1\" = \"--help\" ]; then\n"
    "  echo \"Usage: setup-script\"\n"
    "  echo \"Open setup script menu\"\n"
    "  exit 0\n"
    "fi\n"
    "exec " + string(exe_path) + " 8\n"
    "EOF\n"
    "chmod 755 /usr/bin/setup-script'";
    execute_command("sudo " + command);

    command = "bash -c 'cat > /usr/bin/make-iso << \"EOF\"\n"
    "#!/bin/sh\n"
    "if [ \"$1\" = \"--help\" ]; then\n"
    "  echo \"Usage: make-iso\"\n"
    "  echo \"Launches the ISO creation menu\"\n"
    "  exit 0\n"
    "fi\n"
    "exec " + string(exe_path) + " 3\n"
    "EOF\n"
    "chmod 755 /usr/bin/make-iso'";
    execute_command("sudo " + command);

    command = "bash -c 'cat > /usr/bin/make-squashfs << \"EOF\"\n"
    "#!/bin/sh\n"
    "if [ \"$1\" = \"--help\" ]; then\n"
    "  echo \"Usage: make-squashfs\"\n"
    "  echo \"Launches the SquashFS creation menu\"\n"
    "  exit 0\n"
    "fi\n"
    "exec " + string(exe_path) + " 4\n"
    "EOF\n"
    "chmod 755 /usr/bin/make-squashfs'";
    execute_command("sudo " + command);

    command = "bash -c 'cat > /usr/bin/gen-calamares << \"EOF\"\n"
    "#!/bin/sh\n"
    "if [ \"$1\" = \"--help\" ]; then\n"
    "  echo \"Usage: gen-calamares\"\n"
    "  echo \"Install Calamares installer\"\n"
    "  exit 0\n"
    "fi\n"
    "exec " + string(exe_path) + " 9\n"
    "EOF\n"
    "chmod 755 /usr/bin/gen-calamares'";
    execute_command("sudo " + command);

    command = "bash -c 'cat > /usr/bin/edit-branding << \"EOF\"\n"
    "#!/bin/sh\n"
    "if [ \"$1\" = \"--help\" ]; then\n"
    "  echo \"Usage: edit-branding\"\n"
    "  echo \"Edit Calamares branding\"\n"
    "  exit 0\n"
    "fi\n"
    "exec " + string(exe_path) + " 10\n"
    "EOF\n"
    "chmod 755 /usr/bin/edit-branding'";
    execute_command("sudo " + command);

    cout << GREEN << "Activated! You can now use all commands in your terminal." << RESET << endl;
}

void remove_command_files() {
    execute_command("sudo rm -f /usr/bin/gen-init");
    execute_command("sudo rm -f /usr/bin/edit-isocfg");
    execute_command("sudo rm -f /usr/bin/edit-grubcfg");
    execute_command("sudo rm -f /usr/bin/setup-script");
    execute_command("sudo rm -f /usr/bin/make-iso");
    execute_command("sudo rm -f /usr/bin/make-squashfs");
    execute_command("sudo rm -f /usr/bin/gen-calamares");
    execute_command("sudo rm -f /usr/bin/edit-branding");
    cout << GREEN << "Commands deactivated and removed from system." << RESET << endl;
}

void command_installer_menu(Distro distro) {
    vector<string> items = {
        "Activate terminal commands",
        "Deactivate terminal commands",
        "Back to Main Menu"
    };

    while (true) {
        int choice = dialog_menu("Command Installer Menu", items, 0, 0);
        if (choice == -1 || choice == 2) {
            return;
        }

        switch (choice) {
            case 0:
                create_command_files();
                break;
            case 1:
                remove_command_files();
                break;
        }
    }
}

void setup_script_menu(Distro distro) {
    string distro_name = get_distro_name(distro);
    vector<string> items;

    switch(distro) {
        case ARCH:
            items = {
                "Install Dependencies (Arch)",
                "Generate initcpio configuration (arch)",
                "Set ISO Name",
                "Edit isolinux.cfg (arch)",
                "Edit grub.cfg (arch)",
                "Set clone directory path",
                "Install Calamares",
                "Edit Calamares Branding",
                "Install One Time Updater",
                "Back to Main Menu"
            };
            break;
        case UBUNTU:
            items = {
                "Install Dependencies (Ubuntu)",
                "Generate initramfs (ubuntu)",
                "Set ISO Name",
                "Edit isolinux.cfg (ubuntu)",
                "Edit grub.cfg (ubuntu)",
                "Set clone directory path",
                "Install Calamares",
                "Edit Calamares Branding",
                "Install One Time Updater",
                "Back to Main Menu"
            };
            break;
        case DEBIAN:
            items = {
                "Install Dependencies (Debian)",
                "Generate initramfs (debian)",
                "Set ISO Name",
                "Edit isolinux.cfg (debian)",
                "Edit grub.cfg (debian)",
                "Set clone directory path",
                "Install Calamares",
                "Edit Calamares Branding",
                "Install One Time Updater",
                "Back to Main Menu"
            };
            break;
        case CACHYOS:
            items = {
                "Install Dependencies (CachyOS)",
                "Generate initcpio configuration (cachyos)",
                "Set ISO Name",
                "Edit isolinux.cfg (cachyos)",
                "Edit grub.cfg (cachyos)",
                "Set clone directory path",
                "Install Calamares",
                "Edit Calamares Branding",
                "Install One Time Updater",
                "Back to Main Menu"
            };
            break;
        case NEON:
            items = {
                "Install Dependencies (Ubuntu)",
                "Generate initramfs (ubuntu)",
                "Set ISO Name",
                "Edit isolinux.cfg (ubuntu)",
                "Edit grub.cfg (ubuntu)",
                "Set clone directory path",
                "Install Calamares",
                "Edit Calamares Branding",
                "Install One Time Updater",
                "Back to Main Menu"
            };
            break;
        case UNKNOWN:
            error_box("Error", "Unsupported distribution");
            return;
    }

    while (true) {
        int choice = dialog_menu("Setup Script Menu", items, 0, 0);
        if (choice == -1 || choice == items.size() - 1) {
            return;
        }

        switch(choice) {
            case 0:
                if (distro == ARCH) install_dependencies_arch();
                else if (distro == CACHYOS) install_dependencies_cachyos();
                else if (distro == UBUNTU || distro == NEON) install_dependencies_ubuntu();
                else if (distro == DEBIAN) install_dependencies_debian();
                break;
            case 1:
                if (distro == ARCH || distro == CACHYOS) generate_initrd_arch();
                else if (distro == UBUNTU || distro == NEON) generate_initrd_ubuntu();
                else if (distro == DEBIAN) generate_initrd_debian();
                break;
            case 2:
                set_iso_name();
                break;
            case 3:
                if (distro == ARCH || distro == CACHYOS) edit_isolinux_cfg_arch();
                else if (distro == UBUNTU || distro == NEON) edit_isolinux_cfg_ubuntu();
                else if (distro == DEBIAN) edit_isolinux_cfg_debian();
                break;
            case 4:
                if (distro == ARCH || distro == CACHYOS) edit_grub_cfg_arch();
                else if (distro == UBUNTU || distro == NEON) edit_grub_cfg_ubuntu();
                else if (distro == DEBIAN) edit_grub_cfg_debian();
                break;
            case 5:
                set_clone_directory();
                break;
            case 6:
                if (distro == ARCH) install_calamares_arch();
                else if (distro == CACHYOS) install_calamares_cachyos();
                else if (distro == UBUNTU || distro == NEON) install_calamares_ubuntu();
                else if (distro == DEBIAN) install_calamares_debian();
                break;
            case 7:
                edit_calamares_branding();
                break;
            case 8:
                install_one_time_updater();
                break;
        }
    }
}

int main(int argc, char* argv[]) {
    tcgetattr(STDIN_FILENO, &original_term);
    thread time_thread(update_time_thread);

    if (argc > 1) {
        int option = atoi(argv[1]);
        Distro distro = UNKNOWN;

        switch(option) {
            case 5:
                distro = detect_distro();
                if (distro == ARCH || distro == CACHYOS) generate_initrd_arch();
                else if (distro == UBUNTU || distro == NEON) generate_initrd_ubuntu();
                else if (distro == DEBIAN) generate_initrd_debian();
                break;
            case 6:
                distro = detect_distro();
                if (distro == ARCH || distro == CACHYOS) edit_isolinux_cfg_arch();
                else if (distro == UBUNTU || distro == NEON) edit_isolinux_cfg_ubuntu();
                else if (distro == DEBIAN) edit_isolinux_cfg_debian();
                break;
            case 7:
                distro = detect_distro();
                if (distro == ARCH || distro == CACHYOS) edit_grub_cfg_arch();
                else if (distro == UBUNTU || distro == NEON) edit_grub_cfg_ubuntu();
                else if (distro == DEBIAN) edit_grub_cfg_debian();
                break;
            case 8:
                setup_script_menu(detect_distro());
                break;
            case 3:
                iso_creator_menu(detect_distro());
                break;
            case 4:
                squashfs_menu(detect_distro());
                break;
            case 9:
                distro = detect_distro();
                if (distro == ARCH) install_calamares_arch();
                else if (distro == CACHYOS) install_calamares_cachyos();
                else if (distro == UBUNTU || distro == NEON) install_calamares_ubuntu();
                else if (distro == DEBIAN) install_calamares_debian();
                break;
            case 10:
                edit_calamares_branding();
                break;
            default:
                cout << "Invalid option" << endl;
        }

        time_thread_running = false;
        time_thread.join();
        return 0;
    }

    Distro distro = detect_distro();
    vector<string> items = {
        "Guide",
        "Setup Script",
        "SquashFS Creator",
        "ISO Creator",
        "Command Installer",
        "Changelog",
        "Exit"
    };

    while (true) {
        if (should_reset) {
            should_reset = false;
            system("clear");
            continue;
        }

        int choice = dialog_menu("Main Menu", items, 0, 0);
        if (choice == -1 || choice == 6) {
            time_thread_running = false;
            time_thread.join();
            disable_raw_mode();
            return 0;
        }

        switch (choice) {
            case 0:
                execute_command("nano /home/$USER/.config/cmi/readme.txt");
                break;
            case 1:
                setup_script_menu(distro);
                break;
            case 2:
                squashfs_menu(distro);
                break;
            case 3:
                iso_creator_menu(distro);
                break;
            case 4:
                command_installer_menu(distro);
                break;
            case 5:
                execute_command("nano /home/$USER/.config/cmi/changes.txt");
                break;
        }
    }
}

