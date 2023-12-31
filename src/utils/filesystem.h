#pragma once

#include <ctime>
#include <string>
#include <vector>

namespace Filesystem
{
    enum ObjCategory {file, directory, other}; // Symlinks can't be detected due to MinGW-w64 `sys/stat.h` limitations.

    struct ObjInfo
    {
        ObjCategory category = ObjCategory::other;
        std::time_t time_modified = 0; // Modification of files in nested directories doesn't affect this time.
    };

    // Throws if the file or directory can't be accessed.
    // If `ok` isn't null, sets `*ok` to `false` instead of throwing.
    ObjInfo GetObjectInfo(const std::string &entry_name, bool *ok = nullptr);

    // Throws if the directory can't be accessed.
    // If `ok` isn't null, sets `*ok` to `false` instead of throwing.
    // The list might be sorted, but we shouldn't rely on that.
    // Expect the list to contain `.` and `..`.
    std::vector<std::string> GetDirectoryContents(const std::string &dir_name, bool *ok = nullptr);

    struct TreeNode
    {
        std::string name; // File name without path. For the root node returned by `GetObjectTree` this is equal to `entry_name`.
        std::string path; // Starts with `entry_name` passed to `GetObjectTree` and ends with `name`.
        ObjInfo info;
        std::time_t time_modified_recursive = 0; // Unlike `info.time_modified` this variable includes modification time for nested objects.
        std::vector<TreeNode> contents;
    };

    // Throws if the specified file or directory can't be accessed.
    // If `ok` isn't null, sets `*ok` to `false` instead of throwing.
    // If some of the nested entries can't accessed, an incomplete tree will be returned. That won't be reported in any way, no exceptions will be thrown
    // Using a negative `max_depth` disables depth limit. But then a circular symlink might cause stack overflow.
    TreeNode GetObjectTree(const std::string &entry_name, int max_depth, bool *ok = nullptr);

    template <typename F> void ForEachObject(const TreeNode &tree, F &&func) // `func` should be `void func(const TreeNode &node)`.
    {
        func(tree);
        for (const auto &elem : tree.contents)
            ForEachObject(elem, func);
    }
}
