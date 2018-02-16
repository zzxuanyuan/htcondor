
void export_collector();
void export_negotiator();
void export_schedd();
void export_dc_tool();
void export_daemon_and_ad_types();
void export_config();
void export_secman();
void export_event_reader();
void export_log_reader();
void enable_classad_extensions();
void enable_deprecation_warnings();
void export_claim();
void export_startd();
void export_query_iterator();

#ifdef WITH_CACHED
  void export_cached();
#endif // WITH_CACHED
#ifdef WITH_CACHEFLOW_MANAGER
  void export_cacheflow_manager();
#endif
#ifdef WITH_STORAGE_OPTIMIZER
  void export_storage_optimizer();
#endif
