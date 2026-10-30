
#include <iostream>
#include <string>
#include <map>
#include <sstream>
#include <vector>
#include <fstream>
#include <iomanip>
#include <cstdlib>
#include<set>
using namespace std;

enum FileType { REGULAR_FILE, DIRECTORY, SOFT_LINK };

struct Inode {
    int id;
    FileType type;
    string content;
    string target;
    int linkCount;
    map<string, int> entries;
    int parentID;
};

map<int, Inode> inodes;
int nextInodeID = 1;
int rootID = 0;
int currentDirectoryID = 0;

void initializeFileSystem() {
    Inode root;
    root.id = nextInodeID++;
    root.type = DIRECTORY;
    root.content = "";
    root.target = "";
    root.linkCount = 1;
    root.parentID = root.id;
    root.entries["."] = root.id;
    root.entries[".."] = root.id;

    inodes[root.id] = root;
    rootID = currentDirectoryID = root.id;
}

bool validName(string name) {
    return !name.empty() && name != "." && name != ".."
           && name.find('/') == string::npos;
}

void mkdirCommand(string name) {
    if (!validName(name)) {
        cout << "Invalid directory name.\n";
        return;
    }

    if (inodes[currentDirectoryID].entries.count(name)) {
        cout << "Name already exists.\n";
        return;
    }

    Inode dir;
    dir.id = nextInodeID++;
    dir.type = DIRECTORY;
    dir.content = "";
    dir.target = "";
    dir.linkCount = 1;
    dir.parentID = currentDirectoryID;
    dir.entries["."] = dir.id;
    dir.entries[".."] = currentDirectoryID;

    int id = dir.id;
    inodes[id] = dir;
    inodes[currentDirectoryID].entries[name] = id;
    cout << "Directory created: " << name << "\n";
}

void touchCommand(string name) {
    if (!validName(name)) {
        cout << "Invalid file name.\n";
        return;
    }

    if (inodes[currentDirectoryID].entries.count(name)) {
        cout << "Name already exists.\n";
        return;
    }

    Inode file;
    file.id = nextInodeID++;
    file.type = REGULAR_FILE;
    file.content = "";
    file.target = "";
    file.linkCount = 1;
    file.parentID = currentDirectoryID;

    int id = file.id;
    inodes[id] = file;
    inodes[currentDirectoryID].entries[name] = id;
    cout << "File created: " << name << "\n";
}

void lsCommand() {
    for (const auto &entry : inodes[currentDirectoryID].entries) {
        if (entry.first == "." || entry.first == "..")
            continue;

        cout << entry.first;

        if (inodes[entry.second].type == DIRECTORY)
            cout << "/";
        else if (inodes[entry.second].type == SOFT_LINK)
            cout << "@";

        cout << "\n";
    }
}

int resolvePathInternal(
    string path,
    int startDirectory,
    set<int> &visitedLinks
) {
    if (path.empty())
        return -1;

    int current = (path[0] == '/') ? rootID : startDirectory;

    stringstream ss(path);
    string component;
    vector<string> components;

    while (getline(ss, component, '/')) {
        if (!component.empty())
            components.push_back(component);
    }

    for (size_t i = 0; i < components.size(); i++) {
        component = components[i];

        if (inodes.find(current) == inodes.end())
            return -1;

        if (inodes[current].type != DIRECTORY)
            return -1;

        if (component == ".")
            continue;

        if (component == "..") {
            current = inodes[current].parentID;
            continue;
        }

        auto &entries = inodes[current].entries;

        if (!entries.count(component))
            return -1;

        int id = entries[component];

        if (inodes[id].type == SOFT_LINK) {
            if (visitedLinks.count(id))
                return -1;

            visitedLinks.insert(id);

            string newPath = inodes[id].target;

            for (size_t j = i + 1; j < components.size(); j++) {
                if (!newPath.empty())
                    newPath += "/";

                newPath += components[j];
            }

            return resolvePathInternal(
                newPath,
                current,
                visitedLinks
            );
        }

        current = id;
    }

    return current;
}

int resolvePath(string path) {
    set<int> visitedLinks;
    return resolvePathInternal(path, currentDirectoryID, visitedLinks);
}

int resolveFile(string path) {
    int id = resolvePath(path);

    if (id == -1)
        return -1;

    if (inodes[id].type != REGULAR_FILE)
        return -1;

    return id;
}

void catCommand(string name) {
    int id = resolveFile(name);

    if (id == -1) {
        cout << "File not found or not a regular file.\n";
        return;
    }

    if (inodes[id].content.empty())
        cout << "(Empty file)\n";
    else
        cout << inodes[id].content << "\n";
}

void writeCommand(string name, string content) {
    int id = resolveFile(name);

    if (id == -1) {
        cout << "File not found or not a regular file.\n";
        return;
    }

    inodes[id].content = content;
    cout << "File content saved.\n";
}

void cdCommand(string path) {
    int id = resolvePath(path);

    if (id == -1) {
        cout << "Path not found.\n";
        return;
    }

    if (inodes[id].type != DIRECTORY) {
        cout << "Not a directory.\n";
        return;
    }

    currentDirectoryID = id;
}

void pwdCommand() {
    if (currentDirectoryID == rootID) {
        cout << "/\n";
        return;
    }

    vector<string> path;
    int current = currentDirectoryID;

    while (current != rootID) {
        int parent = inodes[current].parentID;
        string name;

        for (const auto &entry : inodes[parent].entries) {
            if (entry.second == current &&
                entry.first != "." && entry.first != "..") {
                name = entry.first;
                break;
            }
        }

        if (name.empty())
            break;

        path.push_back(name);
        current = parent;
    }

    cout << "/";
    for (int i = (int)path.size() - 1; i >= 0; i--) {
        cout << path[i];
        if (i > 0)
            cout << "/";
    }
    cout << "\n";
}

void lnCommand(string source, string linkName, bool symbolic) {
    auto &entries = inodes[currentDirectoryID].entries;

    if (!validName(linkName)) {
        cout << "Invalid link name.\n";
        return;
    }

    if (entries.count(linkName)) {
        cout << "Link name already exists.\n";
        return;
    }

    if (symbolic) {
        if (source.empty()) {
            cout << "Invalid symbolic link target.\n";
            return;
        }

        Inode link;
        link.id = nextInodeID++;
        link.type = SOFT_LINK;
        link.content = "";
        link.target = source;
        link.linkCount = 1;
        link.parentID = currentDirectoryID;

        int id = link.id;
        inodes[id] = link;
        entries[linkName] = id;

        cout << "Symbolic link created: " << linkName << "\n";
        return;
    }

    if (!entries.count(source)) {
        cout << "Source file not found.\n";
        return;
    }

    int sourceID = entries[source];

    if (inodes[sourceID].type != REGULAR_FILE) {
        cout << "Hard links to directories or symbolic links are not supported.\n";
        return;
    }

    entries[linkName] = sourceID;
    inodes[sourceID].linkCount++;

    cout << "Hard link created: " << linkName << "\n";
}


void mvCommand(string oldName, string newName) {
    auto &entries = inodes[currentDirectoryID].entries;

    if (!entries.count(oldName)) {
        cout << "Source name not found.\n";
        return;
    }

    if (!validName(newName)) {
        cout << "Invalid destination name.\n";
        return;
    }

    if (entries.count(newName)) {
        cout << "Destination name already exists.\n";
        return;
    }

    int id = entries[oldName];
    entries.erase(oldName);
    entries[newName] = id;

    // Keep same-directory symbolic links working when their target is renamed.
    for (auto &item : inodes) {
        if (item.second.type == SOFT_LINK &&
            item.second.parentID == currentDirectoryID &&
            item.second.target == oldName) {
            item.second.target = newName;
        }
    }

    cout << "Renamed: " << oldName << " -> " << newName << "\n";
}

void removeDirectoryContents(int directoryID) {
    vector<pair<string, int>> children;

    for (const auto &entry : inodes[directoryID].entries) {
        if (entry.first != "." && entry.first != "..")
            children.push_back(entry);
    }

    for (const auto &child : children) {
        string name = child.first;
        int id = child.second;

        if (inodes.find(id) == inodes.end()) {
            inodes[directoryID].entries.erase(name);
            continue;
        }

        if (inodes[id].type == DIRECTORY) {
            removeDirectoryContents(id);
            inodes.erase(id);
        }
        else if (inodes[id].type == REGULAR_FILE) {
            inodes[id].linkCount--;

            if (inodes[id].linkCount <= 0)
                inodes.erase(id);
        }
        else {
            inodes.erase(id);
        }

        inodes[directoryID].entries.erase(name);
    }
}

void rmCommand(string name, bool recursive = false) {
    auto &entries = inodes[currentDirectoryID].entries;

    if (!entries.count(name)) {
        cout << "Name not found.\n";
        return;
    }

    int id = entries[name];

    if (inodes[id].type == DIRECTORY) {
        if (id == rootID) {
            cout << "Cannot remove root directory.\n";
            return;
        }

        if (!recursive) {
            cout << "Cannot remove a directory using rm. Use rm -r.\n";
            return;
        }

        removeDirectoryContents(id);
        inodes[currentDirectoryID].entries.erase(name);
        inodes.erase(id);

        cout << "Directory and its contents removed: " << name << "\n";
        return;
    }

    entries.erase(name);

    if (inodes[id].type == REGULAR_FILE) {
        inodes[id].linkCount--;

        if (inodes[id].linkCount <= 0)
            inodes.erase(id);
    }
    else {
        inodes.erase(id);
    }

    cout << "Removed: " << name << "\n";
}

void rmdirCommand(string name) {
    auto &entries = inodes[currentDirectoryID].entries;

    if (!entries.count(name)) {
        cout << "Directory not found.\n";
        return;
    }

    int id = entries[name];

    if (inodes[id].type != DIRECTORY) {
        cout << "Not a directory.\n";
        return;
    }

    if (id == rootID) {
        cout << "Cannot remove root directory.\n";
        return;
    }

    if (inodes[id].entries.size() > 2) {
        cout << "Directory is not empty.\n";
        return;
    }

    entries.erase(name);
    inodes.erase(id);
    cout << "Directory deleted: " << name << "\n";
}

void statCommand(string name) {
    auto &entries = inodes[currentDirectoryID].entries;

    if (!entries.count(name)) {
        cout << "Name not found.\n";
        return;
    }

    int id = entries[name];
    Inode &node = inodes[id];

    cout << "Name: " << name << "\n";
    cout << "Inode ID: " << node.id << "\n";

    cout << "Type: ";
    if (node.type == REGULAR_FILE)
        cout << "Regular File\n";
    else if (node.type == DIRECTORY)
        cout << "Directory\n";
    else
        cout << "Symbolic Link\n";

    cout << "Link Count: " << node.linkCount << "\n";

    if (node.type == REGULAR_FILE)
        cout << "Size: " << node.content.size() << " bytes\n";
    else if (node.type == SOFT_LINK)
        cout << "Target: " << node.target << "\n";
    else
        cout << "Entries: " << node.entries.size() << "\n";
}

void inodesCommand() {
    cout << "Active Inodes:\n";

    for (const auto &item : inodes) {
        cout << "Inode " << item.first << " - ";

        if (item.second.type == REGULAR_FILE)
            cout << "Regular File";
        else if (item.second.type == DIRECTORY)
            cout << "Directory";
        else
            cout << "Symbolic Link";

        cout << "\n";
    }
}


const string SAVE_FILE = "filesystem_data.txt";

void saveFileSystem() {
    ofstream out(SAVE_FILE);
    if (!out) {
        cout << "Warning: could not save filesystem data.\n";
        return;
    }

    out << nextInodeID << ' ' << rootID << ' ' << currentDirectoryID << '\n';
    out << inodes.size() << '\n';

    for (const auto &item : inodes) {
        const Inode &node = item.second;
        out << node.id << ' ' << static_cast<int>(node.type) << ' '
            << node.linkCount << ' ' << node.parentID << ' '
            << quoted(node.content) << ' ' << quoted(node.target) << '\n';
        out << node.entries.size() << '\n';
        for (const auto &entry : node.entries)
            out << quoted(entry.first) << ' ' << entry.second << '\n';
    }
}

bool validateFileSystemData(
    const map<int, Inode> &data,
    int loadedRootID,
    int loadedCurrentID,
    int loadedNextID
) {
    if (!data.count(loadedRootID) || !data.count(loadedCurrentID))
        return false;

    if (data.at(loadedRootID).type != DIRECTORY ||
        data.at(loadedCurrentID).type != DIRECTORY)
        return false;

    if (loadedNextID <= 0)
        return false;

    map<int, int> referenceCount;
    int maxID = 0;

    for (const auto &item : data) {
        int id = item.first;
        const Inode &node = item.second;

        maxID = max(maxID, id);

        if (node.id != id || id <= 0 || node.linkCount < 1)
            return false;

        if (node.type < REGULAR_FILE || node.type > SOFT_LINK)
            return false;

        if (node.type == DIRECTORY) {
            if (!node.entries.count(".") ||
                !node.entries.count(".."))
                return false;

            if (node.entries.at(".") != id ||
                node.entries.at("..") != node.parentID)
                return false;

            if (!data.count(node.parentID))
                return false;

            if (data.at(node.parentID).type != DIRECTORY)
                return false;

            if (id == loadedRootID) {
                if (node.parentID != id)
                    return false;
            }
            else {
                if (node.parentID == id)
                    return false;
            }
        }
        else {
            if (!node.entries.empty())
                return false;

            if (!data.count(node.parentID))
                return false;

            if (data.at(node.parentID).type != DIRECTORY)
                return false;

            if (node.type == SOFT_LINK && node.target.empty())
                return false;
        }
    }

    if (loadedNextID <= maxID)
        return false;

    for (const auto &item : data) {
        int directoryID = item.first;
        const Inode &directory = item.second;

        if (directory.type != DIRECTORY)
            continue;

        for (const auto &entry : directory.entries) {
            const string &name = entry.first;
            int childID = entry.second;

            if (name == ".") {
                if (childID != directoryID)
                    return false;
                continue;
            }

            if (name == "..") {
                if (childID != directory.parentID)
                    return false;
                continue;
            }

            if (!validName(name) || !data.count(childID))
                return false;

            const Inode &child = data.at(childID);

            if (child.parentID != directoryID)
                return false;

            if (child.type == DIRECTORY &&
                child.parentID != directoryID)
                return false;

            referenceCount[childID]++;
        }
    }

    for (const auto &item : data) {
        int id = item.first;
        const Inode &node = item.second;

        int references = referenceCount[id];

        if (node.type == REGULAR_FILE) {
            if (references != node.linkCount)
                return false;
        }
        else if (node.type == SOFT_LINK) {
            if (references != 1 || node.linkCount != 1)
                return false;
        }
        else if (id != loadedRootID) {
            if (references != 1)
                return false;
        }
        else {
            if (references != 0)
                return false;
        }
    }

    return true;
}

bool loadFileSystem() {
    ifstream in(SAVE_FILE);
    if (!in)
        return false;

    int loadedNextID, loadedRootID, loadedCurrentID;
    size_t inodeCount;
    if (!(in >> loadedNextID >> loadedRootID >> loadedCurrentID >> inodeCount))
        return false;

    map<int, Inode> loadedInodes;
    for (size_t i = 0; i < inodeCount; ++i) {
        Inode node;
        int typeValue;
        if (!(in >> node.id >> typeValue >> node.linkCount >> node.parentID
                 >> quoted(node.content) >> quoted(node.target)))
            return false;
        if (typeValue < REGULAR_FILE || typeValue > SOFT_LINK)
            return false;
        node.type = static_cast<FileType>(typeValue);

        size_t entryCount;
        if (!(in >> entryCount))
            return false;
        for (size_t j = 0; j < entryCount; ++j) {
            string entryName;
            int entryID;
            if (!(in >> quoted(entryName) >> entryID))
                return false;
            node.entries[entryName] = entryID;
        }
        loadedInodes[node.id] = node;
    }

    if (!validateFileSystemData(
        loadedInodes,
        loadedRootID,
        loadedCurrentID,
        loadedNextID
    )) {
    return false;
    }
    inodes = loadedInodes;
    nextInodeID = loadedNextID;
    rootID = loadedRootID;
    currentDirectoryID = loadedCurrentID;
    return true;
}


void printTree(int directoryID, const string &prefix) {
    vector<pair<string, int>> children;

    for (const auto &entry : inodes[directoryID].entries) {
        if (entry.first == "." || entry.first == "..")
            continue;
        children.push_back(entry);
    }

    for (size_t i = 0; i < children.size(); ++i) {
        bool isLast = (i == children.size() - 1);
        const string &name = children[i].first;
        int id = children[i].second;

        cout << prefix << (isLast ? "`-- " : "|-- ") << name;

        if (inodes[id].type == DIRECTORY)
            cout << "/";
        else if (inodes[id].type == SOFT_LINK)
            cout << "@";

        cout << "\n";

        if (inodes[id].type == DIRECTORY) {
            printTree(id, prefix + (isLast ? "    " : "|   "));
        }
    }
}

void treeCommand() {
    pwdCommand();
    printTree(currentDirectoryID, "");
}

void helpCommand() {
    cout << "Available commands:\n";
    cout << "  mkdir <name>             Create a directory\n";
    cout << "  touch <name>             Create an empty file\n";
    cout << "  ls                       List current directory entries\n";
    cout << "  tree                     Show directory hierarchy\n";
    cout << "  cd <name>                Change directory; cd / returns to root\n";
    cout << "  pwd                      Show current directory path\n";
    cout << "  write <name> <text>      Write text to a file\n";
    cout << "  cat <name>               Display file contents\n";
    cout << "  rm <name>                Remove a file or symbolic link\n";
    cout << "  rmdir <name>             Remove an empty directory\n";
    cout << "  mv <old> <new>           Rename a file or directory\n";
    cout << "  ln <source> <link>       Create a hard link\n";
    cout << "  ln -s <source> <link>    Create a symbolic link\n";
    cout << "  stat <name>              Show file metadata\n";
    cout << "  inodes                   List active inodes\n";
    cout << "  help                     Show this help message\n";
    cout << "  clear                    Clear the terminal screen\n";
    cout << "  exit                     Save data and close the simulator\n";
    cout << "  rm -r <directory>        Recursively remove a directory\n";
}

void clearCommand() {
    // This project is being run in Windows PowerShell/Command Prompt.
    system("cls");
}

int main() {
    if (loadFileSystem())
        cout << "Filesystem data restored.\n";
    else {
        initializeFileSystem();
        saveFileSystem();
        cout << "New filesystem initialized.\n";
    }

    cout << "File System Simulator\n";
    cout << "Commands:\n";
    cout << "mkdir <name>, touch <name>, ls, tree, cat <name>\n";
    cout << "write <name> <text>, cd <name>, pwd\n";
    cout << "ln <source> <link>, ln -s <source> <link>\n";
    cout << "rm <name>, rmdir <name>, mv <old> <new>\n";
    cout << "stat <name>, inodes, help, clear, exit\n";

    string input, command, name, content, source, linkName;

    while (true) {
        cout << "fs> ";
        getline(cin, input);

        stringstream ss(input);
        ss >> command;

        if (command == "mkdir" || command == "touch" ||
            command == "cat" || command == "cd" ||
            command == "rm" || command == "rmdir" ||
            command == "stat") {

            if (ss >> name) {
                if (command == "mkdir")
                    mkdirCommand(name);
                else if (command == "touch")
                    touchCommand(name);
                else if (command == "cat")
                    catCommand(name);
                else if (command == "cd")
                    cdCommand(name);
                else if (command == "rm") {
                    if (name == "-r") {
                        string target;

                    if (ss >> target)
                        rmCommand(target, true);
                    else
                        cout << "Usage: rm -r <directory>\n";
                    }
                    else {
                        rmCommand(name);
                    }
                }
                else if (command == "rmdir")
                    rmdirCommand(name);
                else if (command == "stat")
                    statCommand(name);
            } else if (command == "cd") {
                cdCommand("/");
            } else {
                cout << "Usage: " << command << " <name>\n";
            }

        } else if (command == "write") {
            if (ss >> name) {
                getline(ss, content);

                if (!content.empty() && content[0] == ' ')
                    content.erase(0, 1);

                if (content.empty())
                    cout << "Usage: write <name> <text>\n";
                else
                    writeCommand(name, content);
            } else {
                cout << "Usage: write <name> <text>\n";
            }

        } else if (command == "ln") {
            string first;
            ss >> first;

            if (first == "-s") {
                if (ss >> source >> linkName)
                    lnCommand(source, linkName, true);
                else
                    cout << "Usage: ln -s <source> <link>\n";
            } else {
                source = first;

                if (!source.empty() && ss >> linkName)
                    lnCommand(source, linkName, false);
                else
                    cout << "Usage: ln <source> <link>\n";
            }

        } else if (command == "mv") {
            string oldName, newName;
            if (ss >> oldName >> newName)
                mvCommand(oldName, newName);
            else
                cout << "Usage: mv <old_name> <new_name>\n";

        } else if (command == "ls") {
            lsCommand();

        } else if (command == "tree") {
            treeCommand();

        } else if (command == "inodes") {
            inodesCommand();

        } else if (command == "pwd") {
            pwdCommand();

        } else if (command == "help") {
            helpCommand();

        } else if (command == "clear") {
            clearCommand();

        } else if (command == "exit") {
            saveFileSystem();
            cout << "Filesystem data saved.\n";
            cout << "File system closed.\n";
            break;

        } else if (!command.empty()) {
            cout << "Unknown command.\n";
        }

        command.clear();
        name.clear();
        content.clear();
        source.clear();
        linkName.clear();
        saveFileSystem();
    }

    return 0;
}