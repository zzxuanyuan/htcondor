
#include "condor_common.h"

#include <algorithm>

#include "condor_debug.h"
#include "MyString.h"
#include "generic_stats.h"
#include "condor_config.h"

#include "ganglia_reporting.h"
#include "ganglia_interaction.h"

// From ganglia.h, which we can't include directly
enum ganglia_slope {
   GANGLIA_SLOPE_ZERO = 0,
   GANGLIA_SLOPE_POSITIVE,
   GANGLIA_SLOPE_NEGATIVE,
   GANGLIA_SLOPE_BOTH,
   GANGLIA_SLOPE_UNSPECIFIED,
   GANGLIA_SLOPE_DERIVATIVE
};

using namespace condor;

GangliaReporting *GangliaReporting::m_instance = NULL;


GangliaReporting::GangliaReporting()
    : m_context(NULL),
      m_send_channels(NULL),
      m_config(NULL),
      m_flags(IF_VERBOSEPUB),
      m_configured(false)
{
}


GangliaReporting::~GangliaReporting()
{}


void
GangliaReporting::Register(StatisticsPool *pool)
{
    if (pool)
    {
        m_pools.push_back(pool);
    }
}


void
GangliaReporting::Publish(void)
{
    if (!m_configured) return;

    dprintf(D_FULLDEBUG, "Starting Ganglia report for %lu statistics pools.\n", m_pools.size());
    std::vector<StatisticsPool*>::const_iterator it;
    for (it=m_pools.begin(); it != m_pools.end(); it++)
    {
        PublishPool(**it);
    }
}


void
GangliaReporting::PublishPool(StatisticsPool& pool)
{
    // Yikes!
    StatisticsPool::pubitem item;
    MyString name;
    pool.pub.startIterations();

    compat_classad::ClassAd ad;
    std::string value_str;
    classad::ClassAdUnParser unparser;

    while (pool.pub.iterate(name,item))
    {
        if (item.units & IS_HISTOGRAM) continue;
        if (item.flags & IF_RECENTPUB) continue;
        if (!(m_flags & IF_DEBUGPUB) && (item.flags & IF_DEBUGPUB)) continue;
        if ((m_flags & IF_PUBKIND) && (item.flags & IF_PUBKIND) && !(m_flags & item.flags & IF_PUBKIND)) continue;
        if ((item.flags & IF_PUBLEVEL) > (m_flags & IF_PUBLEVEL)) continue;
        if (!item.Publish) continue;
        if (!item.pitem) continue;

        int item_flags = (m_flags & IF_NONZERO) ? item.flags : (item.flags & ~IF_NONZERO);

        ad.Clear();
        stats_entry_base * probe = (stats_entry_base *)item.pitem;
        (probe->*(item.Publish))(ad, item.pattr ? item.pattr : name.Value(), item_flags);

        compat_classad::ClassAd::const_iterator it;
        for (it = ad.begin(); it != ad.end(); it++)
        {
            classad::Value val;
            if (!it->second->Evaluate(val)) continue;
            value_str.clear();
            unparser.Unparse(value_str, val);

            std::string type; // string|int8|uint8|int16|uint16|int32|uint32|float|double
            switch (val.GetType())
            {
            case classad::Value::INTEGER_VALUE: // We convert a classad int to a ganglia double as ganglia cannot handle 64-bit ints.
            case classad::Value::REAL_VALUE:
                type = "double";
                break;
            case classad::Value::STRING_VALUE:
                type = "string";
                break;
            default:
                break;
            }
            if (!type.size()) continue;

            if (it->first.substr(0, 6) == "Recent") continue;

            int slope = GANGLIA_SLOPE_BOTH;
            //const char * attr = item.pattr ? item.pattr : name.Value();
            const char * attr = it->first.c_str();
            std::string group = "condor." + pool.GetPoolName();

            int ret = 0;
            if ((ret = ganglia_send(m_context, m_send_channels, group.c_str(), attr, value_str.c_str(), type.c_str(), slope)))
            {
                dprintf(D_FULLDEBUG, "Failed to send ganglia metric (%s.%s) flags=%0x, units=%0x, ret=%d\n", group.c_str(), attr, item.flags, item.units, ret);
            }
        }
    }
}


void
GangliaReporting::Unregister(StatisticsPool *pool)
{
    m_pools.erase(std::remove(m_pools.begin(), m_pools.end(), pool), m_pools.end());
}


void
GangliaReporting::Reconfig()
{
    m_configured = false;
    if (!param_boolean("ENABLE_GANGLIA", true))
    {
        dprintf(D_FULLDEBUG, "Ganglia integration is not enabled.\n");
    }

    std::string conf_location;
    param(conf_location, "GANGLIA_CONFIG", "/etc/ganglia/gmond.conf");
    int fd;
    if ((fd = open(conf_location.c_str(), O_RDONLY)) < 0)
    {
        dprintf(D_ALWAYS, "Cannot open Ganglia configuration file %s.\n", conf_location.c_str());
        return;
    }

    if (ganglia_reconfig(conf_location.c_str(), &m_context, &m_config, &m_send_channels))
    {
        dprintf(D_ALWAYS, "Failed to load Ganglia configuration file %s.\n", conf_location.c_str());
        return;
    }

    m_configured = true;
    dprintf(D_FULLDEBUG, "Ganglia integration is enabled.\n");
}


GangliaReporting &
GangliaReporting::GetInstance()
{
    if (!GangliaReporting::m_instance)
    {
        GangliaReporting::m_instance = new GangliaReporting();
    }
    return *m_instance;
}


