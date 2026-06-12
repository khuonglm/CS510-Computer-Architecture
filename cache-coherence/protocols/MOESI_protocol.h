#ifndef _MOESI_CACHE_H
#define _MOESI_CACHE_H

#include "../sim/types.h"
#include "../sim/enums.h"
#include "../sim/module.h"
#include "../sim/mreq.h"
#include "protocol.h"

/** Cache states.
 *  Stable states: I, S, E, O, M
 *  Transient states:
 *    IS  = sent GETS, waiting for DATA           (I -> S or I -> E)
 *    IM  = sent GETM, waiting for DATA           (I -> M)
 *    SM  = in S, sent GETM, waiting for DATA     (S -> M upgrade)
 *    OM  = in O, sent GETM, waiting for DATA     (O -> M upgrade)
 *
 *  Key difference from MESI: the O (Owned) state.
 *  When an M-state cache sees GETS, instead of flushing to memory and going
 *  to S/I, it supplies data directly (cache-to-cache) and moves to O.
 *  The O-state cache is still responsible for eventually writing back the
 *  dirty line, but other caches can now hold clean S copies simultaneously.
 *  This avoids a writeback penalty on every GETS hit to a dirty line.
 */
typedef enum {
    MOESI_CACHE_I = 1,
    MOESI_CACHE_IS,
    MOESI_CACHE_IM,
    MOESI_CACHE_S,
    MOESI_CACHE_SM,
    MOESI_CACHE_E,
    MOESI_CACHE_O,
    MOESI_CACHE_OM,
    MOESI_CACHE_M
} MOESI_cache_state_t;

class MOESI_protocol : public Protocol {
public:
    MOESI_protocol (Hash_table *my_table, Hash_entry *my_entry);
    ~MOESI_protocol ();

    MOESI_cache_state_t state;
    
    void process_cache_request (Mreq *request);
    void process_snoop_request (Mreq *request);
    void dump (void);

    inline void do_cache_I  (Mreq *request);
    inline void do_cache_IS (Mreq *request);
    inline void do_cache_IM (Mreq *request);
    inline void do_cache_S  (Mreq *request);
    inline void do_cache_SM (Mreq *request);
    inline void do_cache_E  (Mreq *request);
    inline void do_cache_O  (Mreq *request);
    inline void do_cache_OM (Mreq *request);
    inline void do_cache_M  (Mreq *request);

    inline void do_snoop_I  (Mreq *request);
    inline void do_snoop_IS (Mreq *request);
    inline void do_snoop_IM (Mreq *request);
    inline void do_snoop_S  (Mreq *request);
    inline void do_snoop_SM (Mreq *request);
    inline void do_snoop_E  (Mreq *request);
    inline void do_snoop_O  (Mreq *request);
    inline void do_snoop_OM (Mreq *request);
    inline void do_snoop_M  (Mreq *request);
};

#endif // _MOESI_CACHE_H
