#include "MSI_protocol.h"
#include "../sim/mreq.h"
#include "../sim/sim.h"
#include "../sim/hash_table.h"

extern Simulator *Sim;

/*************************
 * Constructor/Destructor.
 *************************/
MSI_protocol::MSI_protocol (Hash_table *my_table, Hash_entry *my_entry)
    : Protocol (my_table, my_entry)
{
    this->state = MSI_CACHE_I;
}

MSI_protocol::~MSI_protocol ()
{
}

void MSI_protocol::dump (void)
{
    const char *block_states[7] = {"X","I","IS","IM","S","SM","M"};
    fprintf (stderr, "MSI_protocol - state: %s\n", block_states[state]);
}

void MSI_protocol::process_cache_request (Mreq *request)
{
	switch (state) {
        case MSI_CACHE_I: do_cache_I(request); break;
        case MSI_CACHE_IS: do_cache_IS(request); break;
        case MSI_CACHE_IM: do_cache_IM(request); break;
        case MSI_CACHE_S: do_cache_S(request); break;
        case MSI_CACHE_SM: do_cache_SM(request); break;
        case MSI_CACHE_M: do_cache_M(request); break;
        default:
            fatal_error ("Invalid Cache State for MSI Protocol\n");
    }
}

void MSI_protocol::process_snoop_request (Mreq *request)
{
	switch (state) {
        case MSI_CACHE_I:  do_snoop_I  (request); break;
        case MSI_CACHE_IS: do_snoop_IS (request); break;
        case MSI_CACHE_IM: do_snoop_IM (request); break;
        case MSI_CACHE_S:  do_snoop_S  (request); break;
        case MSI_CACHE_SM: do_snoop_SM (request); break;
        case MSI_CACHE_M:  do_snoop_M  (request); break;
        default:
            fatal_error ("Invalid Cache State for MSI Protocol\n");
    }
}

inline void MSI_protocol::do_cache_I (Mreq *request)
{
    switch(request->msg) {
        case LOAD:
            send_GETS(request->addr);
            state = MSI_CACHE_IS;
            ++Sim->cache_misses;
            break;
        case STORE:
            send_GETM(request->addr);
            state = MSI_CACHE_IM;
            ++Sim->cache_misses;
            break;
        default:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("MSI: I state: unexpected processor message\n");
    }
}

inline void MSI_protocol::do_cache_IS (Mreq *request)
{
    // We are waiting for DATA. The processor must not
    // issue another request while one is outstanding.
    switch(request->msg) {
        case LOAD:
        case STORE:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error("Should only have one outstanding request per processor!");
        default:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("Client: I state shouldn't see this message\n");
    }
}

inline void MSI_protocol::do_cache_IM (Mreq *request)
{
    // We are waiting for DATA. The processor must not
    // issue another request while one is outstanding.
    switch(request->msg) {
        case LOAD:
        case STORE:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error("Should only have one outstanding request per processor!");
        default:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("Client: I state shouldn't see this message\n");
    }
}

inline void MSI_protocol::do_cache_S (Mreq *request)
{
    switch(request->msg) {
        case LOAD:
            send_DATA_to_proc(request->addr);
            break;
        case STORE:
            send_GETM(request->addr);
            state = MSI_CACHE_SM;
            ++Sim->cache_misses;
            break;
        default:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("Client: I state shouldn't see this message\n");
    }
}

inline void MSI_protocol::do_cache_SM (Mreq *request)
{
    // We are waiting for DATA. The processor must not
    // issue another request while one is outstanding.
    switch(request->msg) {
        case LOAD:
        case STORE:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error("Should only have one outstanding request per processor!");
        default:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("Client: I state shouldn't see this message\n");
    }
}

inline void MSI_protocol::do_cache_M (Mreq *request)
{
    switch(request->msg) {
        case LOAD:
        case STORE:
            send_DATA_to_proc(request->addr);
            break;
        default:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("Client: I state shouldn't see this message\n");
    }
}

inline void MSI_protocol::do_snoop_I (Mreq *request)
{
    switch (request->msg) {
        case GETS:
        case GETM:
        case DATA:
            // We have no data; nothing to do.
            break;
        default:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("MSI: snoop I: unexpected message\n");
    }
}

inline void MSI_protocol::do_snoop_IS (Mreq *request)
{
    switch (request->msg) {
        case GETS:
        case GETM:
            break;
        case DATA:
            send_DATA_to_proc(request->addr);
            state = MSI_CACHE_S;
            break;
        default:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("MSI: snoop I: unexpected message\n");
    }
}

inline void MSI_protocol::do_snoop_IM (Mreq *request)
{
    switch (request->msg) {
        case GETS:
        case GETM:
            break;
        case DATA:
            send_DATA_to_proc(request->addr);
            state = MSI_CACHE_M;
            break;
        default:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("MSI: snoop I: unexpected message\n");
    }
}

inline void MSI_protocol::do_snoop_S (Mreq *request)
{
    switch (request->msg) {
        // although we have data, but memory is responsible for transferring data
        case GETS:
            break;
        case GETM:
            state = MSI_CACHE_I;
            break;
        case DATA:
            break;
        default:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("MSI: snoop I: unexpected message\n");
    }
}

inline void MSI_protocol::do_snoop_SM (Mreq *request)
{
    switch (request->msg) {
        case GETS:
            break;
        case GETM:
            if(request->src_mid != my_table->moduleID) {
                state = MSI_CACHE_IM;
            }
            break;
        case DATA:
            send_DATA_to_proc(request->addr);
            state = MSI_CACHE_M;
            break;
        default:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("MSI: snoop I: unexpected message\n");
    }
}

inline void MSI_protocol::do_snoop_M (Mreq *request)
{
    // M transfer data to other caches,
    // this is cache to cache transfer
    switch (request->msg) {
        case GETS:
            send_DATA_on_bus(request->addr,request->src_mid);
            state = MSI_CACHE_S;
            break;
        case GETM:
            send_DATA_on_bus(request->addr,request->src_mid);
            state = MSI_CACHE_I;
            break;
        case DATA:
            fatal_error ("Should not see data for this line!  I have the line!");
            break;
        default:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("MSI: snoop I: unexpected message\n");
    }
}
