
#include <iostream>
#include <string>
#include <map>
#include <sstream>
#include <vector>
#include <fstream>
#include <iomanip>
#include <cstdlib>
#include<set>
#include <algorithm>
#include <cctype>
#include <conio.h>
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
    int ownerUID;
    int groupGID;
    int permissions;
    vector<int> blockIDs;
};

struct User {
    int uid;
    int gid;
    string username;
};

struct OpenFileDescriptor {
    int fd;
    int inodeID;
    string mode;
    size_t offset;
};

struct DiskBlock {
    int blockID;
    bool isAllocated;
    string data;
};

map<int, Inode> inodes;
map<int, OpenFileDescriptor> fileDescriptors;
int nextFD = 3;
int nextInodeID = 1;
int rootID = 0;
int currentDirectoryID = 0;
const long long FILESYSTEM_CAPACITY = 1024 * 1024;
const int BLOCK_SIZE = 512;
const int TOTAL_DISK_BLOCKS = FILESYSTEM_CAPACITY / BLOCK_SIZE;

vector<DiskBlock> diskBlocks;
int currentUID = 0;
int resolvePath(string path);
void saveFileSystem();
map<string, User> users;
int nextUID = 1;

void setDefaultMetadata(Inode &node, bool isDirectory) {
    node.ownerUID = currentUID;
    node.groupGID = currentUID;

    if (isDirectory)
        node.permissions = 0755;
    else
        node.permissions = 0644;
}

string permissionsToString(int permissions) {
    string result;
    const int masks[] = {0400, 0200, 0100, 0040, 0020, 0010, 0004, 0002, 0001};
    const char symbols[] = {'r', 'w', 'x', 'r', 'w', 'x', 'r', 'w', 'x'};

    for (int i = 0; i < 9; i++) {
        result += (permissions & masks[i]) ? symbols[i] : '-';
    }

    return result;
}

string getUsernameByUID(int uid) {
    for (const auto &item : users) {
        if (item.second.uid == uid)
            return item.second.username;
    }

    return "unknown";
}

bool hasPermission(const Inode &node, int requiredPermission) {
    if (currentUID == 0)
        return true;

    int permissionBits = 0;

    if (currentUID == node.ownerUID) {
        permissionBits = (node.permissions >> 6) & 7;
    }
    else {
        int currentGID = -1;

        for (const auto &item : users) {
            if (item.second.uid == currentUID) {
                currentGID = item.second.gid;
                break;
            }
        }

        if (currentGID == -1)
            return false;

        if (currentGID == node.groupGID)
            permissionBits = (node.permissions >> 3) & 7;
        else
            permissionBits = node.permissions & 7;
    }

    return (permissionBits & requiredPermission) == requiredPermission;
}

bool hasDirectoryWritePermission(int directoryID) {
    if (!inodes.count(directoryID))
        return false;

    const Inode &directory = inodes.at(directoryID);

    if (directory.type != DIRECTORY)
        return false;

    return hasPermission(directory, 3);
}

void chmodCommand(string mode, string path) {
    int newPermissions = 0;

    if (mode.empty()) {
        cout << "Usage: chmod <permissions> <path>\n";
        return;
    }

    for (char c : mode) {
        if (c < '0' || c > '7') {
            cout << "Invalid permission format. Use octal digits (0000-0777).\n";
            return;
        }
    }

    try {
        newPermissions = stoi(mode, nullptr, 8);
    }
    catch (...) {
        cout << "Invalid permission format.\n";
        return;
    }

    if (newPermissions > 0777) {
        cout << "Permissions must be between 0000 and 0777.\n";
        return;
    }

    int inodeID = resolvePath(path);

    if (inodeID == -1 || !inodes.count(inodeID)) {
        cout << "Path not found.\n";
        return;
    }

    Inode &node = inodes[inodeID];

    if (currentUID != 0 && currentUID != node.ownerUID) {
        cout << "Permission denied: only root or the owner can use chmod.\n";
        return;
    }

    node.permissions = newPermissions;

    cout << "Permissions changed successfully to "
         << permissionsToString(newPermissions) << ".\n";

    saveFileSystem();
}

void chownCommand(string username, string path) {
    if (currentUID != 0) {
        cout << "Permission denied: only root can use chown.\n";
        return;
    }

    if (!users.count(username)) {
        cout << "User does not exist.\n";
        return;
    }

    int inodeID = resolvePath(path);

    if (inodeID == -1 || !inodes.count(inodeID)) {
        cout << "Path not found.\n";
        return;
    }

    Inode &node = inodes[inodeID];

    node.ownerUID = users[username].uid;
    node.groupGID = users[username].gid;

    cout << "Ownership changed successfully.\n";
    saveFileSystem();
}

void initializeUsers() {
    User root;
    root.uid = 0;
    root.gid = 0;
    root.username = "root";

    users["root"] = root;
    currentUID = 0;
}

void useraddCommand(string username) {
    if (currentUID != 0) {
        cout << "Permission denied. Only root can add users.\n";
        return;
    }

    if (username.empty() || users.count(username)) {
        cout << "Invalid or existing username.\n";
        return;
    }

    User user;
    user.uid = nextUID;
    user.gid = nextUID;
    user.username = username;

    users[username] = user;
    nextUID++;

    cout << "User created: " << username << "\n";
}

void usersCommand() {
    cout << left
         << setw(15) << "USERNAME"
         << setw(10) << "UID"
         << setw(10) << "GID" << "\n";

    for (const auto &item : users) {
        cout << left
             << setw(15) << item.second.username
             << setw(10) << item.second.uid
             << setw(10) << item.second.gid << "\n";
    }
}

void whoamiCommand() {
    for (const auto &item : users) {
        if (item.second.uid == currentUID) {
            cout << item.second.username << "\n";
            return;
        }
    }

    cout << "Unknown user.\n";
}

void suCommand(string username) {
    if (!users.count(username)) {
        cout << "User does not exist.\n";
        return;
    }

    currentUID = users[username].uid;

    cout << "Switched to user: " << username << "\n";
}

void initializeDisk() {
    diskBlocks.clear();

    for (int i = 0; i < TOTAL_DISK_BLOCKS; i++) {
        DiskBlock block;

        block.blockID = i;
        block.isAllocated = false;
        block.data = "";

        diskBlocks.push_back(block);
    }

    cout << "Simulated disk initialized: "
         << TOTAL_DISK_BLOCKS << " blocks.\n";
}

bool syncFileBlocks(int inodeID) {
    if (inodes.find(inodeID) == inodes.end()) {
        return false;
    }

    Inode& inode = inodes[inodeID];

    if (inode.type != REGULAR_FILE) {
        return false;
    }

    int requiredBlocks = (inode.content.size() + BLOCK_SIZE - 1) / BLOCK_SIZE;
    int currentBlocks = inode.blockIDs.size();

    vector<int> newBlocks;

    if (requiredBlocks > currentBlocks) {
        int additionalBlocks = requiredBlocks - currentBlocks;

        for (const auto& block : diskBlocks) {
            if (!block.isAllocated) {
                newBlocks.push_back(block.blockID);

                if (newBlocks.size() == additionalBlocks) {
                    break;
                }
            }
        }

        if (newBlocks.size() != additionalBlocks) {
            return false;
        }
    }

    for (int blockID : newBlocks) {
        diskBlocks[blockID].isAllocated = true;
        inode.blockIDs.push_back(blockID);
    }

    while (inode.blockIDs.size() > requiredBlocks) {
        int blockID = inode.blockIDs.back();

        diskBlocks[blockID].isAllocated = false;
        diskBlocks[blockID].data.clear();

        inode.blockIDs.pop_back();
    }

    for (int i = 0; i < requiredBlocks; i++) {
        int blockID = inode.blockIDs[i];
        int start = i * BLOCK_SIZE;

        diskBlocks[blockID].data =
            inode.content.substr(start, BLOCK_SIZE);
    }

    return true;
}

bool rebuildDiskFromInodes() {
    size_t totalRequiredBlocks = 0;

    // Calculate the total number of blocks required.
    for (const auto &entry : inodes) {
        const Inode &inode = entry.second;

        if (inode.type == REGULAR_FILE) {
            totalRequiredBlocks +=
                (inode.content.size() + BLOCK_SIZE - 1) / BLOCK_SIZE;
        }
    }

    // Check whether all files can fit on the simulated disk.
    if (totalRequiredBlocks > TOTAL_DISK_BLOCKS) {
        cout << "Error: Saved files exceed available disk blocks.\n";
        return false;
    }

    // Reset the simulated disk.
    for (auto &block : diskBlocks) {
        block.isAllocated = false;
        block.data.clear();
    }

    // Remove old block mappings.
    for (auto &entry : inodes) {
        if (entry.second.type == REGULAR_FILE) {
            entry.second.blockIDs.clear();
        }
    }

    // Reconstruct block allocation for every regular file.
    for (auto &entry : inodes) {
        if (entry.second.type == REGULAR_FILE) {
            if (!syncFileBlocks(entry.first)) {
                cout << "Error: Could not rebuild disk allocation.\n";
                return false;
            }
        }
    }

    cout << "Disk allocation successfully rebuilt.\n";
    return true;
}

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
    root.parentID = root.id;
    setDefaultMetadata(root, true);

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
    if (!hasDirectoryWritePermission(currentDirectoryID)) {
        cout << "Permission denied: directory write and execute access required.\n";
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
    setDefaultMetadata(dir, true);
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

    if (!hasDirectoryWritePermission(currentDirectoryID)) {
        cout << "Permission denied: directory write and execute access required.\n";
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
    setDefaultMetadata(file, false);

    int id = file.id;
    inodes[id] = file;

    if (!syncFileBlocks(id)) {
        inodes.erase(id);
        cout << "Error: Unable to allocate disk blocks.\n";
        return;
    }

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

void lsLongCommand() {
    cout << left
     << setw(20) << "NAME"
     << setw(15) << "TYPE"
     << setw(12) << "INODE"
     << setw(12) << "SIZE"
     << setw(12) << "LINKS"
     << setw(15) << "PERMISSIONS"
     << setw(15) << "OWNER"
     << setw(10) << "GROUP" << "\n";

    for (const auto &entry : inodes[currentDirectoryID].entries) {
        if (entry.first == "." || entry.first == "..")
            continue;

        int id = entry.second;

        if (!inodes.count(id))
            continue;

        const Inode &node = inodes.at(id);

        string type;
        long long size = 0;

        if (node.type == REGULAR_FILE) {
            type = "File";
            size = node.content.size();
        }
        else if (node.type == DIRECTORY) {
            type = "Directory";
        }
        else {
            type = "Symbolic Link";
            size = node.target.size();
        }

    cout << left
     << setw(20) << entry.first
     << setw(15) << type
     << setw(12) << node.id
     << setw(12) << size
     << setw(12) << node.linkCount
     << setw(15) << permissionsToString(node.permissions)
     << setw(15) << getUsernameByUID(node.ownerUID)
     << setw(10) << node.groupGID
     << "\n";
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

void openCommand(string path, string mode) {
    if (mode != "r" && mode != "w" && mode != "rw") {
        cout << "Invalid mode. Use r, w, or rw.\n";
        return;
    }

    int inodeID = resolveFile(path);

    if (inodeID == -1) {
        cout << "File not found or not a regular file.\n";
        return;
    }

    if (mode == "r" && !hasPermission(inodes[inodeID], 4)) {
        cout << "Permission denied: read access required.\n";
        return;
    }

    if (mode == "w" && !hasPermission(inodes[inodeID], 2)) {
        cout << "Permission denied: write access required.\n";
        return;
    }

    if (mode == "rw" &&
        (!hasPermission(inodes[inodeID], 4) ||
         !hasPermission(inodes[inodeID], 2))) {
        cout << "Permission denied: read and write access required.\n";
        return;
    }

    int fd = nextFD++;

    OpenFileDescriptor descriptor;
    descriptor.fd = fd;
    descriptor.inodeID = inodeID;
    descriptor.mode = mode;
    descriptor.offset = 0;

    fileDescriptors[fd] = descriptor;

    cout << "File opened successfully. FD: " << fd << "\n";
}

void readFDCommand(int fd, int count = -1) {
    auto it = fileDescriptors.find(fd);

    if (it == fileDescriptors.end()) {
        cout << "Invalid file descriptor.\n";
        return;
    }

    OpenFileDescriptor &descriptor = it->second;

    if (descriptor.mode != "r" && descriptor.mode != "rw") {
        cout << "Permission denied: descriptor is not open for reading.\n";
        return;
    }

    auto inodeIt = inodes.find(descriptor.inodeID);

    if (inodeIt == inodes.end()) {
        cout << "File no longer exists.\n";
        return;
    }

    string &content = inodeIt->second.content;

    if (descriptor.offset >= content.size()) {
        cout << "End of file.\n";
        return;
    }

    size_t remaining = content.size() - descriptor.offset;
    size_t bytesToRead = remaining;

    if (count >= 0) {
        bytesToRead = min(remaining, static_cast<size_t>(count));
    }

    if (bytesToRead == 0) {
        cout << "Nothing to read.\n";
        return;
    }

    cout << content.substr(descriptor.offset, bytesToRead) << "\n";

    descriptor.offset += bytesToRead;
}

void writeFDCommand(int fd, string text) {
    auto it = fileDescriptors.find(fd);

    if (it == fileDescriptors.end()) {
        cout << "Invalid file descriptor.\n";
        return;
    }

    OpenFileDescriptor &descriptor = it->second;

    if (descriptor.mode != "w" && descriptor.mode != "rw") {
        cout << "Permission denied: descriptor is not open for writing.\n";
        return;
    }

    auto inodeIt = inodes.find(descriptor.inodeID);

    if (inodeIt == inodes.end()) {
        cout << "File no longer exists.\n";
        return;
    }

    string &content = inodeIt->second.content;

    size_t offset = descriptor.offset;
    size_t oldSize = content.size();

    if (text.size() > FILESYSTEM_CAPACITY ||
        offset > FILESYSTEM_CAPACITY - text.size()) {
        cout << "Error: Filesystem capacity exceeded.\n";
        return;
    }

    size_t newEnd = offset + text.size();
    size_t newSize = max(oldSize, newEnd);

    size_t currentUsage = 0;

    for (const auto &entry : inodes) {
        if (entry.second.type == REGULAR_FILE) {
            currentUsage += entry.second.content.size();
        }
    }

    if (currentUsage - oldSize + newSize > FILESYSTEM_CAPACITY) {
        cout << "Error: Filesystem capacity exceeded.\n";
        return;
    }

    string oldContent = content;

    if (offset > content.size()) {
        content.resize(offset, '\0');
    }

    if (newEnd > content.size()) {
        content.resize(newEnd);
    }

    content.replace(offset, text.size(), text);

    if (!syncFileBlocks(descriptor.inodeID)) {
        content = oldContent;
        cout << "Error: Not enough disk blocks available.\n";
        return;
    }

    descriptor.offset += text.size();

    cout << "Written " << text.size() << " bytes successfully.\n";
}

void closeFDCommand(int fd) {
    auto it = fileDescriptors.find(fd);

    if (it == fileDescriptors.end()) {
        cout << "Invalid file descriptor or already closed.\n";
        return;
    }

    fileDescriptors.erase(it);

    cout << "File descriptor " << fd << " closed successfully.\n";
}

string getBaseName(string path) {
    while (path.size() > 1 && path.back() == '/')
        path.pop_back();

    size_t pos = path.find_last_of('/');

    if (pos == string::npos)
        return path;

    return path.substr(pos + 1);
}

bool resolveParentPath(string path, int &parentID, string &name) {
    if (path.empty() || path.back() == '/')
        return false;

    size_t pos = path.find_last_of('/');

    string parentPath;

    if (pos == string::npos)
        parentPath = ".";
    else if (pos == 0)
        parentPath = "/";
    else
        parentPath = path.substr(0, pos);

    name = (pos == string::npos) ? path : path.substr(pos + 1);

    if (!validName(name))
        return false;

    parentID = resolvePath(parentPath);

    return parentID != -1 &&
           inodes[parentID].type == DIRECTORY;
}

void cpCommand(string source, string destination) {
    int sourceID = resolveFile(source);

    if (sourceID == -1) {
        cout << "Source file not found.\n";
        return;
    }
    if (!hasPermission(inodes[sourceID], 4)) {
        cout << "Permission denied: read access required.\n";
        return;
    }

    int destinationParent;
    string destinationName;

    int existingDestination = resolvePath(destination);

    if (existingDestination != -1 &&
        inodes[existingDestination].type == DIRECTORY) {
        destinationParent = existingDestination;
        destinationName = getBaseName(source);
    }
    else if (!resolveParentPath(destination, destinationParent, destinationName)) {
        cout << "Invalid destination path.\n";
        return;
    }
    if (!hasDirectoryWritePermission(destinationParent)) {
        cout << "Permission denied: destination directory write and execute access required.\n";
        return;
    }

    if (inodes[destinationParent].entries.count(destinationName)) {
        cout << "Destination already exists.\n";
        return;
    }

    long long used = 0;

    for (const auto &item : inodes) {
        if (item.second.type == REGULAR_FILE)
            used += item.second.content.size();
    }

    long long required = inodes[sourceID].content.size();

    if (used + required > FILESYSTEM_CAPACITY) {
        cout << "Insufficient filesystem space.\n";
        return;
    }

    Inode file;
    file.id = nextInodeID++;
    file.type = REGULAR_FILE;
    file.content = inodes[sourceID].content;
    file.target = "";
    file.linkCount = 1;
    file.parentID = destinationParent;
    setDefaultMetadata(file, false);

    int newID = file.id;

    inodes[newID] = file;
    inodes[destinationParent].entries[destinationName] = newID;

// Allocate disk blocks for the copied file.
    if (!syncFileBlocks(newID)) {
        inodes[destinationParent].entries.erase(destinationName);
        inodes.erase(newID);

        cout << "Error: Not enough disk blocks available. Copy cancelled.\n";
        return;
    }

    cout << "File copied: " << destinationName << "\n";
}

void catCommand(string name) {
    int id = resolveFile(name);

    if (id == -1) {
        cout << "File not found or not a regular file.\n";
        return;
    }
    if (!hasPermission(inodes[id], 4)) {
        cout << "Permission denied: read access required.\n";
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

    if (!hasPermission(inodes[id], 2)) {
        cout << "Permission denied: write access required.\n";
        return;
    }

    string oldContent = inodes[id].content;
    inodes[id].content = content;

    if (!syncFileBlocks(id)) {
        inodes[id].content = oldContent;
        cout << "Error: Not enough disk blocks available.\n";
        return;
    }

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
    if (!hasPermission(inodes[id], 1)) {
        cout << "Permission denied: execute access required.\n";
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
    if (!hasDirectoryWritePermission(currentDirectoryID)) {
        cout << "Permission denied: directory write and execute access required.\n";
        return;
    }

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
        setDefaultMetadata(link, false);

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


void mvCommand(string source, string destination) {
    int sourceParent;
    string sourceName;

    if (!resolveParentPath(source, sourceParent, sourceName)) {
        cout << "Invalid source path.\n";
        return;
    }
    if (!hasDirectoryWritePermission(sourceParent)) {
        cout << "Permission denied: source directory write and execute access required.\n";
        return;
    }

    auto &sourceEntries = inodes[sourceParent].entries;

    if (!sourceEntries.count(sourceName)) {
        cout << "Source not found.\n";
        return;
    }

    int id = sourceEntries[sourceName];

    if (id == rootID) {
        cout << "Cannot move root directory.\n";
        return;
    }

    int destinationParent;
    string destinationName;

    int existingDestination = resolvePath(destination);

    if (existingDestination != -1 &&
        inodes[existingDestination].type == DIRECTORY) {
        destinationParent = existingDestination;
        destinationName = sourceName;
    }
    else if (!resolveParentPath(destination, destinationParent, destinationName)) {
        cout << "Invalid destination path.\n";
        return;
    }
    if (!hasDirectoryWritePermission(destinationParent)) {
        cout << "Permission denied: destination directory write and execute access required.\n";
        return;
    }

    if (sourceParent == destinationParent &&
        sourceName == destinationName) {
        cout << "Source and destination are identical.\n";
        return;
    }

    if (inodes[destinationParent].entries.count(destinationName)) {
        cout << "Destination already exists.\n";
        return;
    }

    if (inodes[id].type == REGULAR_FILE &&
    inodes[id].linkCount > 1 &&
    sourceParent != destinationParent) {
        cout << "Cannot move a multiply-linked file across directories.\n";
        return;
    }

    if (inodes[id].type == DIRECTORY) {
        int current = destinationParent;

        while (true) {
            if (current == id) {
                cout << "Cannot move a directory into itself or its subdirectory.\n";
                return;
            }

            if (current == rootID)
                break;

            current = inodes[current].parentID;
        }
    }

    sourceEntries.erase(sourceName);
    inodes[destinationParent].entries[destinationName] = id;
    inodes[id].parentID = destinationParent;

    if (inodes[id].type == DIRECTORY)
        inodes[id].entries[".."] = destinationParent;

    cout << "Moved: " << source << " -> " << destination << "\n";
}

void findRecursive(int directoryID, string targetName, string currentPath) {
    if (!inodes.count(directoryID))
        return;

    for (const auto &entry : inodes[directoryID].entries) {
        if (entry.first == "." || entry.first == "..")
            continue;

        string fullPath = currentPath;

        if (fullPath != "/")
            fullPath += "/";

        fullPath += entry.first;

        if (entry.first == targetName)
            cout << fullPath << "\n";

        int id = entry.second;

        if (inodes.count(id) && inodes[id].type == DIRECTORY)
            findRecursive(id, targetName, fullPath);
    }
}

void findCommand(string startPath, string targetName) {
    int startID = resolvePath(startPath);

    if (startID == -1 || inodes[startID].type != DIRECTORY) {
        cout << "Starting directory not found.\n";
        return;
    }

    if (targetName.empty()) {
        cout << "Usage: find <name> or find <path> <name>\n";
        return;
    }

    findRecursive(startID, targetName, startPath);
}

long long calculateDirectoryUsage(int directoryID, set<int> &countedFiles) {
    if (!inodes.count(directoryID))
        return 0;

    long long total = 0;

    for (const auto &entry : inodes[directoryID].entries) {
        if (entry.first == "." || entry.first == "..")
            continue;

        int id = entry.second;

        if (!inodes.count(id))
            continue;

        const Inode &node = inodes.at(id);

        if (node.type == REGULAR_FILE) {
            if (countedFiles.insert(id).second)
                total += node.content.size();
        }
        else if (node.type == DIRECTORY) {
            total += calculateDirectoryUsage(id, countedFiles);
        }
    }

    return total;
}

void duCommand(string path = ".") {
    int id = resolvePath(path);

    if (id == -1) {
        cout << "Path not found.\n";
        return;
    }

    if (inodes[id].type != DIRECTORY) {
        cout << "Not a directory.\n";
        return;
    }

    set<int> countedFiles;
    long long total = calculateDirectoryUsage(id, countedFiles);

    cout << "Directory: " << path << "\n";
    cout << "Storage used: " << total << " bytes\n";
}

void dfCommand() {
    size_t usedBytes = 0;
    int allocatedBlocks = 0;

    for (const auto &entry : inodes) {
        if (entry.second.type == REGULAR_FILE) {
            usedBytes += entry.second.content.size();
        }
    }

    for (const auto &block : diskBlocks) {
        if (block.isAllocated)
            allocatedBlocks++;
    }

    int freeBlocks = TOTAL_DISK_BLOCKS - allocatedBlocks;

    cout << "\n===== FILESYSTEM USAGE =====\n";

    cout << "Filesystem Capacity: "
         << FILESYSTEM_CAPACITY << " bytes\n";

    cout << "Logical Data Used: "
         << usedBytes << " bytes\n";

    cout << "Logical Space Available: "
         << FILESYSTEM_CAPACITY - usedBytes << " bytes\n";

    cout << "Allocated Disk Space: "
         << allocatedBlocks * BLOCK_SIZE << " bytes\n";

    cout << "Free Disk Space: "
         << freeBlocks * BLOCK_SIZE << " bytes\n";

    cout << "Allocated Blocks: "
         << allocatedBlocks << "/" << TOTAL_DISK_BLOCKS << "\n";

    double usage = (usedBytes * 100.0) / FILESYSTEM_CAPACITY;

    cout << "Logical Usage: " << fixed << setprecision(2)
         << usage << "%\n";

    cout << "============================\n";
}

void diskCommand() {
    int allocatedBlocks = 0;
    int freeBlocks = 0;

    for (const auto &block : diskBlocks) {
        if (block.isAllocated)
            allocatedBlocks++;
        else
            freeBlocks++;
    }

    cout << "\n===== SIMULATED DISK STATUS =====\n";

    cout << "Total blocks: " << TOTAL_DISK_BLOCKS << "\n";
    cout << "Block size: " << BLOCK_SIZE << " bytes\n";

    cout << "Allocated blocks: " << allocatedBlocks << "\n";
    cout << "Free blocks: " << freeBlocks << "\n";

    cout << "Allocated capacity: "
         << allocatedBlocks * BLOCK_SIZE << " bytes\n";

    cout << "Free capacity: "
         << freeBlocks * BLOCK_SIZE << " bytes\n";

    cout << "================================\n";
}

void blocksCommand(string filename) {
    int inodeID = resolveFile(filename);

    if (inodeID == -1) {
        cout << "File not found or not a regular file.\n";
        return;
    }

    Inode &inode = inodes[inodeID];

    if (!hasPermission(inode, 4)) {
        cout << "Permission denied: read access required.\n";
        return;
    }

    cout << "\n===== FILE BLOCK INFORMATION =====\n";

    cout << "Filename: " << filename << "\n";
    cout << "Inode ID: " << inodeID << "\n";
    cout << "File size: " << inode.content.size() << " bytes\n";
    cout << "Allocated blocks: " << inode.blockIDs.size() << "\n";

    if (inode.blockIDs.empty()) {
        cout << "No blocks allocated (empty file).\n";
    }

    for (int blockID : inode.blockIDs) {
        if (blockID < 0 || blockID >= TOTAL_DISK_BLOCKS) {
            cout << "Invalid block ID detected: " << blockID << "\n";
            continue;
        }

        const DiskBlock &block = diskBlocks[blockID];

        cout << "\nBlock ID: " << block.blockID << "\n";
        cout << "Status: "
             << (block.isAllocated ? "Allocated" : "Free") << "\n";
        cout << "Stored bytes: " << block.data.size() << "\n";
        cout << "Data: " << block.data << "\n";
    }

    cout << "==================================\n";
}

void checkDiskCommand() {
    set<int> referencedBlocks;
    int errors = 0;

    cout << "\n===== DISK INTEGRITY CHECK =====\n";

    for (const auto &entry : inodes) {
        const Inode &inode = entry.second;

        if (inode.type != REGULAR_FILE)
            continue;

        int expectedBlocks =
            (inode.content.size() + BLOCK_SIZE - 1) / BLOCK_SIZE;

        if (inode.blockIDs.size() != expectedBlocks) {
            cout << "ERROR: Incorrect block count for inode "
                 << inode.id << "\n";
            errors++;
        }

        for (int blockID : inode.blockIDs) {
            if (blockID < 0 || blockID >= TOTAL_DISK_BLOCKS) {
                cout << "ERROR: Invalid block ID " << blockID << "\n";
                errors++;
                continue;
            }

            if (!referencedBlocks.insert(blockID).second) {
                cout << "ERROR: Block " << blockID
                     << " is referenced by multiple inodes.\n";
                errors++;
            }

            if (!diskBlocks[blockID].isAllocated) {
                cout << "ERROR: Block " << blockID
                     << " is referenced but marked free.\n";
                errors++;
            }
        }
    }

    for (const auto &block : diskBlocks) {
        if (block.isAllocated &&
            referencedBlocks.count(block.blockID) == 0) {
            cout << "ERROR: Allocated block " << block.blockID
                 << " has no inode reference.\n";
            errors++;
        }
    }

    if (errors == 0)
        cout << "Disk integrity check passed. No errors found.\n";
    else
        cout << "Disk integrity check found "
             << errors << " error(s).\n";

    cout << "================================\n";
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

            if (inodes[id].linkCount <= 0) {
                for (int blockID : inodes[id].blockIDs) {
                    diskBlocks[blockID].isAllocated = false;
                    diskBlocks[blockID].data.clear();
                }

                inodes.erase(id);
            }
        }
        else {
            inodes.erase(id);
        }

        inodes[directoryID].entries.erase(name);
    }
}

void rmCommand(string name, bool recursive = false) {
    auto &entries = inodes[currentDirectoryID].entries;

    if (!hasDirectoryWritePermission(currentDirectoryID)) {
        cout << "Permission denied: directory write and execute access required.\n";
        return;
    }

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

        if (inodes[id].linkCount <= 0) {
            for (int blockID : inodes[id].blockIDs) {
                diskBlocks[blockID].isAllocated = false;
                diskBlocks[blockID].data.clear();
            }

            inodes.erase(id);
        }
    }
    else {
        inodes.erase(id);
    }

    cout << "Removed: " << name << "\n";
}

void rmdirCommand(string name) {
    auto &entries = inodes[currentDirectoryID].entries;
    if (!hasDirectoryWritePermission(currentDirectoryID)) {
        cout << "Permission denied: directory write and execute access required.\n";
        return;
    }

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
    if (node.type == REGULAR_FILE) {
        cout << "Size: " << node.content.size() << " bytes\n";

        cout << "Allocated Blocks: "
            << node.blockIDs.size() << "\n";

        cout << "Allocated Disk Space: "
            << node.blockIDs.size() * BLOCK_SIZE << " bytes\n";

        cout << "Block IDs: ";

        if (node.blockIDs.empty()) {
            cout << "None";
        } else {
            for (int blockID : node.blockIDs) {
                cout << blockID << " ";
            }
        }

        cout << "\n";
    }
    else if (node.type == DIRECTORY)
        cout << "Directory\n";
    else
        cout << "Symbolic Link\n";

    cout << "Link Count: " << node.linkCount << "\n";
    cout << "Permissions: " << permissionsToString(node.permissions) << "\n";
    cout << "Owner UID: " << node.ownerUID << "\n";
    cout << "Group GID: " << node.groupGID << "\n";

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

    out << "FS3\n";

    out << nextInodeID << ' '
        << rootID << ' '
        << currentDirectoryID << ' '
        << nextUID << ' '
        << currentUID << '\n';

    out << users.size() << '\n';

    for (const auto &item : users) {
        const User &user = item.second;

        out << quoted(user.username) << ' '
            << user.uid << ' '
            << user.gid << '\n';
    }

    out << inodes.size() << '\n';

    for (const auto &item : inodes) {
        const Inode &node = item.second;

        out << node.id << ' '
            << static_cast<int>(node.type) << ' '
            << node.linkCount << ' '
            << node.parentID << ' '
            << node.ownerUID << ' '
            << node.groupGID << ' '
            << node.permissions << ' '
            << quoted(node.content) << ' '
            << quoted(node.target) << '\n';

        out << node.entries.size() << '\n';

        for (const auto &entry : node.entries) {
            out << quoted(entry.first) << ' '
                << entry.second << '\n';
        }
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

    string format;
    if (!(in >> format))
        return false;

    int loadedNextID;
    int loadedRootID;
    int loadedCurrentID;
    int loadedNextUID = 1;
    int loadedCurrentUID = 0;

    map<string, User> loadedUsers;

    if (format == "FS3") {
        if (!(in >> loadedNextID
                 >> loadedRootID
                 >> loadedCurrentID
                 >> loadedNextUID
                 >> loadedCurrentUID))
            return false;

        size_t userCount;

        if (!(in >> userCount))
            return false;

        for (size_t i = 0; i < userCount; i++) {
            User user;

            if (!(in >> quoted(user.username)
                     >> user.uid
                     >> user.gid))
                return false;

            if (user.uid < 0 || loadedUsers.count(user.username))
                return false;

            loadedUsers[user.username] = user;
        }
    }
    else {
        try {
            loadedNextID = stoi(format);
        }
        catch (...) {
            return false;
        }

        if (!(in >> loadedRootID >> loadedCurrentID))
            return false;

        User root;
        root.uid = 0;
        root.gid = 0;
        root.username = "root";

        loadedUsers["root"] = root;
    }

    size_t inodeCount;

    if (!(in >> inodeCount))
        return false;

    map<int, Inode> loadedInodes;

    for (size_t i = 0; i < inodeCount; i++) {
        Inode node;
        int typeValue;

        if (format == "FS3") {
            if (!(in >> node.id
                     >> typeValue
                     >> node.linkCount
                     >> node.parentID
                     >> node.ownerUID
                     >> node.groupGID
                     >> node.permissions
                     >> quoted(node.content)
                     >> quoted(node.target)))
                return false;
        }
        else {
            if (!(in >> node.id
                     >> typeValue
                     >> node.linkCount
                     >> node.parentID
                     >> quoted(node.content)
                     >> quoted(node.target)))
                return false;

            node.ownerUID = 0;
            node.groupGID = 0;

            node.permissions =
                (typeValue == DIRECTORY) ? 0755 : 0644;
        }

        if (typeValue < REGULAR_FILE || typeValue > SOFT_LINK)
            return false;

        node.type = static_cast<FileType>(typeValue);

        size_t entryCount;

        if (!(in >> entryCount))
            return false;

        for (size_t j = 0; j < entryCount; j++) {
            string entryName;
            int entryID;

            if (!(in >> quoted(entryName) >> entryID))
                return false;

            node.entries[entryName] = entryID;
        }

        if (loadedInodes.count(node.id))
            return false;

        loadedInodes[node.id] = node;
    }

    if (loadedUsers.empty() ||
        !loadedUsers.count("root"))
        return false;

    if (!loadedUsers.count(
        [&]() {
            for (const auto &item : loadedUsers) {
                if (item.second.uid == loadedCurrentUID)
                    return item.first;
            }
            return string();
        }()))
        return false;

    int maxUID = 0;

    for (const auto &item : loadedUsers)
        maxUID = max(maxUID, item.second.uid);

    if (loadedNextUID <= maxUID)
        return false;

    for (const auto &item : loadedInodes) {
        const Inode &node = item.second;

        bool ownerExists = false;
        bool groupExists = false;

        for (const auto &userItem : loadedUsers) {
            if (userItem.second.uid == node.ownerUID)
                ownerExists = true;

            if (userItem.second.gid == node.groupGID)
                groupExists = true;
        }

        if (!ownerExists || !groupExists)
            return false;

        if (node.permissions < 0 || node.permissions > 0777)
            return false;
    }

    if (!validateFileSystemData(
        loadedInodes,
        loadedRootID,
        loadedCurrentID,
        loadedNextID))
        return false;

    inodes = loadedInodes;
    users = loadedUsers;

    nextInodeID = loadedNextID;
    rootID = loadedRootID;
    currentDirectoryID = loadedCurrentID;
    nextUID = loadedNextUID;
    currentUID = loadedCurrentUID;

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
    cout << "\n========== FILE SYSTEM COMMANDS ==========\n";

    cout << "\n[DIRECTORY & FILE MANAGEMENT]\n";
    cout << "  mkdir <name>                  Create directory\n";
    cout << "  touch <name>                  Create empty file\n";
    cout << "  ls                            List directory contents\n";
    cout << "  ls -l                         Detailed listing\n";
    cout << "  tree                          Display directory tree\n";
    cout << "  cd <path>                     Change directory\n";
    cout << "  cd /                          Return to root\n";
    cout << "  pwd                           Show current path\n";
    cout << "  cat <path>                    Display file contents\n";
    cout << "  write <file> <text>           Write to a file\n";
    cout << "  rm <file>                     Delete file or link\n";
    cout << "  rm -r <directory>             Recursive deletion\n";
    cout << "  rmdir <directory>             Remove empty directory\n";
    cout << "  cp <source> <destination>     Copy file\n";
    cout << "  mv <source> <destination>     Move or rename\n";

    cout << "\n[LINKS & SEARCH]\n";
    cout << "  ln <source> <link>            Create hard link\n";
    cout << "  ln -s <source> <link>         Create symbolic link\n";
    cout << "  find <name>                   Search current directory\n";
    cout << "  find <path> <name>            Search specified directory\n";

    cout << "\n[METADATA & STORAGE]\n";
    cout << "  stat <path>                   Display inode metadata\n";
    cout << "  inodes                        List active inodes\n";
    cout << "  du [path]                     Show storage usage\n";
    cout << "  df                            Show filesystem capacity\n";
    cout << "  disk                          Show simulated disk status\n";
    cout << "  blocks <file>                 Show allocated file blocks\n";
    cout << "  checkdisk                     Validate disk allocation\n";

    cout << "\n[USERS & PERMISSIONS]\n";
    cout << "  useradd <username>            Create simulated user\n";
    cout << "  su <username>                 Switch user\n";
    cout << "  users                         List users\n";
    cout << "  whoami                        Show current user\n";
    cout << "  chmod <mode> <path>           Change permissions\n";
    cout << "  chown <user> <path>           Change file owner\n";

    cout << "\n[FILE DESCRIPTORS]\n";
    cout << "  open <path> <r|w|rw>          Open file\n";
    cout << "  read <fd> [count]             Read from descriptor\n";
    cout << "  write <fd> <text>             Write using descriptor\n";
    cout << "  close <fd>                    Close descriptor\n";

    cout << "\n[SHELL]\n";
    cout << "  history                       Show session command history\n";
    cout << "  help                          Display this menu\n";
    cout << "  clear                         Clear terminal\n";
    cout << "  exit                          Save and close simulator\n";

    cout << "\n==========================================\n";
}

void clearCommand() {
    // This project is being run in Windows PowerShell/Command Prompt.
    system("cls");
}

string readInputWithAutocomplete() {
    string input;

    vector<string> commands = {
        "mkdir", "touch", "ls", "tree", "cat", "write",
        "cd", "pwd", "ln", "rm", "rmdir", "mv", "cp",
        "find", "du", "df", "stat", "inodes", "help",
        "clear", "exit", "chmod", "chown", "useradd",
        "su", "users", "whoami", "open", "read",
        "close", "disk", "blocks", "checkdisk", "history"
    };

    while (true) {
        int ch = _getch();

        if (ch == 13) {
            cout << "\n";
            break;
        }
        else if (ch == 8) {
            if (!input.empty()) {
                input.pop_back();
                cout << "\b \b";
            }
        }
        else if (ch == 9) {
            if (input.find(' ') == string::npos) {
                vector<string> matches;

                for (const string &cmd : commands) {
                    if (cmd.find(input) == 0) {
                        matches.push_back(cmd);
                    }
                }

                if (matches.size() == 1) {
                    string completion = matches[0].substr(input.length());
                    input += completion;
                    cout << completion;
                }
            }
        }
        else if (ch >= 32 && ch <= 126) {
            input += (char)ch;
            cout << (char)ch;
        }
    }

    return input;
}

int main() {
    initializeUsers();
    initializeDisk();

    if (loadFileSystem()) {
        cout << "Filesystem data restored.\n";

        if (!rebuildDiskFromInodes()) {
            cout << "Warning: Disk allocation reconstruction failed.\n";
        }
    }
    else {
        initializeFileSystem();
        saveFileSystem();
        cout << "New filesystem initialized.\n";
    }

    cout << "\n";
    cout << "============================================\n";
    cout << "       FILE SYSTEM SIMULATOR v1.0\n";
    cout << "============================================\n";
    cout << "  In-Memory Filesystem | Simulated Disk\n";
    cout << "  Capacity : 1 MB\n";
    cout << "  Block Size : 512 Bytes\n";
    cout << "  Total Blocks : 2048\n";
    cout << "--------------------------------------------\n";
    cout << "  Type 'help' to view available commands.\n";
    cout << "  Type 'exit' to save and close.\n";
    cout << "============================================\n\n";

    string input, command, name, content, source, linkName;
    vector<string> commandHistory;

    while (true) {
        string currentPath;

    if (currentDirectoryID == rootID) {
        currentPath = "/";
    } else {
        // Build the current path without printing it.
        int current = currentDirectoryID;
        vector<string> pathParts;

        while (current != rootID) {
            int parent = inodes[current].parentID;
            string directoryName;

            for (const auto &entry : inodes[parent].entries) {
                if (entry.second == current &&
                    entry.first != "." && entry.first != "..") {
                    directoryName = entry.first;
                    break;
                }
            }

            if (directoryName.empty())
                break;

            pathParts.push_back(directoryName);
            current = parent;
        }

        currentPath = "/";
        for (int i = (int)pathParts.size() - 1; i >= 0; i--) {
            currentPath += pathParts[i];
            if (i > 0)
                currentPath += "/";
        }
    }

    cout << "[" << getUsernameByUID(currentUID)
        << ":" << currentPath << "] fs> ";
        input = readInputWithAutocomplete();
        if (!input.empty()) {
            commandHistory.push_back(input);
        }

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
            string firstToken;
            string remainingText;

            if (ss >> firstToken) {
                getline(ss, remainingText);

                if (!remainingText.empty() && remainingText[0] == ' ')
                    remainingText.erase(0, 1);

                bool isFD = !firstToken.empty() &&
                    all_of(firstToken.begin(), firstToken.end(),
                    [](unsigned char c) { return isdigit(c); });

                if (isFD) {
                    writeFDCommand(stoi(firstToken), remainingText);
                } else {
                    writeCommand(firstToken, remainingText);
                }
            } else {
                 cout << "Usage: write <filename|fd> <text>\n";
            }
        }else if (command == "ln") {
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

        }else if (command == "mv" || command == "cp") {
            string first, second;

            if (ss >> first >> second) {
                if (command == "mv")
                    mvCommand(first, second);
                else
                    cpCommand(first, second);
            }
            else {
                cout << "Usage: " << command << " <source> <destination>\n";
            }

        }else if (command == "tree") {
            treeCommand();
        } else if (command == "ls") {
            string option;
            ss >> option;

            if (option == "-l")
                lsLongCommand();
            else
                lsCommand();

        } else if (command == "find") {
            string first, second;

            if (!(ss >> first)) {
                cout << "Usage: find <name> or find <path> <name>\n";
            }
            else if (ss >> second) {
                findCommand(first, second);
            }
            else {
                findCommand(".", first);
            }

        } else if (command == "du") {
            string path;

            if (ss >> path)
                duCommand(path);
            else
                duCommand();

        } else if (command == "df") {
            dfCommand();

        } else if (command == "disk") {
            diskCommand();

        } else if (command == "blocks") {
            string filename;
            if (ss >> filename)
                blocksCommand(filename);
            else
                cout << "Usage: blocks <filename>\n";
        }
        else if (command == "checkdisk") {
            checkDiskCommand();

        } else if (command == "inodes") {
            inodesCommand();

        } else if (command == "pwd") {
            pwdCommand();

        } else if (command == "help") {
            helpCommand();

        }else if (command == "history") {
            for (int i = 0; i < (int)commandHistory.size(); i++) {
                cout << i + 1 << ". " << commandHistory[i] << "\n";
            }
        } else if (command == "clear") {
            clearCommand();

        }
        else if (command == "useradd" || command == "su") {
            string username;

            if (ss >> username) {
                if (command == "useradd")
                    useraddCommand(username);
                else
                    suCommand(username);
            }
            else {
                cout << "Usage: " << command << " <username>\n";
            }

        } else if (command == "users") {
            usersCommand();

        } else if (command == "whoami") {
            whoamiCommand();
        }
        else if (command == "chown") {
            string username, path;

        if (ss >> username >> path)
            chownCommand(username, path);
        else
            cout << "Usage: chown <username> <path>\n";
        }
        else if (command == "open") {
            string path, mode;

            if (ss >> path >> mode)
                openCommand(path, mode);
            else
                cout << "Usage: open <path> <r|w|rw>\n";
        }
        else if (command == "open") {
            string path, mode;

            if (ss >> path >> mode)
                openCommand(path, mode);
            else
                cout << "Usage: open <path> <r|w|rw>\n";
        }
        else if (command == "read") {
            int fd;
            int count = -1;

            if (ss >> fd) {
                ss >> count;
                readFDCommand(fd, count);
            } else {
                cout << "Usage: read <fd> [count]\n";
            }
        }
        else if (command == "close") {
            int fd;

            if (ss >> fd)
            closeFDCommand(fd);
            else
                cout << "Usage: close <fd>\n";
        }
        else if (command == "exit") {
            saveFileSystem();
            cout << "Filesystem data saved.\n";
            cout << "File system closed.\n";
            break;
        }
        else if (command == "chmod") {
            string mode, path;

            if (ss >> mode >> path)
                chmodCommand(mode, path);
            else
                cout << "Usage: chmod <permissions> <path>\n";
        }  else if (!command.empty()) {
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