

#include "libtorrent/config.hpp"
#include "libtorrent/entry.hpp"
#include "libtorrent/session.hpp"
#include "libtorrent/create_torrent.hpp"
#include "libtorrent/bencode.hpp"

#include "condor_common.h"
#include "condor_config.h"
#include "condor_daemon_core.h"
#include "ipv6_hostname.h"
#include "basename.h"

#include "cached_torrent.h"
 
#include <fstream>


using namespace libtorrent;

libtorrent::session* s;

void InitTracker() 
{
  s = new libtorrent::session();
  boost::system::error_code ec;
  s->listen_on(std::make_pair(6881, 6889), ec);
  if (ec)
  {
    dprintf(D_ALWAYS, "failed to open listen socket: %s\n", ec.message().c_str());
    return;
  }
  
  
}


void DownloadTorrent(char* torrent_buf, std::string destination) 
{
  
  add_torrent_params p;
  p.save_path = destination.c_str();
  


  
  
}


void ServeTorrent() 
{
  
}



void MakeTorrent(const std::string directory) 
{
  file_storage fs;
  error_code ec;
  
  // Add the files to the torrent
  add_files(fs, directory.c_str());
  
  create_torrent t(fs);
  std::string hostname = get_local_fqdn();
  t.add_node(std::make_pair(hostname.c_str(), s->listen_port()));
  
  // Hash the piecies of the torrent
  set_piece_hashes(t, condor_dirname(directory.c_str()));
  
  // Save the torrent into the cache directory
  std::string torrent_save = directory;
  torrent_save += ".torrent";
  std::ofstream out(torrent_save.c_str(), std::ios_base::binary);
  bencode(std::ostream_iterator<char>(out), t.generate());
  
  // Read in the torrent into the torrent info
  add_torrent_params p;
  p.save_path = condor_dirname(directory.c_str());
  p.ti = new torrent_info(torrent_save.c_str(), ec);
  if (ec)
  {
    dprintf(D_ALWAYS, "%s\n", ec.message().c_str());
    return;
  }
  
  s->add_torrent(p, ec);
  if (ec)
  {
    dprintf(D_ALWAYS, "%s\n", ec.message().c_str());
    return;
  }

  
  
  
}
