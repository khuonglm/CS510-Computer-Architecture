#include "MESI_protocol.h"
#include "../sim/mreq.h"
#include "../sim/sim.h"
#include "../sim/hash_table.h"

extern Simulator *Sim;

/*************************
 * Constructor/Destructor.
 *************************/
MESI_protocol::MESI_protocol (Hash_table *my_table, Hash_entry *my_entry)
    : Protocol (my_table, my_entry)
{
    this->state = MESI_CACHE_I;
}

MESI_protocol::~MESI_protocol ()
{    
}

void MESI_protocol::dump (void)
{
    const char *block_states[8] = {"X","I","IS","IM","S","SM","E","M"};
    fprintf (stderr, "MESI_protocol - state: %s\n", block_states[state]);
}

void MESI_protocol::process_cache_request (Mreq *request)
{
	switch (state) {
        case MESI_CACHE_I:  do_cache_I  (request); break;
        case MESI_CACHE_IS: do_cache_IS (request); break;
        case MESI_CACHE_IM: do_cache_IM (request); break;
        case MESI_CACHE_S:  do_cache_S  (request); break;
        case MESI_CACHE_SM: do_cache_SM (request); break;
        case MESI_CACHE_E:  do_cache_E  (request); break;
        case MESI_CACHE_M:  do_cache_M  (request); break;
        default:
            fatal_error ("Invalid Cache State for MESI Protocol\n");
    }
}

void MESI_protocol::process_snoop_request (Mreq *request)
{
	switch (state) {
        case MESI_CACHE_I:  do_snoop_I  (request); break;
        case MESI_CACHE_IS: do_snoop_IS (request); break;
        case MESI_CACHE_IM: do_snoop_IM (request); break;
        case MESI_CACHE_S:  do_snoop_S  (request); break;
        case MESI_CACHE_SM: do_snoop_SM (request); break;
        case MESI_CACHE_E:  do_snoop_E  (request); break;
        case MESI_CACHE_M:  do_snoop_M  (request); break;
        default:
            fatal_error ("Invalid Cache State for MESI Protocol\n");
    }
}

inline void MESI_protocol::do_cache_I (Mreq *request)
{
    switch (request->msg) {
        case LOAD:
            // Send GETS; might end up in E or S depending on shared line
            send_GETS(request->addr);
            state = MESI_CACHE_IS;
            ++Sim->cache_misses;
            break;
        case STORE:
            // Send GETM; will end up in M
            send_GETM(request->addr);
            state = MESI_CACHE_IM;
            ++Sim->cache_misses;
            break;
        default:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("MESI: I state: unexpected processor message\n");
    }
}

inline void MESI_protocol::do_cache_IS (Mreq *request)
{
    switch (request->msg) {
    case LOAD:
    case STORE:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("MESI: Should only have one outstanding request per processor!\n");
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("MESI: IS state: unexpected processor message\n");
    }
}

inline void MESI_protocol::do_cache_IM (Mreq *request)
{
    switch (request->msg) {
    case LOAD:
    case STORE:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("MESI: Should only have one outstanding request per processor!\n");
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("MESI: IM state: unexpected processor message\n");
    }
}

inline void MESI_protocol::do_cache_S (Mreq *request)
{
    switch (request->msg) {
        case LOAD:
            // Read hit
            send_DATA_to_proc(request->addr);
            break;
        case STORE:
            // Upgrade miss: need exclusive ownership
            send_GETM(request->addr);
            state = MESI_CACHE_SM;
            ++Sim->cache_misses;
            break;
        default:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("MESI: S state: unexpected processor message\n");
    }
}

inline void MESI_protocol::do_cache_SM (Mreq *request)
{
    switch (request->msg) {
    case LOAD:
    case STORE:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("MESI: Should only have one outstanding request per processor!\n");
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("MESI: SM state: unexpected processor message\n");
    }
}

inline void MESI_protocol::do_cache_E (Mreq *request)
{
    switch (request->msg) {
        case LOAD:
            // Read hit
            send_DATA_to_proc(request->addr);
            break;
        case STORE:
            send_DATA_to_proc(request->addr);
            state = MESI_CACHE_M;
            ++Sim->silent_upgrades;
            break;
        default:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("MESI: SM state: unexpected processor message\n");
    }
}

inline void MESI_protocol::do_cache_M (Mreq *request)
{
    switch (request->msg) {
        case LOAD:
        case STORE:
            // Full hit — we own this line exclusively
            send_DATA_to_proc(request->addr);
            break;
        default:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("MESI: M state: unexpected processor message\n");
    }
}

inline void MESI_protocol::do_snoop_I (Mreq *request)
{
    switch (request->msg) {
        case GETS:
        case GETM:
        case DATA:
            break;
        default:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("MESI: snoop I: unexpected message\n");
    }
}

inline void MESI_protocol::do_snoop_IS (Mreq *request)
{
    switch (request->msg) {
    case GETS:
    case GETM:
        // Our own request on the bus — ignore and keep waiting.
        break;
    case DATA:
        // If shared line is NOT set, we are the only requester -> go to E.
        // If shared line IS set, another cache has/had a copy -> go to S.
        send_DATA_to_proc(request->addr);
        if (get_shared_line()) {
            state = MESI_CACHE_S;
        } else {
            state = MESI_CACHE_E;
        }
        break;
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("MESI: snoop IS: unexpected message\n");
    }
}
 
inline void MESI_protocol::do_snoop_IM (Mreq *request)
{
    switch (request->msg) {
    case GETS:
    case GETM:
        // Our own request on the bus - ignore and keep waiting.
        break;
    case DATA:
        send_DATA_to_proc(request->addr);
        state = MESI_CACHE_M;
        break;
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("MESI: snoop IM: unexpected message\n");
    }
}

inline void MESI_protocol::do_snoop_S (Mreq *request)
{
    switch (request->msg) {
        case GETS:
            // Another cache wants a shared copy — we can stay in S.
            // Also set the shared line so the requester knows it gets S not E.
            set_shared_line();
            break;
        case GETM:
            // Another cache wants exclusive ownership — invalidate our copy.
            state = MESI_CACHE_I;
            break;
        case DATA:
            // Data for another cache — not our concern.
            break;
        default:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("MESI: snoop S: unexpected message\n");
    }
}

inline void MESI_protocol::do_snoop_SM (Mreq *request)
{
    switch (request->msg) {
    case GETS:
        if (request->src_mid != my_table->moduleID) {
            set_shared_line();
        }
        break;
    case GETM:
        // just ignore
        break;
    case DATA:
        send_DATA_to_proc(request->addr);
        state = MESI_CACHE_M;
        break;
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("MESI: snoop SM: unexpected message\n");
    }
}

inline void MESI_protocol::do_snoop_E (Mreq *request)
{
    switch (request->msg) {
        case GETS:
            set_shared_line();
            send_DATA_on_bus(request->addr, request->src_mid);
            state = MESI_CACHE_S;
            break;
        case GETM:
            send_DATA_on_bus(request->addr, request->src_mid);
            state = MESI_CACHE_I;
            break;
        case DATA:
            fatal_error ("MESI: E state: should not see DATA — I own the line!\n");
            break;
        default:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("MESI: snoop E: unexpected message\n");
    }
}

inline void MESI_protocol::do_snoop_M (Mreq *request)
{
    switch (request->msg) {
        case GETS:
            set_shared_line();
            send_DATA_on_bus(request->addr, request->src_mid);
            state = MESI_CACHE_S;
            break;
        case GETM:
            send_DATA_on_bus(request->addr, request->src_mid);
            state = MESI_CACHE_I;
            break;
        case DATA:
            fatal_error ("MESI: M state: should not see DATA — I own the line!\n");
            break;
        default:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("MESI: snoop M: unexpected message\n");
    }   
}

