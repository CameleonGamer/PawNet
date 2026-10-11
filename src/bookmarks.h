#ifndef PAWNET_SRC_BOOKMARKS_H_
#define PAWNET_SRC_BOOKMARKS_H_

#include <string>
#include <vector>

struct Bookmark {
  std::string title;
  std::string url;
};

// Bookmarks are kept in bookmarks.json, next to the configuration file. Every
// function here reads or writes the file under one lock, so the window and the
// bookmarks page can use them from different threads.

// bookmarks.json next to the configuration file, or "" when no home directory
// is known.
std::string BookmarksPath();

// All bookmarks, in the order they were added. A missing file gives an empty
// list; a file that cannot be read is reported on stderr and also gives an
// empty list.
std::vector<Bookmark> LoadBookmarks();

bool IsBookmarked(const std::string& url);

// Adds |url| when it is not bookmarked yet, and removes it when it is.
// |bookmarked| receives the new state. Returns false and sets |error| when the
// file cannot be written.
bool ToggleBookmark(const std::string& title,
                    const std::string& url,
                    bool* bookmarked,
                    std::string* error);

// Removes every bookmark with |url|. Returns false and sets |error| on failure.
bool RemoveBookmark(const std::string& url, std::string* error);

#endif  // PAWNET_SRC_BOOKMARKS_H_
