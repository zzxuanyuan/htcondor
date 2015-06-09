
#include "condor_common.h"
#include "condor_config.h"
#include "condor_daemon_core.h"
#include "ipv6_hostname.h"
#include "basename.h"

#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string.hpp>

#include "libtorrent/config.hpp"
#include "libtorrent/entry.hpp"
#include "libtorrent/session.hpp"
#include "libtorrent/create_torrent.hpp"
#include "libtorrent/bencode.hpp"
#include "libtorrent/alert_types.hpp"
#include "libtorrent/alert.hpp"
#include "libtorrent/magnet_uri.hpp"



#include "cached_torrent.h"
#include "cached_server.h"
 
#include <fstream>
#include <deque>
#include <sstream>
#include <iostream>
#include <string>
#include <list>


using namespace libtorrent;

libtorrent::session* s;

void InitTracker() 
{
  s = new libtorrent::session();
  boost::system::error_code ec;
  s->listen_on(std::make_pair(6881, 6889), ec);
  if (ec)
  {
    dprintf(D_ALWAYS | D_FAILURE, "failed to open listen socket: %s\n", ec.message().c_str());
    dprintf(D_ALWAYS | D_FAILURE, "This can happen if ipv6 is not working properly, so continuing as if no error\n");
  }
  
  dprintf(D_FULLDEBUG, "Started libtorrent on port: %i\n", s->listen_port());
  dprintf(D_FULLDEBUG, "Peer Id: %s\n", s->id().to_string().c_str());
  
  s->start_dht();
  dprintf(D_FULLDEBUG, "Started DHT\n");
  
  std::pair<std::string, int> dht_router = std::make_pair("router.bittorrent.com", 6881);
  s->add_dht_router(dht_router);
  
  // Add any predefined nodes to the DHT
  std::string dht_nodes;
  param(dht_nodes, "DHT_NODES");
  std::vector<std::string> nodes;
  boost::split(nodes, dht_nodes, boost::is_any_of(", "));
  for(std::vector<std::string>::iterator it = nodes.begin(); it != nodes.end(); it++) {
    
    dprintf(D_FULLDEBUG, "Adding %s as dht node\n", it->c_str());
    // Split the string with hostname:port
    std::vector<std::string> node_port;
    boost::split(node_port, *it, boost::is_any_of(":"));
    if (node_port.size() > 1)  {
      s->add_dht_node(std::make_pair(node_port[0], atoi(node_port[1].c_str())));
    } else {
      s->add_dht_node(std::make_pair(node_port[0], 6881));
    }
    
  }
  
  
  
  s->start_lsd();
  dprintf(D_FULLDEBUG, "Started LSD\n");
  
  s->set_alert_mask(libtorrent::add_torrent_alert::error_notification | 
                    /* libtorrent::add_torrent_alert::progress_notification | */
                    libtorrent::add_torrent_alert::status_notification);
  
  
  
}


int DownloadTorrent(const std::string magnet_uri, const std::string destination, const std::string originhost) 
{
  
  add_torrent_params p;
  error_code ec;
  parse_magnet_uri(magnet_uri, p, ec);
  p.save_path = destination.c_str();
  dprintf(D_FULLDEBUG, "Bittorrent save path: %s\n", destination.c_str());
  if (ec) {
    dprintf(D_FAILURE | D_ALWAYS, "Failed to parse magnet uri\n");
  }
  
  dprintf(D_FULLDEBUG, "Adding torrent to session\n");
  s->add_torrent(p);

  


  
  
}

bool returnTrue(const torrent_status &ts) {
  return true;
}


 /* 
  * HandleAlerts handles all relevant alerts from the libtorrent thread.
  * 
  *
  */

void HandleAlerts(std::list<std::string> & completed_torrents, std::list<std::string> & error_torrents) 
{
  std::deque<alert*> alerts;

  s->pop_alerts(&alerts);
  
  dprintf(D_FULLDEBUG, "Got %lu alerts from pop_alerts\n", alerts.size());
  
  // Get the alerts
  while(!alerts.empty()) {
    alert* cur_alert = alerts.front();
    switch(cur_alert->type()) {
      case read_piece_alert::alert_type:
      {
        break;
      }
      case block_finished_alert::alert_type:
      {
        std::stringstream os;
        os << ((block_finished_alert*)cur_alert)->ip;
        std::string pid = ((block_finished_alert*)cur_alert)->pid.to_string();
        torrent_status status = ((block_finished_alert*)cur_alert)->handle.status(libtorrent::torrent_handle::query_torrent_file);
        dprintf(D_FULLDEBUG, "Got block of size %i bytes from %s, peer_id = %s\n", status.block_size, os.str().c_str(), pid.c_str());
        break;
      }
      case torrent_finished_alert::alert_type:
      {
        // Get the torrent finished alert, and set the state as committed
        torrent_finished_alert* finished_alert = (torrent_finished_alert*)cur_alert;
        torrent_handle handle = finished_alert->handle;
        
        // Get the magnet URL
        std::string magnet_uri = make_magnet_uri(handle);
        if (magnet_uri.empty()) {
          dprintf(D_FAILURE | D_ALWAYS, "Magnet URI from completed torrent is empty, invalid handle\n");
        }
        
        completed_torrents.push_front(magnet_uri);
        
      }
    }

    dprintf(D_FULLDEBUG, "Got alert from libtorrent: %s\n", cur_alert->what());
    dprintf(D_FULLDEBUG, "Message from alert: %s\n", cur_alert->message().c_str());
    
    libtorrent::add_torrent_alert const* ap = alert_cast<libtorrent::add_torrent_alert>(cur_alert);
    
    if(ap) {
      //dprintf(D_FULLDEBUG, "Alert is of type add_torrent_alert\n");
      
      if(ap->error.value()) {
        dprintf(D_FULLDEBUG, "Got error from add alert: %i\n", ap->error.value());
        dprintf(D_FULLDEBUG, "Error Message: %s\n", ap->error.message().c_str());
      }
      
    }
    
    
    alerts.pop_front();
    delete cur_alert;
  }
  
  if (s->is_dht_running()) {
    dprintf(D_FULLDEBUG, "DHT is running\n");
    session_status status = s->status();
    
    dprintf(D_FULLDEBUG, "Number of torrents tracked by dht: %i\n", status.dht_torrents);
    dprintf(D_FULLDEBUG, "Number of dht_nodes: %i\n", status.dht_nodes);
    dprintf(D_FULLDEBUG, "Number of global nodes: %lli\n", status.dht_global_nodes);
    
    
  }
  
  if (s->is_paused()) {
    dprintf(D_FULLDEBUG, "Session was paused, resuming...\n");
    s->resume();
  }
  
  /*
  // Get the status of all the torrents
  std::vector<torrent_status> statuses;
  s->get_torrent_status(&statuses, returnTrue);
  
  dprintf(D_FULLDEBUG, "Total number of torrents = %lu\n", statuses.size());
  
  for(std::vector<torrent_status>::iterator it = statuses.begin(); it != statuses.end(); it++) {
    
    dprintf(D_FULLDEBUG, "Showing status for torrent: %s\n", it->name.c_str());
    dprintf(D_FULLDEBUG, "State = %i\n", it->state);
    dprintf(D_FULLDEBUG, "Save Path: %s\n", it->save_path.c_str());
    
    torrent_status st = it->handle.status(torrent_handle::query_save_path | torrent_handle::query_name);
    
    dprintf(D_FULLDEBUG, "New Status object name: %s\n", st.name.c_str());
    dprintf(D_FULLDEBUG, "New status save path: %s\n", st.save_path.c_str());
    
    
    
  }
  */
  
}



void ServeTorrent() 
{
  
}



std::string MakeTorrent(const std::string directory, const std::string cacheId) 
{
  file_storage fs;
  error_code ec;
  
  // Add the files to the torrent
  add_files(fs, directory.c_str());
  
  create_torrent t(fs);
  std::string hostname = get_local_fqdn();
  t.add_node(std::make_pair(hostname.c_str(), s->listen_port()));
  
  /* Trackers don't work */
  /*
  t.add_tracker("udp://tracker.publicbt.com:80");
  t.add_tracker("udp://tracker.openbittorrent.com:80");
  t.add_tracker("udp://tracker.ccc.de:80");
  t.add_tracker("udp://tracker.istole.it:80");
  */
  
  // Hash the piecies of the torrent
  set_piece_hashes(t, condor_dirname(directory.c_str()));
  
  // Save the torrent into the cache directory
  std::string torrent_save = directory;
  torrent_save += "/.torrent";
  dprintf(D_FULLDEBUG, "Saving torrent file to %s\n", torrent_save.c_str());
  
  if(t.priv()) {
    dprintf(D_FULLDEBUG, "Torrent is set as private.  Setting as public.\n");
    (t.set_priv)(false);
  }
  
  t.set_comment(cacheId.c_str());
  t.set_creator("HTCondor Cached");
  
  // Ouput the file, and flush it to disk
  std::ofstream out(torrent_save.c_str(), std::ios_base::binary);
  bencode(std::ostream_iterator<char>(out), t.generate());
  out.flush();
  out.close();
  
  return AddTorrentFromFile(torrent_save, directory);
  
}


std::string AddTorrentFromFile(const std::string torrent_file, const std::string save_path) {
  
  error_code ec;
  
  // Read in the torrent into the torrent info
  add_torrent_params p;
  p.save_path = std::string(condor_dirname(save_path.c_str()));
  dprintf(D_FULLDEBUG, "Setting torrent save path to: %s\n", p.save_path.c_str());
  torrent_info* ti = new torrent_info(torrent_file, ec);
  if (ti->is_valid()) {
    dprintf(D_FULLDEBUG, "Torrent is valid\n");
  } else {
    dprintf(D_ALWAYS, "Torrent is invalid\n");
  }
  p.ti = ti;
  if (ec)
  {
    dprintf(D_ALWAYS, "%s\n", ec.message().c_str());
    return "";
  }
  s->add_torrent(p, ec);
  if (ec)
  {
    dprintf(D_ALWAYS, "%s\n", ec.message().c_str());
    return "";
  }
  
  std::string magnet_uri = make_magnet_uri(*ti);
  dprintf(D_FULLDEBUG, "Magnet URI: %s\n", magnet_uri.c_str());
  return magnet_uri;
  //return ti->info_hash().to_string();
  
  
  
}
