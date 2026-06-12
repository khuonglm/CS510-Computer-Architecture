#include "MOESI_protocol.h"
#include "../sim/mreq.h"
#include "../sim/sim.h"
#include "../sim/hash_table.h"

extern Simulator *Sim;

/*************************
 * Constructor/Destructor.
 *************************/
MOESI_protocol::MOESI_protocol (Hash_table *my_table, Hash_entry *my_entry)
    : Protocol (my_table, my_entry)
{
    this->state = MOESI_CACHE_I;
}

MOESI_protocol::~MOESI_protocol ()
{    
}

void MOESI_protocol::dump (void)
{
    const char *block_states[10] = {"X","I","IS","IM","S","SM","E","O","OM","M"};
    fprintf (stderr, "MOESI_protocol - state: %s\n", block_states[state]);
}

void MOESI_protocol::process_cache_request (Mreq *request)
{
	switch (state) {
        case MOESI_CACHE_I:  do_cache_I  (request); break;
        case MOESI_CACHE_IS: do_cache_IS (request); break;
        case MOESI_CACHE_IM: do_cache_IM (request); break;
        case MOESI_CACHE_S:  do_cache_S  (request); break;
        case MOESI_CACHE_SM: do_cache_SM (request); break;
        case MOESI_CACHE_E:  do_cache_E  (request); break;
        case MOESI_CACHE_O:  do_cache_O  (request); break;
        case MOESI_CACHE_OM: do_cache_OM (request); break;
        case MOESI_CACHE_M:  do_cache_M  (request); break;
        default:
            fatal_error ("Invalid Cache State for MOESI Protocol\n");
    }
}

void MOESI_protocol::process_snoop_request (Mreq *request)
{
	switch (state) {
        case MOESI_CACHE_I:  do_snoop_I  (request); break;
        case MOESI_CACHE_IS: do_snoop_IS (request); break;
        case MOESI_CACHE_IM: do_snoop_IM (request); break;
        case MOESI_CACHE_S:  do_snoop_S  (request); break;
        case MOESI_CACHE_SM: do_snoop_SM (request); break;
        case MOESI_CACHE_E:  do_snoop_E  (request); break;
        case MOESI_CACHE_O:  do_snoop_O  (request); break;
        case MOESI_CACHE_OM: do_snoop_OM (request); break;
        case MOESI_CACHE_M:  do_snoop_M  (request); break;
        default:
            fatal_error ("Invalid Cache State for MOESI Protocol\n");
    }
}

/*
- All processors work in parallel, therefore, sometimes, there is the case
all caches are in transient states (IM, SM, IS, OM). so we need to snoop
even in transient states, and act accordingly
(IM acts as I, SM acts as S, ...), just make sure to check the source of request.
- When snooping, E/O/M could provide data to other caches. O/S when upgrade
to M state, a miss is needed, and sometimes the cache provide data to itself
(i.e cache 7 - O miss, cache 7 - send data to bus, state = IM,
cache 7 - update state M), but misses is required.
- Only E->M is silent upgrades.
*/

inline void MOESI_protocol::do_cache_I (Mreq *request)
{
    switch (request->msg) {
        case LOAD:
            // Send GETS; may end up in E (exclusive) or S (shared)
            send_GETS(request->addr);
            state = MOESI_CACHE_IS;
            ++Sim->cache_misses;
            break;
        case STORE:
            // Send GETM; will end up in M
            send_GETM(request->addr);
            state = MOESI_CACHE_IM;
            ++Sim->cache_misses;
            break;
        default:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("MOESI: I state: unexpected processor message\n");
    }
}

inline void MOESI_protocol::do_cache_IS (Mreq *request)
{
    // Sent GETS, waiting for DATA — processor must not issue another request.
    switch (request->msg) {
        case LOAD:
        case STORE:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("MOESI: Should only have one outstanding request per processor!\n");
        default:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("MOESI: IS state: unexpected processor message\n");
    }
}
 
inline void MOESI_protocol::do_cache_IM (Mreq *request)
{
    // Sent GETM, waiting for DATA — processor must not issue another request.
    switch (request->msg) {
        case LOAD:
        case STORE:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("MOESI: Should only have one outstanding request per processor!\n");
        default:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("MOESI: IM state: unexpected processor message\n");
    }
}

inline void MOESI_protocol::do_cache_S (Mreq *request)
{
    switch (request->msg) {
        case LOAD:
            // Read hit
            send_DATA_to_proc(request->addr);
            break;
        case STORE:
            // Upgrade miss: need exclusive ownership
            send_GETM(request->addr);
            state = MOESI_CACHE_SM;
            ++Sim->cache_misses;
            break;
        default:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("MOESI: S state: unexpected processor message\n");
    }
}

inline void MOESI_protocol::do_cache_SM (Mreq *request)
{
    // Sent GETM from S, waiting for DATA.
    switch (request->msg) {
        case LOAD:
        case STORE:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("MOESI: Should only have one outstanding request per processor!\n");
        default:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("MOESI: SM state: unexpected processor message\n");
    }
}

inline void MOESI_protocol::do_cache_E (Mreq *request)
{
    switch (request->msg) {
        case LOAD:
            // Read hit
            send_DATA_to_proc(request->addr);
            break;
        case STORE:
            send_DATA_to_proc(request->addr);
            state = MOESI_CACHE_M;
            ++Sim->silent_upgrades;
            break;
        default:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("MOESI: E state: unexpected processor message\n");
    }
}

inline void MOESI_protocol::do_cache_O (Mreq *request)
{
    switch (request->msg) {
        case LOAD:
            // Read hit
            send_DATA_to_proc(request->addr);
            break;
        case STORE:
            send_GETM(request->addr);
            state = MOESI_CACHE_OM;
            ++Sim->cache_misses;
            break;
        default:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("MOESI: E state: unexpected processor message\n");
    }
}

inline void MOESI_protocol::do_cache_OM (Mreq *request)
{
    // Sent GETM from O, waiting for DATA.
    switch (request->msg) {
    case LOAD:
    case STORE:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("MOESI: Should only have one outstanding request per processor!\n");
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("MOESI: OM state: unexpected processor message\n");
    }
}

inline void MOESI_protocol::do_cache_M (Mreq *request)
{
    switch (request->msg) {
        case LOAD:
        case STORE:
            // Read hit
            send_DATA_to_proc(request->addr);
            break;
        default:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("MOESI: M state: unexpected processor message\n");
    }
}

inline void MOESI_protocol::do_snoop_I (Mreq *request)
{
    switch (request->msg) {
        case GETS:
        case GETM:
        case DATA:
            // We have no data; nothing to do.
            break;
        default:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("MOESI: snoop I: unexpected message\n");
    }
}

inline void MOESI_protocol::do_snoop_IS (Mreq *request)
{
    switch (request->msg) {
        case GETS:
        case GETM:
            break;
        case DATA:
            send_DATA_to_proc(request->addr);
            if(get_shared_line()) {
                state = MOESI_CACHE_S;
            } else {
                state = MOESI_CACHE_E;
            }
            break;
        default:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("MOESI: snoop IS: unexpected message\n");
    }
}

inline void MOESI_protocol::do_snoop_IM (Mreq *request)
{
    switch (request->msg) {
        case GETS:
        case GETM:
            break;
        case DATA:
            send_DATA_to_proc(request->addr);
            state = MOESI_CACHE_M;
            break;
        default:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("MOESI: snoop IM: unexpected message\n");
    }
}

inline void MOESI_protocol::do_snoop_S (Mreq *request)
{
    switch (request->msg) {
        case GETS:
            set_shared_line();
            break;
        case GETM:
            state = MOESI_CACHE_I;
            break;
        case DATA:
            break;
        default:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("MOESI: snoop S: unexpected message\n");
    }
}

inline void MOESI_protocol::do_snoop_SM (Mreq *request)
{
    switch (request->msg) {
        case GETS:
            if (request->src_mid != my_table->moduleID) {
                set_shared_line();
            }
            break;
        case GETM:
            break;
        case DATA:
            send_DATA_to_proc(request->addr);
            state = MOESI_CACHE_M;
            break;
        default:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("MOESI: snoop SM: unexpected message\n");
    }
}

inline void MOESI_protocol::do_snoop_E (Mreq *request)
{
    switch (request->msg) {
        case GETS:
            set_shared_line();
            send_DATA_on_bus(request->addr, request->src_mid);
            state = MOESI_CACHE_S;
            break;
        case GETM:
            send_DATA_on_bus(request->addr, request->src_mid);
            state = MOESI_CACHE_I;
            break;
        case DATA:
            fatal_error ("MOESI: E state: should not see DATA — I own the line!\n");
            break;
        default:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("MOESI: snoop E: unexpected message\n");
    }
}

inline void MOESI_protocol::do_snoop_O (Mreq *request)
{
    switch (request->msg) {
        case GETS:
            set_shared_line();
            send_DATA_on_bus(request->addr, request->src_mid);
            break;
        case GETM:
            send_DATA_on_bus(request->addr, request->src_mid);
            state = MOESI_CACHE_I;
            break;
        case DATA:
            fatal_error ("MOESI: O state: should not see DATA — I own the line!\n");
            break;
        default:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("MOESI: snoop O: unexpected message\n");
    }
}

inline void MOESI_protocol::do_snoop_OM (Mreq *request)
{
    switch (request->msg) {
        case GETS:
            if (request->src_mid != my_table->moduleID) {
                set_shared_line();
                send_DATA_on_bus(request->addr, request->src_mid);
            }
            break;
        case GETM:
            send_DATA_on_bus(request->addr, request->src_mid);
            state = MOESI_CACHE_IM;
            break;
        case DATA:
            send_DATA_to_proc(request->addr);
            state = MOESI_CACHE_M;
            break;
        default:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("MOESI: snoop OM: unexpected message\n");
    }
}

inline void MOESI_protocol::do_snoop_M (Mreq *request)
{
    switch (request->msg) {
        case GETS:
            set_shared_line();
            send_DATA_on_bus(request->addr, request->src_mid);
            state = MOESI_CACHE_O;
            break;
        case GETM:
            send_DATA_on_bus(request->addr, request->src_mid);
            state = MOESI_CACHE_I;
            break;
        case DATA:
            fatal_error ("MOESI: M state: should not see DATA — I own the line!\n");
            break;
        default:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("MOESI: snoop M: unexpected message\n");
    }
}


