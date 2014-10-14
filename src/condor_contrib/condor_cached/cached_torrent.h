
#ifndef __CACHED_TORRENT_H__
#define __CACHED_TORRENT_H__

// Initialize the tracker and downloader..
void InitTracker();

// Make a torrent
// Returns: string of sha1 hash identifying torrent
std::string MakeTorrent(const std::string directory);

// Handle alerts
void HandleAlerts();

// Download a torrent
int DownloadTorrent(const std::string magnet_uri, const std::string destination);


#endif
