#ifndef PAWNET_SRC_DOWNLOADS_H_
#define PAWNET_SRC_DOWNLOADS_H_

#include <string>

// Folder that downloads are saved to: $HOME/Downloads. It is created on first
// use, and "" is returned when no home directory is known.
std::string DownloadsDirectory();

// A path inside |directory| for a file called |suggested_name|. The name is
// reduced to its last component, and " (1)", " (2)"... is added before the
// extension when the file already exists, so nothing is overwritten.
std::string UniqueDownloadPath(const std::string& directory,
                               const std::string& suggested_name);

#endif  // PAWNET_SRC_DOWNLOADS_H_
