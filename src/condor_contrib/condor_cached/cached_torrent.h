
#ifndef __CACHED_TORRENT_H__
#define __CACHED_TORRENT_H__

#include <list>
#include <string>

// Initialize the tracker and downloader..
void InitTracker();

// Make a torrent
// Returns: string of sha1 hash identifying torrent
std::string MakeTorrent(const std::string directory, const std::string cacheId);

// Adds a torrent from a file
std::string AddTorrentFromFile(const std::string torrent_file, const std::string save_path);

// Handle alerts
void HandleAlerts(std::list<std::string> & completed_torrents, std::list<std::string> & error_torrents);

// Download a torrent
int DownloadTorrent(const std::string magnet_uri, const std::string destination, const std::string originhost);


#endif
