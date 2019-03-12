#include "cache.h"

//Next N Line -----------------------------------------------------------------
#define N_DEGREE 1

//Stride ----------------------------------------------------------------------
#define IP_TRACKER_COUNT 1024
#define PREFETCH_DEGREE 3

class IP_TRACKER
{
  public:
    // the IP we're tracking
    uint64_t ip;

    // the last address accessed by this IP
    uint64_t last_cl_addr;

    // the stride between the last two addresses accessed by this IP
    int64_t last_stride;

    // use LRU to evict old IP trackers
    uint32_t lru;

    IP_TRACKER()
    {
        ip = 0;
        last_cl_addr = 0;
        last_stride = 0;
        lru = 0;
    };
};
IP_TRACKER trackers[IP_TRACKER_COUNT];

void CACHE::l2c_prefetcher_initialize()
{
    cout << "CPU " << cpu << "PC table based composite prefetcher" << endl;
}


//TODO: Have prefetchers return pf_addr to the coordinator
void l2c_prefetcher_initialize_stride()
{
    //cout << "CPU " << cpu << " L2C IP-based stride prefetcher" << endl;
    for (int i = 0; i < IP_TRACKER_COUNT; i++)
        trackers[i].lru = i;
}

void l2c_prefetcher_operate_next(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type)
{
    for (int i = 1; i < PREFETCH_DEGREE + 1; i++)
    {
        uint64_t pf_addr = ((addr >> LOG2_BLOCK_SIZE) + i) << LOG2_BLOCK_SIZE;
        //cout << " pf_cl: " << (pf_addr >> LOG2_BLOCK_SIZE) << " ip: " << ip << " cache_hit: " << +cache_hit << " type: " << +type << endl;
        //FIXME:prefetch_line(ip, addr, pf_addr, FILL_L2);
    }
}

void l2c_prefetcher_operate_stride(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type)
{
    // check for a tracker hit
    uint64_t cl_addr = addr >> LOG2_BLOCK_SIZE;

    int index = -1;
    for (index = 0; index < IP_TRACKER_COUNT; index++)
    {
        if (trackers[index].ip == ip)
            break;
    }

    // this is a new IP that doesn't have a tracker yet, so allocate one
    if (index == IP_TRACKER_COUNT)
    {

        for (index = 0; index < IP_TRACKER_COUNT; index++)
        {
            if (trackers[index].lru == (IP_TRACKER_COUNT - 1))
                break;
        }

        trackers[index].ip = ip;
        trackers[index].last_cl_addr = cl_addr;
        trackers[index].last_stride = 0;

        //cout << "[IP_STRIDE] MISS index: " << index << " lru: " << trackers[index].lru << " ip: " << hex << ip << " cl_addr: " << cl_addr << dec << endl;

        for (int i = 0; i < IP_TRACKER_COUNT; i++)
        {
            if (trackers[i].lru < trackers[index].lru)
                trackers[i].lru++;
        }
        trackers[index].lru = 0;

        return;
    }

    // sanity check
    // at this point we should know a matching tracker index
    if (index == -1)
        assert(0);

    // calculate the stride between the current address and the last address
    // this bit appears overly complicated because we're calculating
    // differences between unsigned address variables
    int64_t stride = 0;
    if (cl_addr > trackers[index].last_cl_addr)
        stride = cl_addr - trackers[index].last_cl_addr;
    else
    {
        stride = trackers[index].last_cl_addr - cl_addr;
        stride *= -1;
    }

    //cout << "[IP_STRIDE] HIT  index: " << index << " lru: " << trackers[index].lru << " ip: " << hex << ip << " cl_addr: " << cl_addr << dec << " stride: " << stride << endl;

    // don't do anything if we somehow saw the same address twice in a row
    if (stride == 0)
        return;

    // only do any prefetching if there's a pattern of seeing the same
    // stride more than once
    if (stride == trackers[index].last_stride)
    {

        // do some prefetching
        for (int i = 0; i < PREFETCH_DEGREE; i++)
        {
            uint64_t pf_address = (cl_addr + (stride * (i + 1))) << LOG2_BLOCK_SIZE;

            // only issue a prefetch if the prefetch address is in the same 4 KB page
            // as the current demand access address
            if ((pf_address >> LOG2_PAGE_SIZE) != (addr >> LOG2_PAGE_SIZE))
                break;

            // check the MSHR occupancy to decide if we're going to prefetch to the L2 or LLC
            if (CACHE::MSHR.occupancy < (MSHR.SIZE >> 1))
            {
            }
                //FIXME: prefetch_line(ip, addr, pf_address, FILL_L2);
            else
            {
            }
                //FIXME: prefetch_line(ip, addr, pf_address, FILL_LLC);
        }
    }

    trackers[index].last_cl_addr = cl_addr;
    trackers[index].last_stride = stride;

    for (int i = 0; i < IP_TRACKER_COUNT; i++)
    {
        if (trackers[i].lru < trackers[index].lru)
            trackers[i].lru++;
    }
    trackers[index].lru = 0;
}